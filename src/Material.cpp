#include "Material.h"
#include "Renderer.h"
#include <cassert>

Material::Material(vg::Subpass &&subpass, vg::SubpassDependency &&dependecy, const void *materialData, int byteSize)
    : index(subpasses.size()), variant(0) {
    // Book keeping.
    subpasses.emplace_back(std::move(subpass));
    dependecies.emplace_back(std::move(dependecy));

    RenderBuffer::Region region;
    if (materialData) {
        materialBuffer.MoveRegionVariable(materialBuffer.Allocate(byteSize, byteSize), region);
        materialBuffer.Write(region, materialBuffer, byteSize);
    }
    materialDataRegions.emplace_back(std::move(region));

    // Material::materialDataSize.push_back(byteSize);
    // Material::materialDataOffset.push_back(std::ceil(materialBuffer.GetSize() / (float)byteSize));
    // Material::materialVariants.push_back(1);
    // if (materialData) {
    //     // Write data to staging buffer.
    //     vg::Buffer staging(byteSize, vg::BufferUsage::TransferSrc);
    //     vg::Allocate(staging, vg::MemoryProperty::HostVisible);
    //     memcpy(staging.MapMemory(), materialData, byteSize);

    //     // Allocate new, bigger buffer.
    //     vg::Buffer newBuffer(
    //         (materialDataOffset[index] + 1) * materialDataSize[index],
    //         {vg::BufferUsage::TransferDst, vg::BufferUsage::TransferSrc, vg::BufferUsage::StorageBuffer}
    //     );
    //     vg::Allocate(newBuffer, vg::MemoryProperty::DeviceLocal);

    //     // Copy old and new data to the new buffer.
    //     vg::CmdBuffer cmd(vg::currentDevice->GetQueue(0));
    //     cmd.Begin();
    //     if (materialBuffer.GetSize() != 0)
    //         cmd.Append(vg::cmd::CopyBuffer(materialBuffer, newBuffer, {{materialBuffer.GetSize()}}));

    //     cmd.Append(vg::cmd::CopyBuffer(
    //                    staging, newBuffer,
    //                    {{staging.GetSize(), 0, (uint64_t)materialDataOffset[index] * materialDataSize[index]}}
    //                ))
    //         .End()
    //         .Submit()
    //         .Await();

    //     // Swap buffers and recreate renderpasses.
    //     std::swap(materialBuffer, newBuffer);
    // }
    Renderer::RecreateRenderpass();
}

Material::Material(
    const char *vertexShaderPath, const char *fragmentShaderPath, vg::VertexLayout &&vertexInput,
    MaterialCreateInfo &&createInfo, vg::SubpassDependency &&dependency, const void *materialData, int dataSize
)
    : Material(
          vertexShaderPath, fragmentShaderPath, std::move(vertexInput),
          vg::InputAssembly(createInfo.primitive, createInfo.primitiveRestart), std::move(createInfo.viewportState),
          vg::Rasterizer(
              createInfo.depthClamp, createInfo.discard, createInfo.polygonMode, createInfo.cullMode,
              createInfo.frontFace, createInfo.depthBias, createInfo.lineWidth
          ),
          createInfo.stencilTestEnable
              ? vg::DepthStencil(
                    createInfo.depthTestEnable, createInfo.depthWriteEnable, createInfo.depthCompareOp,
                    createInfo.front, createInfo.back, createInfo.depthBoundsTestEnable, createInfo.minDepthBounds,
                    createInfo.maxDepthBounds
                )
              : vg::DepthStencil(
                    createInfo.depthTestEnable, createInfo.depthWriteEnable, createInfo.depthCompareOp,
                    createInfo.depthBoundsTestEnable, createInfo.minDepthBounds, createInfo.maxDepthBounds
                ),
          vg::ColorBlending(
              createInfo.enableLogicOp, createInfo.logicOp, createInfo.blendConsts, createInfo.attachments
          ),
          createInfo.dynamicState, createInfo.colorAttachments, 0, std::move(dependency), materialData, dataSize
      ) {}

