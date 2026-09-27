#pragma once
#include "CmdBuffer.h"
#include "Enums.h"
#include "Framebuffer.h"
#include "Image.h"
#include "ImageView.h"
#include "RenderPass.h"
#include "Structs.h"
#include "Synchronization.h"
#include "VG/VG.h"
#include <string>
#include <unordered_map>
#include <vector>

class Resource {
  public:
    enum class Type { None = 0, Buffer, Image };

  public:
    std::string name;
    Type type;

    vg::Buffer buffer;

    vg::Image image;
    vg::ImageLayout initialLayout;
    vg::ImageLayout finalLayout;
    vg::ImageView imageView;
    vg::Sampler sampler;

    Resource(const std::string &name, vg::Buffer &&buffer)
        : name(name), type(Type::Buffer), buffer(std::move(buffer)) {}

    Resource(
        const std::string &name, vg::Image &&image, vg::ImageLayout &&initialLayout, vg::ImageLayout &&finalLayout,
        vg::Sampler &&sampler
    )
        : name(std::move(name)), type(Type::Image), image(std::move(image)), initialLayout(initialLayout),
          finalLayout(finalLayout), imageView(vg::ImageView(image, vg::ImageSubresource(vg::ImageAspect::Depth))),
          sampler(std::move(sampler)) {}

    Resource(const Resource &) = delete;
    Resource(Resource &&) = default;
    Resource &operator=(const Resource &) = delete;
    Resource &operator=(Resource &&) = default;
};

class Pass {
  public:
    std::string name;
    std::vector<std::string> inputResources;
    std::vector<std::string> outputResources;
    std::function<void(vg::CmdBuffer &)> execute;

    Pass(
        const std::string &name, const std::vector<std::string> &inputResources,
        const std::vector<std::string> &outputResources, const std::function<void(vg::CmdBuffer &)> &execute
    )
        : name(name), inputResources(inputResources), outputResources(outputResources), execute(execute) {}

    Pass(const Pass &) = default;
    Pass(Pass &&) = default;
    Pass &operator=(const Pass &) = default;
    Pass &operator=(Pass &&) = default;
};
class RenderGraph {
  private:
    std::unordered_map<std::string, Resource> resources;
    std::vector<Pass> passes;
    std::vector<int> executionOrder;
    std::vector<vg::Semaphore> semaphores;
    std::vector<std::pair<int, int>> semaphoreSignalWaitPairs;
    std::vector<vg::RenderPass> renderPasses;
    std::vector<vg::Framebuffer> framebuffers;

  public:
    RenderGraph();

    void AddResource(Resource &&resource);
    void AddPass(Pass &&pass);
    bool Compile();
    void Execute(vg::CmdBuffer &cmdBuffer, vg::Queue *queue);

    Resource &GetResource(const std::string &name);
    const Resource &GetResource(const std::string &name) const;
};
