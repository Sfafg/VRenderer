#include "Mesh.h"
#include "Batch.h"
#include "Renderer.h"

MeshManager::MeshManager(int maxFramesInFlight) {
    vertexBuffer = RenderBuffer(maxFramesInFlight, vg::BufferUsage::VertexBuffer, 0);
    indexBuffer = RenderBuffer(maxFramesInFlight, vg::BufferUsage::IndexBuffer, 0);
    meshDataBuffer = RenderBuffer(maxFramesInFlight, vg::BufferUsage::StorageBuffer, 0);
}

MeshManager::MeshManager() {}

MeshManager::MeshManager(MeshManager &&o) : MeshManager() {
    std::swap(vertexBuffer, o.vertexBuffer);
    std::swap(indexBuffer, o.indexBuffer);
    std::swap(meshDataBuffer, o.meshDataBuffer);
    std::swap(meshes, o.meshes);
}

MeshManager &MeshManager::operator=(MeshManager &&o) {
    if (this == &o) return *this;

    std::swap(vertexBuffer, o.vertexBuffer);
    std::swap(indexBuffer, o.indexBuffer);
    std::swap(meshDataBuffer, o.meshDataBuffer);
    std::swap(meshes, o.meshes);

    return *this;
}

MeshManager::~MeshManager() {}

Mesh::Mesh(
    glm::vec3 boundsMin, glm::vec3 boundsMax, int vertexCount, int vertexByteSize, void *vertexData, int indexCount,
    int indexByteSize, void *indexData
) {
    assert(currentRenderer && "Current Renderer needs to be assigned!");
    auto &meshManager = *currentRenderer->managers.meshManager;

    MeshMetaData meshData(
        boundsMin, 0, boundsMax, 0, indexCount, meshManager.indexBuffer.GetSize() / indexByteSize,
        std::ceil(meshManager.vertexBuffer.GetSize() / (float)vertexByteSize), 0
    );

    index = meshManager.meshDataBuffer.Allocate(sizeof(meshData), sizeof(meshData));
    meshManager.meshDataBuffer.Write(index, meshData);

    meshManager.vertexBuffer.Allocate(vertexCount * vertexByteSize, vertexByteSize);
    meshManager.vertexBuffer.Write(index, vertexData, vertexCount * vertexByteSize);
    meshManager.indexBuffer.Allocate(indexCount * indexByteSize, indexByteSize);
    meshManager.indexBuffer.Write(index, indexData, indexCount * indexByteSize);
    meshManager.meshes.push_back(this);
}

Mesh::Mesh() : index(-1U) {}

Mesh::Mesh(Mesh &&o) : Mesh() {
    assert(currentRenderer && "Current Renderer needs to be assigned!");
    auto &meshManager = *currentRenderer->managers.meshManager;
    std::swap(index, o.index);
    if (index != -1U) meshManager.meshes[index] = this;
    if (o.index != -1U) meshManager.meshes[o.index] = &o;
}

Mesh &Mesh::operator=(Mesh &&o) {
    if (this == &o) return *this;

    assert(currentRenderer && "Current Renderer needs to be assigned!");
    auto &meshManager = *currentRenderer->managers.meshManager;

    std::swap(index, o.index);
    if (index != -1U) meshManager.meshes[index] = this;
    if (o.index != -1U) meshManager.meshes[o.index] = &o;

    return *this;
}

Mesh::~Mesh() {
    if (index == -1U) return;
    assert(currentRenderer && "Current Renderer needs to be assigned!");
    auto &batchManager = *currentRenderer->managers.batchManager;
    batchManager.NotifyMeshDestroy(index);

    assert(currentRenderer && "Current Renderer needs to be assigned!");
    auto &meshManager = *currentRenderer->managers.meshManager;
    meshManager.meshDataBuffer.Deallocate(index);
    meshManager.vertexBuffer.Deallocate(index);
    meshManager.indexBuffer.Deallocate(index);
    meshManager.meshes.erase(meshManager.meshes.begin() + index);
    for (int i = index; i < meshManager.meshes.size(); i++) meshManager.meshes[i]->index--;
    index = -1U;
}

Mesh::MeshMetaData Mesh::GetMeshMetaData() const {
    assert(currentRenderer && "Current Renderer needs to be assigned!");
    auto &meshManager = *currentRenderer->managers.meshManager;
    return meshManager.meshDataBuffer.Read<MeshMetaData>(index);
}

