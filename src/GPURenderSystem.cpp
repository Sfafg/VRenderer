#include "GPURenderSystem.h"
#include "Batch.h"
#include "Renderer.h"
#include <vector>
using namespace vg;

GPURenderer::GPURenderer() {}

GPURenderer::GPURenderer(uint framesInFlight) {
    clearInstructions = ComputePipeline(ComputePipeline(
        Shader(ShaderStage::Compute, "resources/shaders/ClearInstructions.comp.spv"),
        PipelineLayout(
            {{{0, DescriptorType::StorageBuffer, 1, ShaderStage::Compute},
              {1, DescriptorType::StorageBuffer, 1, ShaderStage::Compute},
              {2, DescriptorType::StorageBuffer, 1, ShaderStage::Compute}}},
            {{ShaderStage::Compute, 0, sizeof(uint32_t)}}
        )
    ));

    gpuRenderer = ComputePipeline(ComputePipeline(
        Shader(ShaderStage::Compute, "resources/shaders/Renderer.comp.spv"),
        PipelineLayout(
            {{{0, DescriptorType::CombinedImageSampler, 1, ShaderStage::Compute},
              {1, DescriptorType::StorageBuffer, 1, ShaderStage::Compute},
              {2, DescriptorType::StorageBuffer, 1, ShaderStage::Compute},
              {3, DescriptorType::StorageBuffer, 1, ShaderStage::Compute},
              {4, DescriptorType::StorageBuffer, 1, ShaderStage::Compute},
              {5, DescriptorType::StorageBuffer, 1, ShaderStage::Compute}}},
            {{ShaderStage::Compute, 0, 112}}
        )
    ));

    descriptorPool = DescriptorPool(
        3, {{DescriptorType::CombinedImageSampler, 2},
            {DescriptorType::StorageImage, 13},
            {DescriptorType::StorageBuffer, 8}}
    );

    drawCalls = vg::Buffer(
        1,
        {BufferUsage::StorageBuffer, BufferUsage::IndirectBuffer, BufferUsage::TransferDst, BufferUsage::TransferSrc},
        SharingMode::Exclusive
    );
    vg::Allocate(drawCalls, vg::MemoryProperty::DeviceLocal);
    instanceMapping = vg::Buffer(
        1, {BufferUsage::StorageBuffer, BufferUsage::VertexBuffer, BufferUsage::TransferDst, BufferUsage::TransferSrc},
        SharingMode::Exclusive
    );
    vg::Allocate(instanceMapping, vg::MemoryProperty::DeviceLocal);

    clearDescriptors =
        std::move(descriptorPool.Allocate(clearInstructions.GetPipelineLayout().GetDescriptorSets()[0])[0]);
    rendererDescriptors = std::move(descriptorPool.Allocate(gpuRenderer.GetPipelineLayout().GetDescriptorSets()[0])[0]);
}

void GPURenderer::UpdateBuffers(int totalInstanceCount, const vg::Buffer &partialDrawCalls) {
    int drawCallCount = partialDrawCalls.GetSize() / sizeof(BatchArray::PartialDrawCall);
    if (drawCallCount != drawCalls.GetSize() / sizeof(BatchArray::DrawCall)) {
        vg::Buffer newBuffer(
            drawCallCount * sizeof(BatchArray::DrawCall),
            {BufferUsage::StorageBuffer, BufferUsage::IndirectBuffer, BufferUsage::TransferDst,
             BufferUsage::TransferSrc},
            SharingMode::Exclusive
        );
        vg::Allocate(newBuffer, {vg::MemoryProperty::DeviceLocal});

        vg::CmdBuffer(vg::currentDevice->GetQueue(0))
            .Begin()
            .Append(
                vg::cmd::CopyBuffer(
                    drawCalls, newBuffer, {vg::BufferCopyRegion(std::min(drawCalls.GetSize(), newBuffer.GetSize()))}
                )
            )
            .End()
            .Submit()
            .Await();

        drawCalls = std::move(newBuffer);
    }
    if (totalInstanceCount > instanceMapping.GetSize() / sizeof(uint)) {
        vg::Buffer newBuffer(
            totalInstanceCount * sizeof(uint),
            {BufferUsage::StorageBuffer, BufferUsage::VertexBuffer, BufferUsage::TransferSrc, BufferUsage::TransferDst},
            SharingMode::Exclusive
        );
        vg::Allocate(newBuffer, {vg::MemoryProperty::DeviceLocal});

        vg::CmdBuffer(vg::currentDevice->GetQueue(0))
            .Begin()
            .Append(
                vg::cmd::CopyBuffer(
                    instanceMapping, newBuffer,
                    {vg::BufferCopyRegion(std::min(instanceMapping.GetSize(), newBuffer.GetSize()))}
                )
            )
            .End()
            .Submit()
            .Await();

        instanceMapping = std::move(newBuffer);
    }
}

