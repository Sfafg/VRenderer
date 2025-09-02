#include "Renderer.h"
#include "GPURenderSystem.h"
#include <iostream>

using namespace vg;

void Renderer::RecreateRenderpass() {
    if (!IsInitialized()) return;

    if (Material::subpasses.size() == 0 || swapchain.GetImageCount() == 0) return;

    renderPass = RenderPass(
        {Attachment(surface.GetFormat(), ImageLayout::PresentSrc)},
        Vector<PipelineLayout>(PipelineLayout(
            {{vg::DescriptorSetLayoutBinding(0, vg::DescriptorType::UniformBuffer, 1, vg::ShaderStage::Vertex),
              vg::DescriptorSetLayoutBinding(1, vg::DescriptorType::StorageBuffer, 1, vg::ShaderStage::Vertex),
              vg::DescriptorSetLayoutBinding(2, vg::DescriptorType::StorageBuffer, 1, vg::ShaderStage::Vertex),
              vg::DescriptorSetLayoutBinding(3, vg::DescriptorType::StorageBuffer, 1, vg::ShaderStage::Vertex)}},
            {}
        )),
        Material::subpasses, Material::dependecies
    );

    // Create and allocate descriptor set layouts.
    std::vector<vg::DescriptorSetLayoutHandle> layouts(
        swapchain.GetImageCount(), renderPass.GetPipelineLayouts()[0].GetDescriptorSets()[0]
    );
    descriptorSets = descriptorPool.Allocate(layouts);

    for (size_t i = 0; i < descriptorSets.size(); i++) {
        descriptorSets[i].AttachBuffer(DescriptorType::UniformBuffer, passBuffer, 0, -1, 0, 0);
        if (Material::materialBuffer.GetBuffer(i) != vg::BufferHandle())
            descriptorSets[i].AttachBuffer(
                DescriptorType::StorageBuffer, Material::materialBuffer.GetBuffer(i), 0, -1, 1, 0
            );
    }

    framebuffers.resize(swapchain.GetImageCount());
    for (int i = 0; i < swapchain.GetImageCount(); i++)
        framebuffers[i] =
            Framebuffer(renderPass, {swapchain.GetImageViews()[i]}, swapchain.GetWidth(), swapchain.GetHeight());
}
void Renderer::Init(void *window, const vg::Queue *queue, vg::SurfaceHandle windowSurface, int width, int height) {
    Renderer::window = window;
    surface = Surface(windowSurface, {Format::BGRA8SRGB, ColorSpace::SRGBNL});
    swapchain = Swapchain(surface, 2, width, height);

    descriptorPool = DescriptorPool(
        swapchain.GetImageCount(), {{DescriptorType::UniformBuffer, swapchain.GetImageCount()},
                                    {DescriptorType::StorageBuffer, swapchain.GetImageCount() * 3}}
    );

    passBuffer = vg::Buffer(sizeof(PassData), vg::BufferUsage::UniformBuffer);
    vg::Allocate(passBuffer, vg::MemoryProperty::HostVisible);

    RecreateRenderpass();

    commandBuffer = std::vector<CmdBuffer>(swapchain.GetImageCount());
    renderFinishedSemaphore = std::vector<Semaphore>(swapchain.GetImageCount()),
    imageAvailableSemaphore = std::vector<Semaphore>(swapchain.GetImageCount());
    inFlightFence = std::vector<Fence>(swapchain.GetImageCount());
    for (int i = 0; i < swapchain.GetImageCount(); i++) {
        commandBuffer[i] = CmdBuffer(*queue);
        renderFinishedSemaphore[i] = Semaphore();
        imageAvailableSemaphore[i] = Semaphore();
        inFlightFence[i] = Fence(true);
    }

    GPURenderSystem::Init(swapchain.GetImageCount());
}

void Renderer::SetPassData(const PassData &data) {
    void *p = passBuffer.MapMemory();
    memcpy(p, &data, sizeof(data));
}

