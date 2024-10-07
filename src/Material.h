#pragma once
#include <vector>
#include <memory>
#include "VG/VG.h"
#include "VG/Span.h"
struct Material
{
    std::unique_ptr<std::vector<vg::Shader*>> shaders;
    std::unique_ptr<vg::InputAssembly> inputAssembly;
    std::unique_ptr<vg::Rasterizer> rasterizer;
    std::unique_ptr<vg::Multisampling> multisampling;
    std::unique_ptr<vg::DepthStencil> depthStencil;
    std::unique_ptr<vg::ColorBlending> colorBlending;
    std::unique_ptr<std::vector<vg::DynamicState>> dynamicStates;

    Material() {}
    Material(Span<vg::Shader* const> shaders, vg::InputAssembly&& inputAssembly, vg::Rasterizer&& rasterizer, vg::Multisampling&& multisampling, vg::DepthStencil&& depthStencil, vg::ColorBlending&& colorBlending, Span<const vg::DynamicState> dynamicStates)
        :shaders(new std::vector<vg::Shader*>(shaders.begin(), shaders.end())), inputAssembly(new auto(inputAssembly)), rasterizer(new auto(rasterizer)), multisampling(new auto(multisampling)), depthStencil(new auto(depthStencil)), colorBlending(new auto(colorBlending)), dynamicStates(new std::vector<vg::DynamicState>(dynamicStates.begin(), dynamicStates.end()))
    {}

};
