#pragma once
#include <vector>
#include "VG/VG.h"
#include <glm/gtc/matrix_transform.hpp>

class DepthPyramid
{
public:
    vg::RenderPass depthPrepass;
    vg::Framebuffer depthFramebuffer;
    vg::Image pyramidImage;
    vg::ImageView pyramidImageView;

public:

    DepthPyramid() {}
    DepthPyramid(uint32_t width, uint32_t height);
    void SetSize(uint32_t width, uint32_t height);
};