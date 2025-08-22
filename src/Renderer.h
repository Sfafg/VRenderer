#pragma once
#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include "Material.h"
#include "Mesh.h"
#include "VG/VG.h"
#include <vector>

class Renderer {
    static void *window;
    static vg::Surface surface;
    static vg::Swapchain swapchain;

    static std::vector<vg::Framebuffer> framebuffers;
    static vg::DescriptorPool descriptorPool;
    static std::vector<vg::DescriptorSet> descriptorSets;
    static std::vector<vg::CmdBuffer> commandBuffer;
    static std::vector<vg::Semaphore> renderFinishedSemaphore;
    static std::vector<vg::Semaphore> imageAvailableSemaphore;
    static std::vector<vg::Fence> inFlightFence;
    static int frameIndex;
    static int presentImageIndex;

    static vg::RenderPass renderPass;
    static std::vector<std::vector<std::vector<const Mesh *>>> renderMeshes;
    static std::vector<std::vector<std::vector<const vg::Buffer *>>> instanceBuffers;
    static std::vector<std::vector<std::vector<int>>> instanceCount;
    static vg::Buffer passBuffer;

    friend Material;

    static void RecreateRenderpass();

  public:
    struct PassData {
        glm::mat4 viewProjection;
    };

  public:
    static constexpr int maxFramesInFlight = 2;
    static void Init(void *window, const vg::Queue *queue, vg::SurfaceHandle windowSurface, int width, int height);

    static void SetPassData(const PassData &data);
    static void StartFrame();
    static void Draw(const Mesh &mesh, const Material &material, const vg::Buffer &instanceBuffer, int instanceCount);
    static void Draw(const Mesh &mesh, const Material &material);
    static void EndFrame();

    static void Present(vg::Queue &queue);

    static void Destroy();

    static bool IsInitialized();
};
