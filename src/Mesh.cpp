#include "Mesh.h"
#include "Batch.h"
#include "Renderer.h"

MeshArray *Mesh::meshArray = nullptr;

MeshArray::MeshArray(int maxFramesInFlight) {
    vertexBuffer = RenderBuffer(maxFramesInFlight, vg::BufferUsage::VertexBuffer, 0);
    indexBuffer = RenderBuffer(maxFramesInFlight, vg::BufferUsage::IndexBuffer, 0);
    meshDataBuffer = RenderBuffer(maxFramesInFlight, vg::BufferUsage::StorageBuffer, 0);
}

MeshArray::MeshArray() {}

MeshArray::MeshArray(MeshArray &&o) : MeshArray() {
    std::swap(vertexBuffer, o.vertexBuffer);
    std::swap(indexBuffer, o.indexBuffer);
    std::swap(meshDataBuffer, o.meshDataBuffer);
    std::swap(meshes, o.meshes);
}

MeshArray &MeshArray::operator=(MeshArray &&o) {
    if (this == &o) return *this;

    std::swap(vertexBuffer, o.vertexBuffer);
    std::swap(indexBuffer, o.indexBuffer);
    std::swap(meshDataBuffer, o.meshDataBuffer);
    std::swap(meshes, o.meshes);

    return *this;
}

MeshArray::~MeshArray() {}

Mesh::Mesh(
    glm::vec3 boundsMin, glm::vec3 boundsMax, int vertexCount, int vertexByteSize, void *vertexData, int indexCount,
    int indexByteSize, void *indexData
) {
    assert(meshArray && "Current meshArray needs to be assigned!");

    MeshMetaData meshData(
        boundsMin, 0, boundsMax, 0, indexCount, meshArray->indexBuffer.GetSize() / indexByteSize,
        std::ceil(meshArray->vertexBuffer.GetSize() / (float)vertexByteSize), 0
    );

    index = meshArray->meshDataBuffer.Allocate(sizeof(meshData), sizeof(meshData));
    meshArray->meshDataBuffer.Write(index, meshData);

    meshArray->vertexBuffer.Allocate(vertexCount * vertexByteSize, vertexByteSize);
    meshArray->vertexBuffer.Write(index, vertexData, vertexCount * vertexByteSize);
    meshArray->indexBuffer.Allocate(indexCount * indexByteSize, indexByteSize);
    meshArray->indexBuffer.Write(index, indexData, indexCount * indexByteSize);
    meshArray->meshes.push_back(this);
}

Mesh::Mesh() : index(-1U) {}

Mesh::Mesh(Mesh &&o) : Mesh() {
    assert(meshArray && "Current meshArray needs to be assigned!");
    std::swap(index, o.index);
    if (index != -1U) meshArray->meshes[index] = this;
    if (o.index != -1U) meshArray->meshes[o.index] = &o;
}

Mesh &Mesh::operator=(Mesh &&o) {
    if (this == &o) return *this;

    assert(meshArray && "Current meshArray needs to be assigned!");

    std::swap(index, o.index);
    if (index != -1U) meshArray->meshes[index] = this;
    if (o.index != -1U) meshArray->meshes[o.index] = &o;

    return *this;
}

Mesh::~Mesh() {
    if (index == -1U) return;
    assert(BatchArray::batchArray && "Current batchArray needs to be assigned!");
    BatchArray::batchArray->NotifyMeshDestroy(index);

    assert(meshArray && "Current meshArray needs to be assigned!");
    meshArray->meshDataBuffer.Deallocate(index);
    meshArray->vertexBuffer.Deallocate(index);
    meshArray->indexBuffer.Deallocate(index);
    meshArray->meshes.erase(meshArray->meshes.begin() + index);
    for (int i = index; i < meshArray->meshes.size(); i++) meshArray->meshes[i]->index--;
    index = -1U;
}

Mesh::MeshMetaData Mesh::GetMeshMetaData() const {
    assert(meshArray && "Current meshArray needs to be assigned!");
    return meshArray->meshDataBuffer.Read<MeshMetaData>(index);
}

