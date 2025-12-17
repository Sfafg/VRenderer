#include "GPURenderSystem.h"
#include "Batch.h"
#include "Renderer.h"
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
            {{{0, DescriptorType::UniformBuffer, 1, ShaderStage::Compute},
              {1, DescriptorType::StorageBuffer, 1, ShaderStage::Compute},
              {2, DescriptorType::StorageBuffer, 1, ShaderStage::Compute},
              {3, DescriptorType::StorageBuffer, 1, ShaderStage::Compute},
              {4, DescriptorType::StorageBuffer, 1, ShaderStage::Compute}}},
            {{ShaderStage::Compute, 0, sizeof(glm::mat4) + sizeof(uint32_t) * 4}}
        )
    ));

    descriptorPool = DescriptorPool(
        framesInFlight * 2,
        {{DescriptorType::UniformBuffer, framesInFlight}, {DescriptorType::StorageBuffer, 6 * framesInFlight}}
    );

    std::vector<DescriptorSetLayoutHandle> layouts(
        framesInFlight, clearDrawInstructions->GetPipelineLayout().GetDescriptorSets()[0]
    );
    std::vector<DescriptorSetLayoutHandle> layouts1(
        framesInFlight, gpuRenderer->GetPipelineLayout().GetDescriptorSets()[0]
    );

    clearDescriptors = descriptorPool.Allocate(layouts);
    rendererDescriptors = descriptorPool.Allocate(layouts1);
}

void GPURenderSystem::AttachBuffers(
    int frameIndex, const vg::Buffer &passBuffer, const vg::Buffer &meshMetaData, const vg::Buffer &objectData,
    const vg::Buffer &batchBuffer, const vg::Buffer &drawInstructions, const vg::Buffer &instanceMapping
) {
    clearDescriptors[frameIndex].AttachBuffer(DescriptorType::StorageBuffer, meshMetaData, 0, -1, 0, 0);
    clearDescriptors[frameIndex].AttachBuffer(DescriptorType::StorageBuffer, drawInstructions, 0, -1, 1, 0);

    rendererDescriptors[frameIndex].AttachBuffer(DescriptorType::UniformBuffer, passBuffer, 0, -1, 0, 0);
    rendererDescriptors[frameIndex].AttachBuffer(DescriptorType::StorageBuffer, batchBuffer, 0, -1, 1, 0);
    rendererDescriptors[frameIndex].AttachBuffer(DescriptorType::StorageBuffer, drawInstructions, 0, -1, 2, 0);
    rendererDescriptors[frameIndex].AttachBuffer(DescriptorType::StorageBuffer, instanceMapping, 0, -1, 3, 0);
    rendererDescriptors[frameIndex].AttachBuffer(DescriptorType::StorageBuffer, objectData, 0, -1, 4, 0);
}

void GPURenderSystem::RecordCommands(vg::CmdBuffer &cmdBuffer, const glm::mat4 &cameraViewProjection, int frameIndex) {
    assert(BatchArray::batchArray && "Current batchArray needs to be assigned!");

    cmdBuffer.Append(
        cmd::BindPipeline(*clearDrawInstructions),
        cmd::BindDescriptorSets(
            clearDrawInstructions->GetPipelineLayout(), PipelineBindPoint::Compute, 0, {clearDescriptors[frameIndex]}
        ),
        cmd::PushConstants(
            clearDrawInstructions->GetPipelineLayout(), ShaderStage::Compute, 0,
            (uint32_t)BatchArray::batchArray->batches.size()
        ),
        cmd::Dispatch(std::ceil(BatchArray::batchArray->batches.size() / 16.0), 1, 1),
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
                0, 0, BatchArray::batchArray->totalObjects, (uint32_t)BatchArray::batchArray->batches.size(),
                cameraViewProjection
            )
        ),
        cmd::Dispatch(std::ceil(BatchArray::batchArray->totalObjects / 1024.0), 1, 1)
    );
}