void GPURenderer::AttachBuffers(
    const vg::ImageView &hiZView, const vg::Sampler &hiZSampler, const vg::Buffer &meshMetaData,
    const vg::Buffer &objectData, const vg::Buffer &batchBuffer, const vg::Buffer &partialDrawCalls
) {
    clearDescriptors.AttachBuffer(DescriptorType::StorageBuffer, meshMetaData, 0, -1, 0, 0);
    clearDescriptors.AttachBuffer(DescriptorType::StorageBuffer, drawCalls, 0, -1, 1, 0);
    clearDescriptors.AttachBuffer(DescriptorType::StorageBuffer, partialDrawCalls, 0, -1, 2, 0);

    rendererDescriptors.AttachImage(
        vg::DescriptorType::CombinedImageSampler, vg::ImageLayout::ShaderReadOnlyOptimal, hiZView, hiZSampler, 0, 0
    );
    rendererDescriptors.AttachBuffer(DescriptorType::StorageBuffer, batchBuffer, 0, -1, 1, 0);
    rendererDescriptors.AttachBuffer(DescriptorType::StorageBuffer, drawCalls, 0, -1, 2, 0);
    rendererDescriptors.AttachBuffer(DescriptorType::StorageBuffer, instanceMapping, 0, -1, 3, 0);
    rendererDescriptors.AttachBuffer(DescriptorType::StorageBuffer, objectData, 0, -1, 4, 0);
    rendererDescriptors.AttachBuffer(DescriptorType::StorageBuffer, meshMetaData, 0, -1, 5, 0);
}

GPURenderer::WriteInstructions::WriteInstructions(
    GPURenderer &renderer, float cameraFarPlane, float cameraNearPlane, const glm::vec3 &cameraPosition,
    const glm::mat4 &cameraViewProjection
)
    : renderer(renderer), cameraFarPlane(cameraFarPlane), cameraNearPlane(cameraNearPlane),
      cameraPosition(cameraPosition), cameraViewProjection(cameraViewProjection) {}

void GPURenderer::WriteInstructions::operator()(vg::CmdBuffer &commandBuffer) const {
    assert(BatchArray::batchArray && "Current batchArray needs to be assigned!");

    commandBuffer.Append(
        cmd::BindPipeline(renderer.clearInstructions),
        cmd::BindDescriptorSets(
            renderer.clearInstructions.GetPipelineLayout(), PipelineBindPoint::Compute, 0, {renderer.clearDescriptors}
        ),
        cmd::PushConstants(
            renderer.clearInstructions.GetPipelineLayout(), ShaderStage::Compute, 0,
            (uint32_t)BatchArray::batchArray->drawCalls.size()
        ),
        cmd::Dispatch(std::ceil(BatchArray::batchArray->drawCalls.size() / 64.0), 1, 1),
        cmd::PipelineBarier(
            PipelineStage::ComputeShader, PipelineStage::ComputeShader, Dependency::ByRegion,
            {MemoryBarrier(Access::MemoryWrite, Access::MemoryRead)}
        ),
        cmd::BindPipeline(renderer.gpuRenderer),
        cmd::BindDescriptorSets(
            renderer.gpuRenderer.GetPipelineLayout(), PipelineBindPoint::Compute, 0, {renderer.rendererDescriptors}
        ),
        cmd::PushConstants(
            renderer.gpuRenderer.GetPipelineLayout(), ShaderStage::Compute, 0,
            std::make_tuple(
                0, 0, cameraFarPlane, cameraNearPlane, (int)BatchArray::batchArray->transparencyBucketCount,
                cameraPosition, (int)BatchArray::batchArray->transparentDrawCallsCount,
                (int)BatchArray::batchArray->firstTransparentDrawCall, (int)BatchArray::batchArray->totalObjects,
                (int)BatchArray::batchArray->batches.size(), cameraViewProjection
            )
        ),
        cmd::Dispatch(std::ceil(BatchArray::batchArray->totalObjects / 1024.0), 1, 1)
    );
}

GPURenderer::PipelineBarrier::PipelineBarrier(
    GPURenderer &renderer, vg::PipelineStage dstStage, vg::Flags<vg::Access> dstAccessMask
)
    : renderer(renderer), dstStage(dstStage), dstAccessMask(dstAccessMask) {}

void GPURenderer::PipelineBarrier::operator()(vg::CmdBuffer &commandBuffer) const {
    commandBuffer.Append(
        cmd::PipelineBarier(
            PipelineStage::ComputeShader, PipelineStage::VertexShader, Dependency::ByRegion,
            {MemoryBarrier(Access::MemoryWrite, Access::MemoryRead)}
        )
    );
}
