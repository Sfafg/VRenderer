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
    friend BatchArray;
    friend RenderBuffer;

  public:
    struct DataArrays {
        MeshArray *meshArray;
        MaterialArray *materialArray;
        BatchArray *batchArray;
    };

    struct LightData {
        glm::mat4 lightViewProjection;
        glm::vec3 lightDirection;
        float __padding1;
        glm::vec3 lightColor;
        float __padding2;
    };

    Renderer();
    Renderer(
        uint maxFramesInFlight, const vg::Queue *queue, vg::SurfaceHandle windowSurface, int width, int height,
        const DataArrays &managers
    );
    Renderer(const Renderer &o) = delete;
    Renderer(Renderer &&o) = default;
    Renderer &operator=(const Renderer &o) = delete;
    Renderer &operator=(Renderer &&o) = default;
    ~Renderer();

    void MakeCurrent();
    void RenderFrame(
        vg::Queue &queue, const glm::mat4 &cameraViewProjection, const glm::vec3 &cameraPosition, float nearPlane,
        float farPlane, const Renderer::LightData &data, bool updateDrawInstructions = true
    );

    // private:
    void SetLightData(const LightData &data);

    uint maxFramesInFlight;
    vg::Surface surface;
    vg::Swapchain swapchain;
    vg::Image depthImage;
    vg::ImageView depthImageView;
    vg::Framebuffer depthPrepassFramebuffer;

    vg::Image hiZBuffer;
    vg::ImageView hiZBufferView;
    std::vector<vg::ImageView> hiZBufferMips;
    vg::Sampler hiZBufferSampler;

    vg::Image shadowImage;
    vg::ImageView shadowImageView;
    vg::Sampler shadowSampler;
    vg::Framebuffer shadowFramebuffer;
    vg::RenderPass depthOnlyPass;

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

    static void RecreateRenderpass();
    void _RecreateRenderpass();

    vg::Buffer lightBuffer;
    DataArrays dataArrays;

    struct DrawFromBuffer {
        DrawFromBuffer(vg::RenderPass *renderPass, vg::BufferHandle drawBuffer)
            : renderPass(renderPass), drawBuffer(drawBuffer) {}
        vg::RenderPass *renderPass;
        vg::BufferHandle drawBuffer;

      private:
        void operator()(vg::CmdBuffer &commandBuffer) const;
        friend vg::CmdBuffer;
    };

    struct BindMeshBuffers {
        BindMeshBuffers() {};

      private:
        void operator()(vg::CmdBuffer &commandBuffer) const;
        friend vg::CmdBuffer;
    };
};