uint32_t Mesh::GetVertexCount() const {
    assert(currentRenderer && "Current Renderer needs to be assigned!");
    auto &meshManager = *currentRenderer->managers.meshManager;
    return meshManager.vertexBuffer.Size(index) / meshManager.vertexBuffer.Alignment(index);
}

uint32_t Mesh::GetIndexCount() const { return GetMeshMetaData().indexCount; }

void Mesh::AppendVertices(const void *vertexData, uint32_t byteSize) {
    assert(currentRenderer && "Current Renderer needs to be assigned!");
    auto &meshManager = *currentRenderer->managers.meshManager;
    meshManager.vertexBuffer.Reallocate(index, meshManager.vertexBuffer.Size(index) + byteSize);
    meshManager.vertexBuffer.Write(index, vertexData, byteSize, meshManager.vertexBuffer.Size(index) - byteSize);
    for (int i = index + 1; i < meshManager.meshes.size(); i++) {
        meshManager.meshDataBuffer.Write<uint32_t>(
            i, meshManager.vertexBuffer.Offset(i) / meshManager.vertexBuffer.Alignment(i),
            offsetof(MeshMetaData, vertexOffset)
        );
    }
}

void Mesh::AppendIndices(const void *indexData, uint32_t byteSize) {
    assert(currentRenderer && "Current Renderer needs to be assigned!");
    auto &meshManager = *currentRenderer->managers.meshManager;
    meshManager.indexBuffer.Reallocate(index, meshManager.indexBuffer.Size(index) + byteSize);
    meshManager.indexBuffer.Write(index, indexData, byteSize, meshManager.indexBuffer.Size(index) - byteSize);
    meshManager.meshDataBuffer.Write<uint32_t>(
        index, meshManager.indexBuffer.Size(index) / meshManager.indexBuffer.Alignment(index),
        offsetof(MeshMetaData, indexCount)
    );
    for (int i = index + 1; i < meshManager.meshes.size(); i++) {
        meshManager.meshDataBuffer.Write<uint32_t>(
            i, meshManager.indexBuffer.Offset(i) / meshManager.indexBuffer.Alignment(i),
            offsetof(MeshMetaData, firstIndex)
        );
    }
}
void Mesh::EraseVertices(uint32_t count) {
    assert(currentRenderer && "Current Renderer needs to be assigned!");
    auto &meshManager = *currentRenderer->managers.meshManager;
    meshManager.vertexBuffer.Erase(
        index, meshManager.vertexBuffer.Alignment(index) * count,
        meshManager.vertexBuffer.Size(index) - meshManager.vertexBuffer.Alignment(index) * count
    );
    for (int i = index + 1; i < meshManager.meshes.size(); i++) {
        meshManager.meshDataBuffer.Write<uint32_t>(
            i, meshManager.vertexBuffer.Offset(i) / meshManager.vertexBuffer.Alignment(i),
            offsetof(MeshMetaData, vertexOffset)
        );
    }
}

void Mesh::EraseIndices(uint32_t count) {
    assert(currentRenderer && "Current Renderer needs to be assigned!");
    auto &meshManager = *currentRenderer->managers.meshManager;
    meshManager.indexBuffer.Erase(
        index, meshManager.indexBuffer.Alignment(index) * count,
        meshManager.indexBuffer.Size(index) - meshManager.indexBuffer.Alignment(index) * count
    );
    meshManager.meshDataBuffer.Write<uint32_t>(
        index, meshManager.indexBuffer.Size(index) / meshManager.indexBuffer.Alignment(index),
        offsetof(MeshMetaData, indexCount)
    );
    for (int i = index + 1; i < meshManager.meshes.size(); i++) {
        meshManager.meshDataBuffer.Write<uint32_t>(
            i, meshManager.indexBuffer.Offset(i) / meshManager.indexBuffer.Alignment(i),
            offsetof(MeshMetaData, firstIndex)
        );
    }
}

void Mesh::WriteVertexData(const void *vertexData, uint32_t byteSize, uint32_t byteOffset) {
    assert(currentRenderer && "Current Renderer needs to be assigned!");
    auto &meshManager = *currentRenderer->managers.meshManager;
    meshManager.vertexBuffer.Write(index, vertexData, byteSize, byteOffset);
}
void Mesh::WriteIndexData(const void *indexData, uint32_t byteSize, uint32_t byteOffset) {
    assert(currentRenderer && "Current Renderer needs to be assigned!");
    auto &meshManager = *currentRenderer->managers.meshManager;
    meshManager.indexBuffer.Write(index, indexData, byteSize, byteOffset);
}