Material::Material(
    const char *vertexShaderPath, const char *fragmentShaderPath, vg::VertexLayout &&vertexInput,
    vg::InputAssembly &&inputAssembly, vg::ViewportState &&viewportState, vg::Rasterizer &&rasterizer,
    vg::DepthStencil &&depthStencil, vg::ColorBlending &&colorBlending,
    const std::vector<vg::DynamicState> &dynamicState, const std::vector<vg::AttachmentReference> &colorAttachments,
    uint32_t childrenCount, vg::SubpassDependency &&dependecy, const void *materialData, int dataSize
)
    : Material(
          vg::Subpass(
              vg::GraphicsPipeline(
                  0,
                  Vector{
                      vg::Shader(vg::ShaderStage::Vertex, vertexShaderPath),
                      vg::Shader(vg::ShaderStage::Fragment, fragmentShaderPath)},
                  vertexInput, inputAssembly, vg::Tesselation(), viewportState, rasterizer, vg::Multisampling(),
                  depthStencil, colorBlending, dynamicState, childrenCount
              ),
              {}, colorAttachments, {}, {}, {}
          ),
          std::move(dependecy), materialData, dataSize
      ) {}

Material::Material(Material *material, const void *materialData, int byteSize) : variant(index.Size() / byteSize + 1) {
    assert(byteSize == material->index.Size());
    index.index = material->index.index;
    materialBuffer.Reallocate(index, index.Size() + byteSize);
    materialBuffer.Write(index, materialData, byteSize, variant * index.Size());

    // // Write data to a staging buffer.
    // vg::Buffer staging(byteSize, vg::BufferUsage::TransferSrc);
    // vg::Allocate(staging, vg::MemoryProperty::HostVisible);
    // memcpy(staging.MapMemory(), materialData, byteSize);

    // uint64_t writePoint = (uint64_t)(materialDataOffset[index] + variant) * byteSize;
    // uint64_t writeOffset = writePoint + byteSize;
    // uint64_t newSize = writeOffset;
    // std::vector<vg::BufferCopyRegion> regions;
    // regions.push_back(vg::BufferCopyRegion(writePoint));
    // for (int i = index + 1; i < materialDataOffset.size(); i++) {
    //     uint64_t size = materialDataSize[i] * materialVariants[i];
    //     if (size == 0) continue;
    //     uint64_t readOffset = materialDataOffset[i] * materialDataSize[i];
    //     materialDataOffset[i] = std::ceil(writeOffset / (float)materialDataSize[i]);
    //     writeOffset = materialDataOffset[i] * materialDataSize[i];
    //     newSize = writeOffset + size;

    //     regions.push_back(vg::BufferCopyRegion(size, readOffset, writeOffset));
    // }

    // // Allocate new, bigger buffer.
    // vg::Buffer newBuffer(
    //     newSize, {vg::BufferUsage::TransferDst, vg::BufferUsage::TransferSrc, vg::BufferUsage::StorageBuffer}
    // );
    // vg::Allocate(newBuffer, vg::MemoryProperty::DeviceLocal);

    // // Copy old and new data to the new buffer.
    // vg::CmdBuffer(vg::currentDevice->GetQueue(0))
    //     .Begin()
    //     .Append(vg::cmd::CopyBuffer(materialBuffer, newBuffer, regions))
    //     .Append(vg::cmd::CopyBuffer(staging, newBuffer, {{staging.GetSize(), 0, writePoint}}))
    //     .End()
    //     .Submit()
    //     .Await();
    // std::swap(materialBuffer, newBuffer);
}

std::vector<vg::Subpass> Material::subpasses;
std::vector<vg::SubpassDependency> Material::dependecies;
std::vector<RenderBuffer::Region> Material::materialDataRegions;
RenderBuffer Material::materialBuffer(0, vg::BufferUsage::StorageBuffer);
// std::vector<int> Material::materialDataSize;
// std::vector<int> Material::materialDataOffset;
// std::vector<int> Material::materialVariants;
