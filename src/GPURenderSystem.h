#pragma once
#include "VG/VG.h"

class GPURenderSystem {
    static vg::ComputePipeline gpuRenderer;
    static vg::ComputePipeline clearDrawInstructions;
    static vg::DescriptorPool descriptorPool;
    static std::vector<vg::DescriptorSet> clearDescriptors;
    static std::vector<vg::DescriptorSet> rendererDescriptors;

  public:
    static void Init(int framesInFlight);

    static void AttachBuffers(
        int frameIndex, const vg::Buffer &passBuffer, const vg::Buffer &meshMetaData, const vg::Buffer &objectData,
        const vg::Buffer &drawInstructions, const vg::Buffer &instanceMapping
    );

    static void RecordCommands(vg::CmdBuffer &cmdBuffer, int frameIndex);
};
