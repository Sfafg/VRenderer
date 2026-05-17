#pragma once
#include "VG/VG.h"

class HiZBuffer {
    vg::ComputePipeline computePipeline;
    vg::DescriptorPool descriptorPool;
    vg::DescriptorSet descriptors;

    vg::Image image;
    std::vector<vg::ImageView> mips;
    vg::Buffer counter;

  public:
    vg::ImageView view;
    vg::Sampler sampler;

    HiZBuffer() {}
    HiZBuffer(uint width, uint height);
    HiZBuffer(HiZBuffer &&) = default;
    HiZBuffer(const HiZBuffer &) = delete;
    HiZBuffer &operator=(HiZBuffer &&) = default;
    HiZBuffer &operator=(const HiZBuffer &) = delete;
    ~HiZBuffer() = default;

    friend class HiZBufferReduce;
    friend class HiZBufferPipelineBarrier;

    class Reduce {
        HiZBuffer &buffer;
        const vg::ImageView &depthView;

      public:
        Reduce(HiZBuffer &buffer, const vg::ImageView &depthView);

      private:
        void operator()(vg::CmdBuffer &commandBuffer) const;
        friend vg::CmdBuffer;
    };

    class PipelineBarrier {
        HiZBuffer &buffer;
        vg::PipelineStage dstStage;
        vg::ImageLayout targetLayout;
        vg::Flags<vg::Access> dstAccessMask;

      public:
        PipelineBarrier(
            HiZBuffer &buffer, vg::PipelineStage dstStage, vg::ImageLayout targetLayout,
            vg::Flags<vg::Access> dstAccessMask
        );

      private:
        void operator()(vg::CmdBuffer &commandBuffer) const;
        friend vg::CmdBuffer;
    };
    ;
};
