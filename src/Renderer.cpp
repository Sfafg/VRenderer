#include "Renderer.h"
#include <iostream>

using namespace vg;

void Renderer::RecreateRenderpass() {
    if (!IsInitialized()) return;

    if (Material::subpasses.size() == 0) return;

    renderPass = RenderPass(
        {Attachment(surface.GetFormat(), ImageLayout::PresentSrc)},
        Vector<PipelineLayout>(PipelineLayout(
            {{vg::DescriptorSetLayoutBinding(0, vg::DescriptorType::UniformBuffer, 1, vg::ShaderStage::Vertex),
              vg::DescriptorSetLayoutBinding(1, vg::DescriptorType::StorageBuffer, 1, vg::ShaderStage::Vertex)}},
            {vg::PushConstantRange(vg::ShaderStage::Vertex, 0, sizeof(int))}
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
                                    {DescriptorType::StorageBuffer, swapchain.GetImageCount()}}
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
}

void Renderer::SetPassData(const PassData &data) {
    void *p = passBuffer.MapMemory();
    memcpy(p, &data, sizeof(data));
}

void Renderer::StartFrame() {
    renderMeshes.clear();
    renderMeshes.resize(Material::subpasses.size());
    instanceBuffers.clear();
    instanceBuffers.resize(Material::subpasses.size());
    instanceCount.clear();
    instanceCount.resize(Material::subpasses.size());
    for (int i = 0; i < Material::subpasses.size(); i++) {
        int variantCount = std::max(1U, Material::materialBuffer.sizes[i] / Material::materialBuffer.alignments[i]);
        renderMeshes[i].clear();
        renderMeshes[i].resize(variantCount);
        instanceBuffers[i].clear();
        instanceBuffers[i].resize(variantCount);
        instanceCount[i].clear();
        instanceCount[i].resize(variantCount);
    }
    inFlightFence[frameIndex].Await(true);

    auto [imageIndex, result] = swapchain.GetNextImageIndex(imageAvailableSemaphore[frameIndex]);
    if (Material::materialBuffer.FlushBuffer(imageIndex))
        descriptorSets[imageIndex].AttachBuffer(
            DescriptorType::StorageBuffer, Material::materialBuffer.GetBuffer(imageIndex), 0, -1, 1, 0
        );

    Mesh::vertexBuffer.FlushBuffer(imageIndex);
    Mesh::indexBuffer.FlushBuffer(imageIndex);
    Mesh::meshDataBuffer.FlushBuffer(imageIndex);
    commandBuffer[frameIndex].Clear().Begin().Append(
        cmd::BeginRenderpass(
            renderPass, framebuffers[imageIndex], {0, 0}, {swapchain.GetWidth(), swapchain.GetHeight()},
            {ClearColor{0, 0, 0, 255}, ClearDepthStencil{1.0f, 0U}}, SubpassContents::Inline
        ),
        cmd::BindVertexBuffers(Mesh::vertexBuffer.GetBuffer(imageIndex), 0),
        cmd::BindIndexBuffer(Mesh::indexBuffer.GetBuffer(imageIndex), 0, IndexType::Uint32),
        cmd::SetViewport(Viewport(swapchain.GetWidth(), swapchain.GetHeight())),
        cmd::SetScissor(Scissor(swapchain.GetWidth(), swapchain.GetHeight())),
        cmd::BindDescriptorSets(
            renderPass.GetPipelineLayouts()[0], PipelineBindPoint::Graphics, 0, {descriptorSets[imageIndex]}
        )
    );
    presentImageIndex = imageIndex;
}

void Renderer::Draw(const Mesh &mesh, const Material &material, const vg::Buffer &instanceBuffer, int instanceCount) {
    int s = renderMeshes.size();
    int s1 = renderMeshes[material.variant].size();
    renderMeshes[material.index][material.variant].push_back(&mesh);
    instanceBuffers[material.index][material.variant].push_back(&instanceBuffer);
    Renderer::instanceCount[material.index][material.variant].push_back(instanceCount);
}

void Renderer::Draw(const Mesh &mesh, const Material &material) {
    renderMeshes[material.index][material.variant].push_back(&mesh);
    instanceBuffers[material.index][material.variant].push_back(nullptr);
    instanceCount[material.index][material.variant].push_back(1);
}

void Renderer::EndFrame() {
    for (int i = 0; i < Material::subpasses.size(); i++) {
        commandBuffer[frameIndex].Append(cmd::BindPipeline(renderPass.GetPipelines()[i]));
        int variantCount = std::max(1U, Material::materialBuffer.sizes[i] / Material::materialBuffer.alignments[i]);
        for (int j = 0; j < variantCount; j++) {
            if (renderMeshes[i][j].size() == 0) continue;
            if (Material::materialBuffer.sizes[i] != 0)
                commandBuffer[frameIndex].Append(
                    cmd::PushConstants(
                        renderPass.GetPipelineLayouts()[0], ShaderStage::Vertex, 0,
                        Material::materialBuffer.offsets[i] / Material::materialBuffer.alignments[i] + j
                    )
                );

            for (auto &&mesh : renderMeshes[i][j]) {
                auto d = mesh->GetMeshMetaData();
                int k = &mesh - &renderMeshes[i][j][0];
                if (instanceBuffers[i][j][k])
                    commandBuffer[frameIndex].Append(cmd::BindVertexBuffers(*instanceBuffers[i][j][k], 0, 1));
                commandBuffer[frameIndex].Append(
                    cmd::DrawIndexed(d.indexCount, instanceCount[i][j][k], d.firstIndex, d.vertexOffset)
                );
            }
        }
        if (i < Material::subpasses.size() - 1)
            commandBuffer[frameIndex].Append(cmd::NextSubpass(SubpassContents::Inline));
    }

    commandBuffer[frameIndex]
        .Append(cmd::EndRenderpass())
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
std::vector<std::vector<std::vector<const Mesh *>>> Renderer::renderMeshes;
std::vector<std::vector<std::vector<const vg::Buffer *>>> Renderer::instanceBuffers;
std::vector<std::vector<std::vector<int>>> Renderer::instanceCount;
vg::Buffer Renderer::passBuffer;
