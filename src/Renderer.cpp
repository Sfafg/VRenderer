#include "Renderer.h"
#include "GLFW/glfw3.h"
#include <unordered_map>
void Renderer::Init(void* window, vg::SurfaceHandle windowSurface, int width, int height)
{
    Renderer::window = window;
    surface = vg::Surface(windowSurface, { vg::Format::BGRA8SRGB, vg::ColorSpace::SRGBNL });
    swapchain = vg::Swapchain(surface, 2, width, height, vg::Usage::ColorAttachment, vg::PresentMode::Immediate);

    depthImage = vg::Image({ swapchain.GetWidth(), swapchain.GetHeight() }, { vg::Format::D32SFLOAT,vg::Format::D32SFLOATS8UINT,vg::Format::x8D24UNORMPACK }, { vg::FormatFeature::DepthStencilAttachment }, { vg::ImageUsage::DepthStencilAttachment,vg::ImageUsage::Sampled });
    vg::Allocate({ &depthImage }, { vg::MemoryProperty::DeviceLocal });
    depthImageView = vg::ImageView(depthImage, { vg::ImageAspect::Depth });

    renderSystem = GPUDrivenRendererSystem(materials, { vg::Attachment(surface.GetFormat(),vg::ImageLayout::PresentSrc) , vg::Attachment(depthImage.GetFormat(),vg::ImageLayout::DepthStencilAttachmentOptimal) }, swapchain.GetImageCount(), &vg::currentDevice->GetQueue(0));

    framebuffers.resize(swapchain.GetImageCount());
    frameIndex = 0;
    for (int i = 0; i < framebuffers.size(); i++)
        framebuffers[i] = vg::Framebuffer(renderSystem.renderPass, { swapchain.GetImageViews()[i],depthImageView }, swapchain.GetWidth(), swapchain.GetHeight());

    commandBuffer.resize(swapchain.GetImageCount());
    renderFinishedSemaphore.resize(swapchain.GetImageCount());
    imageAvailableSemaphore.resize(swapchain.GetImageCount());
    inFlightFence.resize(swapchain.GetImageCount());

    for (int i = 0; i < swapchain.GetImageCount(); i++)
    {
        commandBuffer[i] = vg::CmdBuffer(vg::currentDevice->GetQueue(0));
        renderFinishedSemaphore[i] = vg::Semaphore();
        imageAvailableSemaphore[i] = vg::Semaphore();
        inFlightFence[i] = vg::Fence(true);
    }
}

void Renderer::Add(MeshArray& component)
{
    renderSystem.AddRenderObject(component.meshID, component.materialID, component.GetComponent<Transform>().Matrix());
}
void Renderer::Destroy(MeshArray& component)
{

}

void Renderer::DrawFrame(Transform cameraTransform, float fov)
{
    inFlightFence[frameIndex].Await(true);

    vg::Swapchain oldSwapchain;
    int width, height;
    glfwGetFramebufferSize((GLFWwindow*) window, &width, &height);
    if (width != swapchain.GetWidth() || height != swapchain.GetHeight())
    {
        vg::currentDevice->WaitUntilIdle();
        while (width != 0 && height != 0)
        {
            glfwGetFramebufferSize((GLFWwindow*) window, &width, &height);
            glfwPollEvents();
        }

        std::swap(oldSwapchain, swapchain);
        swapchain = vg::Swapchain(surface, 2, width, height, oldSwapchain);
        depthImage = vg::Image({ swapchain.GetWidth(), swapchain.GetHeight() }, depthImage.GetFormat(), { vg::ImageUsage::DepthStencilAttachment, vg::ImageUsage::Sampled });
        vg::Allocate(Span<vg::Image* const>{&depthImage }, { vg::MemoryProperty::DeviceLocal });
        depthImageView = vg::ImageView(depthImage, { vg::ImageAspect::Depth });
        for (int i = 0; i < framebuffers.size(); i++)
            framebuffers[i] = vg::Framebuffer(renderSystem.renderPass, { swapchain.GetImageViews()[i], depthImageView }, swapchain.GetWidth(), swapchain.GetHeight());
    }

    auto [imageIndex, result] = swapchain.GetNextImageIndex(imageAvailableSemaphore[frameIndex]);

    glm::mat4 cameraView = glm::lookAt(cameraTransform.position, cameraTransform.position + cameraTransform.Forward(), cameraTransform.Up());
    glm::mat4 cameraProjection = glm::perspective(fov, (float) width / (float) height, 0.01f, 1000.0f);
    cameraProjection[1][1] *= -1;

    commandBuffer[frameIndex].Clear().Begin();
    renderSystem.GetRenderCommands(depthImage, depthImageView, commandBuffer[frameIndex], cameraView, cameraProjection, framebuffers[frameIndex], width, height);
    commandBuffer[frameIndex].End().Submit({ {vg::PipelineStage::ColorAttachmentOutput, imageAvailableSemaphore[frameIndex]} }, { renderFinishedSemaphore[frameIndex] }, inFlightFence[frameIndex]);
}

void Renderer::Present(vg::Queue& queue)
{
    queue.Present({ renderFinishedSemaphore[frameIndex] }, { swapchain }, { (uint32_t) frameIndex });
    frameIndex = (frameIndex + 1) % swapchain.GetImageCount();
}

void Renderer::Destroy()
{
    vg::currentDevice->WaitUntilIdle();
    swapchain.~Swapchain();
    surface.~Surface();

    framebuffers.clear();
    depthImage.~Image();
    depthImageView.~ImageView();
    commandBuffer.clear();
    renderFinishedSemaphore.clear();
    imageAvailableSemaphore.clear();
    inFlightFence.clear();
    renderSystem.~GPUDrivenRendererSystem();

    materials.clear();
}


vg::Surface Renderer::surface;
vg::Swapchain Renderer::swapchain;
vg::Image Renderer::depthImage;
vg::ImageView Renderer::depthImageView;
std::vector<vg::Framebuffer> Renderer::framebuffers;
int Renderer::frameIndex;

std::vector<vg::CmdBuffer> Renderer::commandBuffer;
std::vector<vg::Semaphore> Renderer::renderFinishedSemaphore;
std::vector<vg::Semaphore> Renderer::imageAvailableSemaphore;
std::vector<vg::Fence> Renderer::inFlightFence;

GPUDrivenRendererSystem Renderer::renderSystem;

void* Renderer::window;
std::vector<Material> Renderer::materials;