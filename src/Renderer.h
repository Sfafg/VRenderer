#pragma once
#include <vector>
#include "Components.h"
#include "Mesh.h"
#include "VG/VG.h"
#include "GPUDrivenRendererSystem.h"
#include "Profiler/Profiler.h"

class Renderer : public ECS::System<MeshArray>
{
    static void* window;
    static vg::Surface surface;
    static vg::Swapchain swapchain;

    static std::vector<vg::Framebuffer> framebuffers;
    static vg::Image depthImage;
    static vg::ImageView depthImageView;
    static std::vector<vg::CmdBuffer> commandBuffer;
    static std::vector<vg::Semaphore> renderFinishedSemaphore;
    static std::vector<vg::Semaphore> imageAvailableSemaphore;
    static std::vector<vg::Fence> inFlightFence;
    static int frameIndex;

public:
    static GPUDrivenRendererSystem renderSystem;
    static std::vector<Material> materials;
    static void Init(void* window, vg::SurfaceHandle windowSurface, int width, int height);

    static void Add(MeshArray& component);
    static void Destroy(MeshArray& component);

    static void DrawFrame(Transform camerTransform, float fov);

    static void Present(vg::Queue& queue);

    static void Destroy();
};
REGISTER_SYSTEMS(MeshArray, Renderer);