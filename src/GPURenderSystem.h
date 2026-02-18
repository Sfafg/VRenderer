#pragma once
#include "VG/VG.h"
#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <memory>

class GPURenderSystem {
    // TODO: does it have to be a shared_ptr.
    std::shared_ptr<vg::ComputePipeline> gpuRenderer;
    std::shared_ptr<vg::ComputePipeline> clearDrawInstructions;
    std::shared_ptr<vg::ComputePipeline> depthReduction;
    vg::DescriptorPool descriptorPool;
    std::vector<vg::DescriptorSet> clearDescriptors;
    std::vector<vg::DescriptorSet> rendererDescriptors;
    std::vector<vg::DescriptorSet> depthReductionDescriptors;
    std::vector<vg::Buffer> counterBuffer;

  public:
    GPURenderSystem();
    GPURenderSystem(uint framesInFlight);
    GPURenderSystem(GPURenderSystem &&) = default;
    GPURenderSystem(const GPURenderSystem &) = delete;
    GPURenderSystem &operator=(GPURenderSystem &&) = default;
    GPURenderSystem &operator=(const GPURenderSystem &) = delete;
    ~GPURenderSystem() = default;

    void AttachBuffers(
        int frameIndex, const vg::ImageView &hiZView, const vg::Sampler &hiZSampler, const vg::Buffer &meshMetaData,
        const vg::Buffer &objectData, const vg::Buffer &batchBuffer, const vg::Buffer &drawInstructions,
        const vg::Buffer &instanceMapping
    );

    void Reduce(
        vg::CmdBuffer &cmdBuffer, uint32_t frameIndex, const vg::ImageView &depthView,
        std::vector<vg::ImageView> &mipImageViews, const vg::Sampler &sampler, uint32_t inputWidth, uint32_t inputHeight
    );

    void RecordCommands(
        vg::CmdBuffer &cmdBuffer, float cameraFarPlane, float cameraNearPlane, const glm::vec3 &cameraPositon,
        const glm::mat4 &cameraViewProjection, int frameIndex
    );
};
