#pragma once
#include "VG/VG.h"
#include <memory>

class GPURenderSystem {
    std::shared_ptr<vg::ComputePipeline> gpuRenderer;
    std::shared_ptr<vg::ComputePipeline> clearDrawInstructions;
    vg::DescriptorPool descriptorPool;
    std::vector<vg::DescriptorSet> clearDescriptors;
    std::vector<vg::DescriptorSet> rendererDescriptors;

  public:
    GPURenderSystem();
    GPURenderSystem(uint framesInFlight);
    GPURenderSystem(GPURenderSystem &&) = default;
    GPURenderSystem(const GPURenderSystem &) = delete;
    GPURenderSystem &operator=(GPURenderSystem &&) = default;
    GPURenderSystem &operator=(const GPURenderSystem &) = delete;
    ~GPURenderSystem() = default;

    void AttachBuffers(
        int frameIndex, const vg::Buffer &passBuffer, const vg::Buffer &meshMetaData, const vg::Buffer &objectData,
        const vg::Buffer &drawInstructions, const vg::Buffer &instanceMapping
    );

    void RecordCommands(vg::CmdBuffer &cmdBuffer, int frameIndex);
};