void Renderer::RenderFrame() {
    inFlightFence[frameIndex].Await(true);

    auto [imageIndex, result] = swapchain.GetNextImageIndex(imageAvailableSemaphore[frameIndex]);
    presentImageIndex = imageIndex;
    if (Material::materialBuffer.FlushBuffer(imageIndex))
        descriptorSets[imageIndex].AttachBuffer(
            DescriptorType::StorageBuffer, Material::materialBuffer.GetBuffer(imageIndex), 0, -1, 1, 0
        );

    Mesh::vertexBuffer.FlushBuffer(imageIndex);
    Mesh::indexBuffer.FlushBuffer(imageIndex);
    Mesh::meshDataBuffer.FlushBuffer(imageIndex);
    Batch::objectBuffer.FlushBuffer(imageIndex);
    Batch::instanceMappingBuffer.FlushBuffer(imageIndex);
    Batch::drawCallBuffer.FlushBuffer(imageIndex);
    Material::materialBuffer.FlushBuffer(imageIndex);

    descriptorSets[imageIndex].AttachBuffer(
        DescriptorType::StorageBuffer, Material::materialBuffer.GetBuffer(imageIndex), 0, -1, 1, 0
    );
    descriptorSets[imageIndex].AttachBuffer(
        DescriptorType::StorageBuffer, Batch::objectBuffer.GetBuffer(imageIndex), 0, -1, 2, 0
    );
    descriptorSets[imageIndex].AttachBuffer(
        DescriptorType::StorageBuffer, Batch::drawCallBuffer.GetBuffer(imageIndex), 0, -1, 3, 0
    );

    commandBuffer[frameIndex].Clear().Begin();

    GPURenderSystem::AttachBuffers(
        imageIndex, Mesh::meshDataBuffer.GetBuffer(imageIndex), Batch::drawCallBuffer.GetBuffer(imageIndex),
        Batch::instanceMappingBuffer.GetBuffer(imageIndex)
    );
    GPURenderSystem::RecordCommands(commandBuffer[frameIndex], imageIndex);

    commandBuffer[frameIndex]
        .Append(
            cmd::PipelineBarier(
                PipelineStage::ComputeShader, PipelineStage::VertexShader, Dependency::ByRegion,
                {MemoryBarrier(Access::MemoryWrite, Access::MemoryRead)}
            ),
            cmd::BeginRenderpass(
                renderPass, framebuffers[imageIndex], {0, 0}, {swapchain.GetWidth(), swapchain.GetHeight()},
                {ClearColor{0, 0, 0, 255}, ClearDepthStencil{1.0f, 0U}}, SubpassContents::Inline
            ),
            cmd::BindPipeline(renderPass.GetPipelines()[0]),
            cmd::BindVertexBuffers(
                {
                    (vg::BufferHandle)Mesh::vertexBuffer.GetBuffer(imageIndex),
                    (vg::BufferHandle)Batch::instanceMappingBuffer.GetBuffer(imageIndex),
                },
                {0, 0}
            ),
            cmd::BindIndexBuffer(Mesh::indexBuffer.GetBuffer(imageIndex), 0, IndexType::Uint32),
            cmd::SetViewport(Viewport(swapchain.GetWidth(), swapchain.GetHeight())),
            cmd::SetScissor(Scissor(swapchain.GetWidth(), swapchain.GetHeight())),
            cmd::BindDescriptorSets(
                renderPass.GetPipelineLayouts()[0], PipelineBindPoint::Graphics, 0, {descriptorSets[imageIndex]}
            ),
            cmd::DrawIndexedIndirect(
                Batch::drawCallBuffer.GetBuffer(imageIndex), 0, Batch::drawCallBuffer.GetSize() / sizeof(Batch),
                sizeof(Batch)
            ),
            cmd::EndRenderpass()
        )
        .End()
        .Submit(
            {{PipelineStage::ColorAttachmentOutput, imageAvailableSemaphore[frameIndex]}},
            {renderFinishedSemaphore[frameIndex]}, inFlightFence[frameIndex]
        );
}
void Renderer::Present(vg::Queue &queue) {
    queue.Present({renderFinishedSemaphore[frameIndex]}, {swapchain}, {(unsigned int)presentImageIndex});

    frameIndex = (frameIndex + 1) % swapchain.GetImageCount();
}

void Renderer::Destroy() {
    vg::currentDevice->WaitUntilIdle();
    swapchain.~Swapchain();

    framebuffers.clear();
    commandBuffer.clear();
    renderFinishedSemaphore.clear();
    imageAvailableSemaphore.clear();
    inFlightFence.clear();
}

bool Renderer::IsInitialized() { return window != nullptr; }

void *Renderer::window = nullptr;
vg::Surface Renderer::surface;
vg::Swapchain Renderer::swapchain;

vg::DescriptorPool Renderer::descriptorPool;
std::vector<vg::DescriptorSet> Renderer::descriptorSets;
std::vector<vg::Framebuffer> Renderer::framebuffers;
std::vector<vg::CmdBuffer> Renderer::commandBuffer;
std::vector<vg::Semaphore> Renderer::renderFinishedSemaphore;
std::vector<vg::Semaphore> Renderer::imageAvailableSemaphore;
std::vector<vg::Fence> Renderer::inFlightFence;
int Renderer::frameIndex;
int Renderer::presentImageIndex;

RenderPass Renderer::renderPass;
vg::Buffer Renderer::passBuffer;
