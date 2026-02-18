#include "GPURenderSystem.h"
#include "Batch.h"
#include "Renderer.h"
#include <vector>
using namespace vg;

GPURenderSystem::GPURenderSystem() {}

GPURenderSystem::GPURenderSystem(uint framesInFlight) {
    clearDrawInstructions = std::make_shared<ComputePipeline>(ComputePipeline(
        Shader(ShaderStage::Compute, "resources/shaders/ClearInstructions.comp.spv"),
        PipelineLayout(
            {{{0, DescriptorType::StorageBuffer, 1, ShaderStage::Compute},
              {1, DescriptorType::StorageBuffer, 1, ShaderStage::Compute}}},
            {{ShaderStage::Compute, 0, sizeof(uint32_t)}}
        )
    ));

    gpuRenderer = std::make_shared<ComputePipeline>(ComputePipeline(
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

    depthReduction = std::make_shared<ComputePipeline>(ComputePipeline(
        Shader(ShaderStage::Compute, "resources/shaders/Reduction.comp.spv"),
        PipelineLayout(
            {{{0, DescriptorType::StorageBuffer, 1, ShaderStage::Compute},
              {1, DescriptorType::CombinedImageSampler, 1, ShaderStage::Compute},
              {2, DescriptorType::StorageImage, 12, ShaderStage::Compute}}},
            {{ShaderStage::Compute, 0, sizeof(int) * 4}}
        )
    ));

    descriptorPool = DescriptorPool(
        framesInFlight * 3, {{DescriptorType::CombinedImageSampler, framesInFlight * 2},
                             {DescriptorType::StorageImage, 12 * framesInFlight},
                             {DescriptorType::StorageBuffer, 8 * framesInFlight}}
    );

    std::vector<DescriptorSetLayoutHandle> layouts(
        framesInFlight, clearDrawInstructions->GetPipelineLayout().GetDescriptorSets()[0]
    );
    std::vector<DescriptorSetLayoutHandle> layouts1(
        framesInFlight, gpuRenderer->GetPipelineLayout().GetDescriptorSets()[0]
    );
    std::vector<DescriptorSetLayoutHandle> layouts2(
        framesInFlight, depthReduction->GetPipelineLayout().GetDescriptorSets()[0]
    );

    clearDescriptors = descriptorPool.Allocate(layouts);
    rendererDescriptors = descriptorPool.Allocate(layouts1);
    depthReductionDescriptors = descriptorPool.Allocate(layouts2);

    counterBuffer.resize(framesInFlight);
    for (int i = 0; i < framesInFlight; i++)
        counterBuffer[i] = vg::Buffer(sizeof(uint), {BufferUsage::StorageBuffer, BufferUsage::TransferDst});
    vg::Allocate(counterBuffer, vg::MemoryProperty::DeviceLocal);

    for (int i = 0; i < framesInFlight; i++)
        depthReductionDescriptors[i].AttachBuffer(DescriptorType::StorageBuffer, counterBuffer[i], 0, -1, 0, 0);
}

void GPURenderSystem::AttachBuffers(
    int frameIndex, const vg::ImageView &hiZView, const vg::Sampler &hiZSampler, const vg::Buffer &meshMetaData,
    const vg::Buffer &objectData, const vg::Buffer &batchBuffer, const vg::Buffer &drawInstructions,
    const vg::Buffer &instanceMapping
) {
    clearDescriptors[frameIndex].AttachBuffer(DescriptorType::StorageBuffer, meshMetaData, 0, -1, 0, 0);
    clearDescriptors[frameIndex].AttachBuffer(DescriptorType::StorageBuffer, drawInstructions, 0, -1, 1, 0);

    rendererDescriptors[frameIndex].AttachImage(
        vg::DescriptorType::CombinedImageSampler, vg::ImageLayout::General, hiZView, hiZSampler, 0, 0
    );
    rendererDescriptors[frameIndex].AttachBuffer(DescriptorType::StorageBuffer, batchBuffer, 0, -1, 1, 0);
    rendererDescriptors[frameIndex].AttachBuffer(DescriptorType::StorageBuffer, drawInstructions, 0, -1, 2, 0);
    rendererDescriptors[frameIndex].AttachBuffer(DescriptorType::StorageBuffer, instanceMapping, 0, -1, 3, 0);
    rendererDescriptors[frameIndex].AttachBuffer(DescriptorType::StorageBuffer, objectData, 0, -1, 4, 0);
    rendererDescriptors[frameIndex].AttachBuffer(DescriptorType::StorageBuffer, meshMetaData, 0, -1, 5, 0);
}

void GPURenderSystem::Reduce(
    vg::CmdBuffer &cmdBuffer, uint32_t frameIndex, const ImageView &depthView, std::vector<ImageView> &mipImageViews,
    const Sampler &sampler, uint32_t inputWidth, uint32_t inputHeight
) {
    assert(mipImageViews.size() < 12);

    depthReductionDescriptors[frameIndex].AttachImage(
        vg::DescriptorType::CombinedImageSampler, vg::ImageLayout::DepthStencilReadOnlyOptimal, depthView, sampler, 1, 0
    );
    for (int i = 0; i < 12; i++) {
        depthReductionDescriptors[frameIndex].AttachImage(
            vg::DescriptorType::StorageImage, vg::ImageLayout::General,
            mipImageViews[std::min(i, (int)mipImageViews.size() - 1)], sampler, 2, i
        );
    }

    cmdBuffer.Append(
        vg::cmd::FillBuffer(counterBuffer[frameIndex], 0, sizeof(uint), 0), vg::cmd::BindPipeline(*depthReduction),
        vg::cmd::BindDescriptorSets(
            depthReduction->GetPipelineLayout(), vg::PipelineBindPoint::Compute, 0,
            {depthReductionDescriptors[frameIndex]}
        ),
        vg::cmd::PushConstants(
            depthReduction->GetPipelineLayout(), vg::ShaderStage::Compute, 0,
            std::make_tuple(0, (int)mipImageViews.size(), glm::ivec2(inputWidth, inputHeight))
        ),
        vg::cmd::Dispatch(std::ceil(inputWidth / 64.0f), std::ceil(inputHeight / 64.0f), 1)
    );
}

void GPURenderSystem::RecordCommands(
    vg::CmdBuffer &cmdBuffer, float cameraFarPlane, float cameraNearPlane, const glm::vec3 &cameraPositon,
    const glm::mat4 &cameraViewProjection, int frameIndex
) {
    assert(BatchArray::batchArray && "Current batchArray needs to be assigned!");

    cmdBuffer.Append(
        cmd::BindPipeline(*clearDrawInstructions),
        cmd::BindDescriptorSets(
            clearDrawInstructions->GetPipelineLayout(), PipelineBindPoint::Compute, 0, {clearDescriptors[frameIndex]}
        ),
        cmd::PushConstants(
            clearDrawInstructions->GetPipelineLayout(), ShaderStage::Compute, 0,
            (uint32_t)BatchArray::batchArray->drawCalls.size()
        ),
        cmd::Dispatch(std::ceil(BatchArray::batchArray->drawCalls.size() / 64.0), 1, 1),
        cmd::PipelineBarier(
            PipelineStage::ComputeShader, PipelineStage::ComputeShader, Dependency::ByRegion,
            {MemoryBarrier(Access::MemoryWrite, Access::MemoryRead)}
        ),
        cmd::BindPipeline(*gpuRenderer),
        cmd::BindDescriptorSets(
            gpuRenderer->GetPipelineLayout(), PipelineBindPoint::Compute, 0, {rendererDescriptors[frameIndex]}
        ),
        cmd::PushConstants(
            gpuRenderer->GetPipelineLayout(), ShaderStage::Compute, 0,
            std::make_tuple(
                0, 0, cameraFarPlane, cameraNearPlane, (int)BatchArray::batchArray->transparencyBucketCount,
                cameraPositon, (int)BatchArray::batchArray->transparentDrawCallsCount,
                (int)BatchArray::batchArray->firstTransparentDrawCall, (int)BatchArray::batchArray->totalObjects,
                (int)BatchArray::batchArray->batches.size(), cameraViewProjection
            )
        ),
        cmd::Dispatch(std::ceil(BatchArray::batchArray->totalObjects / 1024.0), 1, 1)
    );
}
