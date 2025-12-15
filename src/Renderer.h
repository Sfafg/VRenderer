#pragma once
#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include "Material.h"
#include "Mesh.h"
#include "RenderObject.h"
#include "GPURenderSystem.h"
#include "VG/VG.h"
#include <vector>

class Renderer {
    friend BatchManager;
    friend RenderBuffer;

  public:
    struct Managers {
        MeshManager *meshManager;
        MaterialManager *materialManager;
        BatchManager *batchManager;
    };

    struct PassData {
        glm::mat4 lightViewProjection;
        glm::vec3 cameraPosition;
        float padding1;
        glm::vec3 lightDirection;
        float padding2;
        glm::vec3 lightColor;
        float padding3;
    };

    Renderer();
    Renderer(
        uint maxFramesInFlight, const vg::Queue *queue, vg::SurfaceHandle windowSurface, int width, int height,
        const Managers &managers
    );
    Renderer(const Renderer &o) = delete;
    Renderer(Renderer &&o) = default;
    Renderer &operator=(const Renderer &o) = delete;
    Renderer &operator=(Renderer &&o) = default;
    ~Renderer();

    void SetPassData(const PassData &data);

    void RenderFrame(vg::Queue &queue, const glm::mat4 &cameraViewProjection, const Renderer::PassData &data);

    // private:
    uint maxFramesInFlight;
    vg::Surface surface;
    vg::Swapchain swapchain;
    vg::Image depthImage;
    vg::ImageView depthImageView;

    vg::Image shadowImage;
    vg::ImageView shadowImageView;
    vg::Sampler shadowSampler;
    vg::Framebuffer shadowFramebuffer;
    vg::RenderPass shadowPass;

    vg::RenderPass renderPass;
    GPURenderSystem gpuRenderSystem;

    vg::DescriptorPool descriptorPool;
    std::vector<vg::DescriptorSet> descriptorSets;
    std::vector<vg::Framebuffer> framebuffers;
    std::vector<vg::CmdBuffer> commandBuffer;
    std::vector<vg::Semaphore> renderFinishedSemaphore;
    std::vector<vg::Semaphore> imageAvailableSemaphore;
    std::vector<vg::Fence> inFlightFence;
    int frameIndex;

    void RecreateRenderpass();

    vg::Buffer passBuffer;
    Managers managers;
};

extern Renderer *currentRenderer;
