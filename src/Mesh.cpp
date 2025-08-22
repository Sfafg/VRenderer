#include "Mesh.h"

Mesh::Mesh(int vertexCount, int vertexByteSize, void *vertexData, int indexCount, int indexByteSize, void *indexData) {
    if ((vg::Buffer &)meshDataBuffer == vg::BufferHandle()) {
        meshDataBuffer = RenderBuffer(sizeof(MeshMetaData), vg::BufferUsage::StorageBuffer);
        vertexBuffer = RenderBuffer(vertexCount * vertexByteSize, vg::BufferUsage::VertexBuffer);
        indexBuffer = RenderBuffer(indexCount * indexByteSize, vg::BufferUsage::IndexBuffer);
    }

    MeshMetaData meshData(
        indexCount, indexBuffer.GetSize() / indexByteSize, std::ceil(vertexBuffer.GetSize() / (float)vertexByteSize)
    );
    index = meshDataBuffer.Allocate(sizeof(meshData), sizeof(meshData));
    meshDataBuffer.Write(index, meshData);

    vertexBuffer.Allocate(vertexCount * vertexByteSize, vertexByteSize);
    vertexBuffer.Write(index, vertexData, vertexCount * vertexByteSize);
    indexBuffer.Allocate(indexCount * indexByteSize, indexByteSize);
    indexBuffer.Write(index, indexData, indexCount * indexByteSize);
    meshes.push_back(this);
}

Mesh::Mesh() : index(-1U) {}

Mesh::Mesh(Mesh &&o) {
    std::swap(index, o.index);
    if (index != -1U) meshes[index] = this;
}

Mesh &Mesh::operator=(Mesh &&o) {
    if (this == &o) return *this;

    if (index != -1U && o.index != -1U) std::swap(meshes[index], meshes[o.index]);
    else if (o.index != -1U) meshes[o.index] = this;

    std::swap(index, o.index);

    return *this;
}

Mesh::~Mesh() {
    if (index == -1U) return;

    meshDataBuffer.Deallocate(index);
    vertexBuffer.Deallocate(index);
    indexBuffer.Deallocate(index);
    meshes.erase(meshes.begin() + index);
    for (int i = index; i < meshes.size(); i++) meshes[i]->index--;
    index = -1U;
}

Mesh::MeshMetaData Mesh::GetMeshMetaData() const { return meshDataBuffer.Read<MeshMetaData>(index); }

uint32_t Mesh::GetVertexCount() const { return vertexBuffer.Size(index) / vertexBuffer.Alignment(index); }

uint32_t Mesh::GetIndexCount() const { return GetMeshMetaData().indexCount; }

void Mesh::AppendVertices(const void *vertexData, uint32_t byteSize) {
    vertexBuffer.Reallocate(index, vertexBuffer.Size(index) + byteSize);
    vertexBuffer.Write(index, vertexData, byteSize, vertexBuffer.Size(index) - byteSize);
    for (int i = index + 1; i < meshes.size(); i++) {
        meshDataBuffer.Write<uint32_t>(
            i, vertexBuffer.Offset(i) / vertexBuffer.Alignment(i), offsetof(MeshMetaData, vertexOffset)
        );
    }
}

void Mesh::AppendIndices(const void *indexData, uint32_t byteSize) {
    indexBuffer.Reallocate(index, indexBuffer.Size(index) + byteSize);
    indexBuffer.Write(index, indexData, byteSize, indexBuffer.Size(index) - byteSize);
    meshDataBuffer.Write<uint32_t>(
        index, indexBuffer.Size(index) / indexBuffer.Alignment(index), offsetof(MeshMetaData, indexCount)
    );
    for (int i = index + 1; i < meshes.size(); i++) {
        meshDataBuffer.Write<uint32_t>(
            i, indexBuffer.Offset(i) / indexBuffer.Alignment(i), offsetof(MeshMetaData, firstIndex)
        );
    }
}
void Mesh::EraseVertices(uint32_t count) {
    vertexBuffer.Erase(
        index, vertexBuffer.Alignment(index) * count, vertexBuffer.Size(index) - vertexBuffer.Alignment(index) * count
    );
    for (int i = index + 1; i < meshes.size(); i++) {
        meshDataBuffer.Write<uint32_t>(
            i, vertexBuffer.Offset(i) / vertexBuffer.Alignment(i), offsetof(MeshMetaData, vertexOffset)
        );
    }
}

void Mesh::EraseIndices(uint32_t count) {
    indexBuffer.Erase(
        index, indexBuffer.Alignment(index) * count, indexBuffer.Size(index) - indexBuffer.Alignment(index) * count
    );
    meshDataBuffer.Write<uint32_t>(
        index, indexBuffer.Size(index) / indexBuffer.Alignment(index), offsetof(MeshMetaData, indexCount)
    );
    for (int i = index + 1; i < meshes.size(); i++) {
        meshDataBuffer.Write<uint32_t>(
            i, indexBuffer.Offset(i) / indexBuffer.Alignment(i), offsetof(MeshMetaData, firstIndex)
        );
    }
}

void Mesh::WriteVertexData(const void *vertexData, uint32_t byteSize, uint32_t byteOffset) {
    vertexBuffer.Write(index, vertexData, byteSize, byteOffset);
}
void Mesh::WriteIndexData(const void *indexData, uint32_t byteSize, uint32_t byteOffset) {
    indexBuffer.Write(index, indexData, byteSize, byteOffset);
}

RenderBuffer Mesh::meshDataBuffer;
RenderBuffer Mesh::vertexBuffer;
RenderBuffer Mesh::indexBuffer;
std::vector<Mesh *> Mesh::meshes;