uint32_t Mesh::GetVertexCount() const {
    assert(meshArray && "Current meshArray needs to be assigned!");
    return meshArray->vertexBuffer.Size(index) / meshArray->vertexBuffer.Alignment(index);
}

uint32_t Mesh::GetIndexCount() const { return GetMeshMetaData().indexCount; }

void Mesh::AppendVertices(const void *vertexData, uint32_t byteSize) {
    assert(meshArray && "Current meshArray needs to be assigned!");
    meshArray->vertexBuffer.Reallocate(index, meshArray->vertexBuffer.Size(index) + byteSize);
    meshArray->vertexBuffer.Write(index, vertexData, byteSize, meshArray->vertexBuffer.Size(index) - byteSize);
    for (int i = index + 1; i < meshArray->meshes.size(); i++) {
        meshArray->meshDataBuffer.Write<uint32_t>(
            i, meshArray->vertexBuffer.Offset(i) / meshArray->vertexBuffer.Alignment(i),
            offsetof(MeshMetaData, vertexOffset)
        );
    }
}

void Mesh::AppendIndices(const void *indexData, uint32_t byteSize) {
    assert(meshArray && "Current meshArray needs to be assigned!");
    meshArray->indexBuffer.Reallocate(index, meshArray->indexBuffer.Size(index) + byteSize);
    meshArray->indexBuffer.Write(index, indexData, byteSize, meshArray->indexBuffer.Size(index) - byteSize);
    meshArray->meshDataBuffer.Write<uint32_t>(
        index, meshArray->indexBuffer.Size(index) / meshArray->indexBuffer.Alignment(index),
        offsetof(MeshMetaData, indexCount)
    );
    for (int i = index + 1; i < meshArray->meshes.size(); i++) {
        meshArray->meshDataBuffer.Write<uint32_t>(
            i, meshArray->indexBuffer.Offset(i) / meshArray->indexBuffer.Alignment(i),
            offsetof(MeshMetaData, firstIndex)
        );
    }
}
void Mesh::EraseVertices(uint32_t count) {
    assert(meshArray && "Current meshArray needs to be assigned!");
    meshArray->vertexBuffer.Erase(
        index, meshArray->vertexBuffer.Alignment(index) * count,
        meshArray->vertexBuffer.Size(index) - meshArray->vertexBuffer.Alignment(index) * count
    );
    for (int i = index + 1; i < meshArray->meshes.size(); i++) {
        meshArray->meshDataBuffer.Write<uint32_t>(
            i, meshArray->vertexBuffer.Offset(i) / meshArray->vertexBuffer.Alignment(i),
            offsetof(MeshMetaData, vertexOffset)
        );
    }
}

void Mesh::EraseIndices(uint32_t count) {
    assert(meshArray && "Current meshArray needs to be assigned!");
    meshArray->indexBuffer.Erase(
        index, meshArray->indexBuffer.Alignment(index) * count,
        meshArray->indexBuffer.Size(index) - meshArray->indexBuffer.Alignment(index) * count
    );
    meshArray->meshDataBuffer.Write<uint32_t>(
        index, meshArray->indexBuffer.Size(index) / meshArray->indexBuffer.Alignment(index),
        offsetof(MeshMetaData, indexCount)
    );
    for (int i = index + 1; i < meshArray->meshes.size(); i++) {
        meshArray->meshDataBuffer.Write<uint32_t>(
            i, meshArray->indexBuffer.Offset(i) / meshArray->indexBuffer.Alignment(i),
            offsetof(MeshMetaData, firstIndex)
        );
    }
}

void Mesh::WriteVertexData(const void *vertexData, uint32_t byteSize, uint32_t byteOffset) {
    assert(meshArray && "Current meshArray needs to be assigned!");
    meshArray->vertexBuffer.Write(index, vertexData, byteSize, byteOffset);
}
void Mesh::WriteIndexData(const void *indexData, uint32_t byteSize, uint32_t byteOffset) {
    assert(meshArray && "Current meshArray needs to be assigned!");
    meshArray->indexBuffer.Write(index, indexData, byteSize, byteOffset);
}
