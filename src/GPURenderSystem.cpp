#include "GPURenderSystem.h"
#include "Batch.h"
#include "Mesh.h"
using namespace vg;

void GPURenderSystem::Init(int framesInFlight) {
    clearDrawInstructions = ComputePipeline(
        Shader(ShaderStage::Compute, "resources/shaders/ClearInstructions.comp.spv"),
        PipelineLayout(
            {{DescriptorSetLayoutBinding(0, DescriptorType::StorageBuffer, 1, ShaderStage::Compute),
              DescriptorSetLayoutBinding(1, DescriptorType::StorageBuffer, 1, ShaderStage::Compute)}},
            {PushConstantRange(ShaderStage::Compute, 0, sizeof(uint32_t))}
        )
    );

    gpuRenderer = ComputePipeline(
        Shader(ShaderStage::Compute, "resources/shaders/Renderer.comp.spv"),
        PipelineLayout(
            {{DescriptorSetLayoutBinding(2, DescriptorType::StorageBuffer, 1, ShaderStage::Compute),
              DescriptorSetLayoutBinding(3, DescriptorType::StorageBuffer, 1, ShaderStage::Compute)}},
            {PushConstantRange(ShaderStage::Compute, 0, sizeof(uint32_t) * 2)}
        )
    );

    descriptorPool = DescriptorPool(framesInFlight * 2, {{DescriptorType::StorageBuffer, 4 * (uint)framesInFlight}});

    std::vector<DescriptorSetLayoutHandle> layouts(
        framesInFlight, clearDrawInstructions.GetPipelineLayout().GetDescriptorSets()[0]
    );
    std::vector<DescriptorSetLayoutHandle> layouts1(
        framesInFlight, gpuRenderer.GetPipelineLayout().GetDescriptorSets()[0]
    );

    clearDescriptors = descriptorPool.Allocate(layouts);
    rendererDescriptors = descriptorPool.Allocate(layouts1);
}

void GPURenderSystem::AttachBuffers(
    int frameIndex, const vg::Buffer &meshMetaData, const vg::Buffer &drawInstructions,
    const vg::Buffer &instanceMapping
) {
    clearDescriptors[frameIndex].AttachBuffer(DescriptorType::StorageBuffer, meshMetaData, 0, -1, 0, 0);
    clearDescriptors[frameIndex].AttachBuffer(DescriptorType::StorageBuffer, drawInstructions, 0, -1, 1, 0);

    rendererDescriptors[frameIndex].AttachBuffer(DescriptorType::StorageBuffer, drawInstructions, 0, -1, 2, 0);
    rendererDescriptors[frameIndex].AttachBuffer(DescriptorType::StorageBuffer, instanceMapping, 0, -1, 3, 0);
}

void GPURenderSystem::RecordCommands(vg::CmdBuffer &cmdBuffer, int frameIndex) {
    cmdBuffer.Append(
        cmd::BindPipeline(clearDrawInstructions),
        cmd::BindDescriptorSets(
            clearDrawInstructions.GetPipelineLayout(), PipelineBindPoint::Compute, 0, {clearDescriptors[frameIndex]}
        ),
        cmd::PushConstants(
            clearDrawInstructions.GetPipelineLayout(), ShaderStage::Compute, 0, (uint32_t)Batch::batches.size()
        ),
        cmd::Dispatch(std::ceil(Batch::batches.size() / 16.0), 1, 1),
        cmd::PipelineBarier(
            PipelineStage::ComputeShader, PipelineStage::ComputeShader, Dependency::ByRegion,
            {MemoryBarrier(Access::MemoryWrite, Access::MemoryRead)}
        ),
        cmd::BindPipeline(gpuRenderer),
        cmd::BindDescriptorSets(
            gpuRenderer.GetPipelineLayout(), PipelineBindPoint::Compute, 0, {rendererDescriptors[frameIndex]}
        ),
        cmd::PushConstants(gpuRenderer.GetPipelineLayout(), ShaderStage::Compute, sizeof(int), Batch::totalObjects),
        cmd::Dispatch(std::ceil(Batch::totalObjects / 1024.0), 1, 1)
    );
}

vg::ComputePipeline GPURenderSystem::gpuRenderer;
vg::ComputePipeline GPURenderSystem::clearDrawInstructions;
vg::DescriptorPool GPURenderSystem::descriptorPool;
std::vector<vg::DescriptorSet> GPURenderSystem::clearDescriptors;
std::vector<vg::DescriptorSet> GPURenderSystem::rendererDescriptors;
