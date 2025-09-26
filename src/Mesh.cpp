#include "Mesh.h"
#include "Batch.h"
#include "Renderer.h"

Mesh::Mesh(int vertexCount, int vertexByteSize, void *vertexData, int indexCount, int indexByteSize, void *indexData) {
    assert(currentRenderer && "Current Renderer needs to be assigned!");
    auto &renderer = *currentRenderer;

    MeshMetaData meshData(
        indexCount, renderer.indexBuffer.GetSize() / indexByteSize,
        std::ceil(renderer.vertexBuffer.GetSize() / (float)vertexByteSize)
    );
    index = renderer.meshDataBuffer.Allocate(sizeof(meshData), sizeof(meshData));
    renderer.meshDataBuffer.Write(index, meshData);

    renderer.vertexBuffer.Allocate(vertexCount * vertexByteSize, vertexByteSize);
    renderer.vertexBuffer.Write(index, vertexData, vertexCount * vertexByteSize);
    renderer.indexBuffer.Allocate(indexCount * indexByteSize, indexByteSize);
    renderer.indexBuffer.Write(index, indexData, indexCount * indexByteSize);
    renderer.meshes.push_back(this);
}

Mesh::Mesh() : index(-1U) {}

Mesh::Mesh(Mesh &&o) : Mesh() {
    assert(currentRenderer && "Current Renderer needs to be assigned!");
    auto &renderer = *currentRenderer;
    std::swap(index, o.index);
    if (index != -1U) renderer.meshes[index] = this;
    if (o.index != -1U) renderer.meshes[o.index] = &o;
}

Mesh &Mesh::operator=(Mesh &&o) {
    if (this == &o) return *this;

    assert(currentRenderer && "Current Renderer needs to be assigned!");
    auto &renderer = *currentRenderer;

    std::swap(index, o.index);
    if (index != -1U) renderer.meshes[index] = this;
    if (o.index != -1U) renderer.meshes[o.index] = &o;

    return *this;
}

Mesh::~Mesh() {
    if (index == -1U) return;
    Batch::NotifyMeshDestroy(index);

    assert(currentRenderer && "Current Renderer needs to be assigned!");
    auto &renderer = *currentRenderer;
    renderer.meshDataBuffer.Deallocate(index);
    renderer.vertexBuffer.Deallocate(index);
    renderer.indexBuffer.Deallocate(index);
    renderer.meshes.erase(renderer.meshes.begin() + index);
    for (int i = index; i < renderer.meshes.size(); i++) renderer.meshes[i]->index--;
    index = -1U;
}

Mesh::MeshMetaData Mesh::GetMeshMetaData() const {
    assert(currentRenderer && "Current Renderer needs to be assigned!");
    auto &renderer = *currentRenderer;
    return renderer.meshDataBuffer.Read<MeshMetaData>(index);
}

uint32_t Mesh::GetVertexCount() const {
    assert(currentRenderer && "Current Renderer needs to be assigned!");
    auto &renderer = *currentRenderer;
    return renderer.vertexBuffer.Size(index) / renderer.vertexBuffer.Alignment(index);
}

uint32_t Mesh::GetIndexCount() const { return GetMeshMetaData().indexCount; }

void Mesh::AppendVertices(const void *vertexData, uint32_t byteSize) {
    assert(currentRenderer && "Current Renderer needs to be assigned!");
    auto &renderer = *currentRenderer;
    renderer.vertexBuffer.Reallocate(index, renderer.vertexBuffer.Size(index) + byteSize);
    renderer.vertexBuffer.Write(index, vertexData, byteSize, renderer.vertexBuffer.Size(index) - byteSize);
    for (int i = index + 1; i < renderer.meshes.size(); i++) {
        renderer.meshDataBuffer.Write<uint32_t>(
            i, renderer.vertexBuffer.Offset(i) / renderer.vertexBuffer.Alignment(i),
            offsetof(MeshMetaData, vertexOffset)
        );
    }
}

void Mesh::AppendIndices(const void *indexData, uint32_t byteSize) {
    assert(currentRenderer && "Current Renderer needs to be assigned!");
    auto &renderer = *currentRenderer;
    renderer.indexBuffer.Reallocate(index, renderer.indexBuffer.Size(index) + byteSize);
    renderer.indexBuffer.Write(index, indexData, byteSize, renderer.indexBuffer.Size(index) - byteSize);
    renderer.meshDataBuffer.Write<uint32_t>(
        index, renderer.indexBuffer.Size(index) / renderer.indexBuffer.Alignment(index),
        offsetof(MeshMetaData, indexCount)
    );
    for (int i = index + 1; i < renderer.meshes.size(); i++) {
        renderer.meshDataBuffer.Write<uint32_t>(
            i, renderer.indexBuffer.Offset(i) / renderer.indexBuffer.Alignment(i), offsetof(MeshMetaData, firstIndex)
        );
    }
}
void Mesh::EraseVertices(uint32_t count) {
    assert(currentRenderer && "Current Renderer needs to be assigned!");
    auto &renderer = *currentRenderer;
    renderer.vertexBuffer.Erase(
        index, renderer.vertexBuffer.Alignment(index) * count,
        renderer.vertexBuffer.Size(index) - renderer.vertexBuffer.Alignment(index) * count
    );
    for (int i = index + 1; i < renderer.meshes.size(); i++) {
        renderer.meshDataBuffer.Write<uint32_t>(
            i, renderer.vertexBuffer.Offset(i) / renderer.vertexBuffer.Alignment(i),
            offsetof(MeshMetaData, vertexOffset)
        );
    }
}

void Mesh::EraseIndices(uint32_t count) {
    assert(currentRenderer && "Current Renderer needs to be assigned!");
    auto &renderer = *currentRenderer;
    renderer.indexBuffer.Erase(
        index, renderer.indexBuffer.Alignment(index) * count,
        renderer.indexBuffer.Size(index) - renderer.indexBuffer.Alignment(index) * count
    );
    renderer.meshDataBuffer.Write<uint32_t>(
        index, renderer.indexBuffer.Size(index) / renderer.indexBuffer.Alignment(index),
        offsetof(MeshMetaData, indexCount)
    );
    for (int i = index + 1; i < renderer.meshes.size(); i++) {
        renderer.meshDataBuffer.Write<uint32_t>(
            i, renderer.indexBuffer.Offset(i) / renderer.indexBuffer.Alignment(i), offsetof(MeshMetaData, firstIndex)
        );
    }
}

void Mesh::WriteVertexData(const void *vertexData, uint32_t byteSize, uint32_t byteOffset) {
    assert(currentRenderer && "Current Renderer needs to be assigned!");
    auto &renderer = *currentRenderer;
    renderer.vertexBuffer.Write(index, vertexData, byteSize, byteOffset);
}
void Mesh::WriteIndexData(const void *indexData, uint32_t byteSize, uint32_t byteOffset) {
    assert(currentRenderer && "Current Renderer needs to be assigned!");
    auto &renderer = *currentRenderer;
    renderer.indexBuffer.Write(index, indexData, byteSize, byteOffset);
}
