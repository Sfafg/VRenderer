#pragma once
#include <vector>
#include "VG/VG.h"
#include "BufferArray.h"
#include <glm/gtc/matrix_transform.hpp>

class DepthPyramid
{
    vg::CmdBuffer reductionCmdBuffer;

public:
    vg::ComputePipeline computeReduction;
    vg::Image pyramidImage;
    vg::ImageView pyramidImageView;
    std::vector<vg::ImageView> mipImageViews;
    vg::Sampler reductionSampler;
    vg::DescriptorPool pool;
    std::vector<vg::DescriptorSet> descriptors;

public:

    DepthPyramid() {}
    DepthPyramid(uint32_t width, uint32_t height);
    vg::cmd::ExecuteCommands Generate(
        vg::CmdBuffer& cmdBuffer,
        const vg::Image& depth,
        const vg::ImageView& depthView);
};