#pragma once
#include "VG/VG.h"
#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <memory>

class GPURenderer {
    vg::ComputePipeline gpuRenderer;
    vg::ComputePipeline clearInstructions;

    vg::DescriptorPool descriptorPool;
    vg::DescriptorSet clearDescriptors;
    vg::DescriptorSet rendererDescriptors;

  public:
    vg::Buffer drawCalls;
    vg::Buffer instanceMapping;

  public:
    GPURenderer();
    GPURenderer(uint framesInFlight);
    GPURenderer(GPURenderer &&) = default;
    GPURenderer(const GPURenderer &) = delete;
    GPURenderer &operator=(GPURenderer &&) = default;
    GPURenderer &operator=(const GPURenderer &) = delete;
    ~GPURenderer() = default;

    void UpdateBuffers(int totalInstanceCount, const vg::Buffer &partialDrawCalls);

    void AttachBuffers(
        const vg::ImageView &hiZView, const vg::Sampler &hiZSampler, const vg::Buffer &meshMetaData,
        const vg::Buffer &objectData, const vg::Buffer &batchBuffer, const vg::Buffer &partialDrawCalls
    );

    void RecordCommands();

    class WriteInstructions {
        GPURenderer &renderer;
        float cameraFarPlane;
        float cameraNearPlane;
        const glm::vec3 &cameraPosition;
        const glm::mat4 &cameraViewProjection;

      public:
        WriteInstructions(
            GPURenderer &renderer, float cameraFarPlane, float cameraNearPlane, const glm::vec3 &cameraPosition,
            const glm::mat4 &cameraViewProjection
        );

      private:
        void operator()(vg::CmdBuffer &commandBuffer) const;
        friend vg::CmdBuffer;
    };

    class PipelineBarrier {
        GPURenderer &renderer;
        vg::PipelineStage dstStage;
        vg::Flags<vg::Access> dstAccessMask;

      public:
        PipelineBarrier(GPURenderer &renderer, vg::PipelineStage dstStage, vg::Flags<vg::Access> dstAccessMask);

      private:
        void operator()(vg::CmdBuffer &commandBuffer) const;
        friend vg::CmdBuffer;
    };
};
