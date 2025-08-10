#include "Mesh.h"

Mesh::Mesh(int vertexCount, int vertexByteSize, void *vertexData, int indexCount, int indexByteSize, void *indexData)
    : index(meshDataBuffer.GetSize() / sizeof(MeshMetaData)) {

    // MeshDataBuffer growth.
    {
        vg::Buffer staging(sizeof(MeshMetaData), vg::BufferUsage::TransferSrc);
        vg::Allocate(staging, vg::MemoryProperty::HostVisible);

        MeshMetaData &meshData = *(MeshMetaData *)staging.MapMemory();
        meshData.firstIndex = (combinedBuffer.GetSize() - vertexBufferSize) / indexByteSize;
        meshData.indexCount = indexCount;
        meshData.vertexOffset = std::ceil(vertexBufferSize / (float)vertexByteSize);

        vg::Buffer newBuffer(
            meshDataBuffer.GetSize() + staging.GetSize(),
            {vg::BufferUsage::TransferDst, vg::BufferUsage::TransferSrc, vg::BufferUsage::StorageBuffer}
        );
        vg::Allocate(newBuffer, vg::MemoryProperty::DeviceLocal);

        vg::CmdBuffer cmd(vg::currentDevice->GetQueue(0));
        cmd.Begin();
        if (meshDataBuffer.GetSize() != 0)
            cmd.Append(vg::cmd::CopyBuffer(meshDataBuffer, newBuffer, {{meshDataBuffer.GetSize()}}));

        cmd.Append(vg::cmd::CopyBuffer(staging, newBuffer, {{staging.GetSize(), 0, meshDataBuffer.GetSize()}}))
            .End()
            .Submit()
            .Await();

        std::swap(meshDataBuffer, newBuffer);
    }
    // CombinedBuffer growth.
    {
        vg::Buffer staging(vertexByteSize * vertexCount + indexByteSize * indexCount, vg::BufferUsage::TransferSrc);

        vg::Allocate(staging, vg::MemoryProperty::HostVisible);
        memcpy(staging.MapMemory(), vertexData, vertexByteSize * vertexCount);
        memcpy(staging.MapMemory() + vertexByteSize * vertexCount, indexData, indexByteSize * indexCount);

        int vertexPadding = std::ceil(vertexBufferSize / (float)vertexByteSize) * vertexByteSize - vertexBufferSize;

        vg::Buffer newBuffer(
            combinedBuffer.GetSize() + staging.GetSize() + vertexPadding,
            {vg::BufferUsage::TransferDst, vg::BufferUsage::TransferSrc, vg::BufferUsage::VertexBuffer,
             vg::BufferUsage::IndexBuffer}
        );
        vg::Allocate(newBuffer, vg::MemoryProperty::DeviceLocal);

        uint64_t newVertexBufferSize = vertexBufferSize + vertexByteSize * vertexCount + vertexPadding;

        vg::CmdBuffer cmd(vg::currentDevice->GetQueue(0));
        cmd.Begin();
        if (combinedBuffer.GetSize() != 0)
            cmd.Append(vg::cmd::CopyBuffer(
                combinedBuffer, newBuffer,
                {{vertexBufferSize},
                 {combinedBuffer.GetSize() - vertexBufferSize, vertexBufferSize, newVertexBufferSize}}
            ));

        cmd
            .Append(vg::cmd::CopyBuffer(
                staging, newBuffer,
                {{(uint64_t)(vertexByteSize * vertexCount), 0, newVertexBufferSize - (vertexByteSize * vertexCount)},
                 {(uint64_t)(indexByteSize * indexCount), (uint64_t)(vertexByteSize * vertexCount),
                  newBuffer.GetSize() - (indexByteSize * indexCount)}}
            ))
            .End()
            .Submit()
            .Await();

        std::swap(combinedBuffer, newBuffer);
        vertexBufferSize = newVertexBufferSize;
    }
}

Mesh::Mesh() : index(-1) {}

Mesh::~Mesh() {
    if (index >= meshDataBuffer.GetSize() / sizeof(MeshMetaData)) return;

    if (index == 0) {
        combinedBuffer.~Buffer();
        meshDataBuffer.~Buffer();
    }
}

Mesh::MeshMetaData Mesh::GetMeshMetaData() const {
    if (index >= meshDataBuffer.GetSize() / sizeof(MeshMetaData)) return MeshMetaData();

    vg::Buffer read(sizeof(MeshMetaData), vg::BufferUsage::TransferDst);
    vg::Allocate(read, vg::MemoryProperty::HostVisible);

    vg::CmdBuffer(vg::currentDevice->GetQueue(0))
        .Begin()
        .Append(vg::cmd::CopyBuffer(meshDataBuffer, read, {{read.GetSize(), index * sizeof(MeshMetaData), 0}}))
        .End()
        .Submit()
        .Await();

    MeshMetaData meshData = *(MeshMetaData *)read.MapMemory();
    return meshData;
}

vg::Buffer Mesh::combinedBuffer;
vg::Buffer Mesh::meshDataBuffer;
uint64_t Mesh::vertexBufferSize = 0;
