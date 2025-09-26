#include "Renderer.h"
#include "GPURenderSystem.h"
using namespace vg;

Renderer *currentRenderer;

Renderer::Renderer() {}

Renderer::Renderer(const vg::Queue *queue, vg::SurfaceHandle windowSurface, int width, int height)
    : frameIndex(0), presentImageIndex(0), totalObjects(0) {
    surface = Surface(windowSurface, {Format::BGRA8SRGB, ColorSpace::SRGBNL});
    swapchain = Swapchain(surface, MAX_FRAMES_IN_FLIGHT, width, height);
    depthImage = Image(
        {swapchain.GetWidth(), swapchain.GetHeight()},
        {Format::D32SFLOAT, Format::D32SFLOATS8UINT, Format::x8D24UNORMPACK}, {FormatFeature::DepthStencilAttachment},
        {ImageUsage::DepthStencilAttachment}, 1, 1
    );
    Allocate(depthImage, {MemoryProperty::DeviceLocal});
    depthImageView = ImageView(depthImage, {ImageAspect::Depth});

    shadowImage = Image(
        {8192, 8192}, {Format::D32SFLOAT, Format::D32SFLOATS8UINT, Format::x8D24UNORMPACK},
        {FormatFeature::DepthStencilAttachment}, {ImageUsage::DepthStencilAttachment, ImageUsage::Sampled}, 1, 1
    );
    Allocate(shadowImage, {MemoryProperty::DeviceLocal});
    shadowImageView = ImageView(shadowImage, {ImageAspect::Depth});
    shadowSampler = Sampler(
        Filter::Linear, Filter::Cubic, SamplerMipmapMode::Nearest, SamplerAddressMode::ClampToEdge,
        SamplerAddressMode::ClampToEdge, SamplerAddressMode::ClampToEdge
    );

    descriptorPool = DescriptorPool(
        MAX_FRAMES_IN_FLIGHT, {{DescriptorType::UniformBuffer, MAX_FRAMES_IN_FLIGHT},
                               {DescriptorType::StorageBuffer, MAX_FRAMES_IN_FLIGHT * 3},
                               {DescriptorType::CombinedImageSampler, MAX_FRAMES_IN_FLIGHT}}
    );

    commandBuffer = std::vector<CmdBuffer>(MAX_FRAMES_IN_FLIGHT);
    renderFinishedSemaphore = std::vector<Semaphore>(swapchain.GetImageCount()),
    imageAvailableSemaphore = std::vector<Semaphore>(MAX_FRAMES_IN_FLIGHT);
    inFlightFence = std::vector<Fence>(MAX_FRAMES_IN_FLIGHT);

    for (int i = 0; i < renderFinishedSemaphore.size(); i++) {
        renderFinishedSemaphore[i] = Semaphore();
        for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
            commandBuffer[i] = CmdBuffer(*queue);
            imageAvailableSemaphore[i] = Semaphore();
            inFlightFence[i] = Fence(true);
        }

        materialBuffer = RenderBuffer(MAX_FRAMES_IN_FLIGHT, vg::BufferUsage::StorageBuffer, 0);
        vertexBuffer = RenderBuffer(MAX_FRAMES_IN_FLIGHT, vg::BufferUsage::VertexBuffer, 0);
        indexBuffer = RenderBuffer(MAX_FRAMES_IN_FLIGHT, vg::BufferUsage::IndexBuffer, 0);
        meshDataBuffer = RenderBuffer(MAX_FRAMES_IN_FLIGHT, vg::BufferUsage::StorageBuffer, 0);
        instanceMappingBuffer =
            RenderBuffer(MAX_FRAMES_IN_FLIGHT, {vg::BufferUsage::StorageBuffer, vg::BufferUsage::VertexBuffer}, 0);
        drawCallBuffer =
            RenderBuffer(MAX_FRAMES_IN_FLIGHT, {vg::BufferUsage::StorageBuffer, vg::BufferUsage::IndirectBuffer}, 0);
        objectBuffer = RenderBuffer(MAX_FRAMES_IN_FLIGHT, vg::BufferUsage::StorageBuffer, 0);
        passBuffer = vg::Buffer(sizeof(PassData), vg::BufferUsage::UniformBuffer);
        vg::Allocate(passBuffer, vg::MemoryProperty::HostVisible);

        gpuRenderSystem = GPURenderSystem(MAX_FRAMES_IN_FLIGHT);
    }
}

Renderer::~Renderer() {}

void Renderer::SetPassData(const PassData &data) {
    void *p = passBuffer.MapMemory();
    memcpy(p, &data, sizeof(data));
}

void Renderer::RenderFrame(const PassData &data) {
    if (batches.size() == 0) return;

    inFlightFence[frameIndex].Await(true);

    auto [imageIndex, result] = swapchain.GetNextImageIndex(imageAvailableSemaphore[frameIndex]);
    presentImageIndex = imageIndex;
    if (materialBuffer.FlushBuffer(frameIndex))
        descriptorSets[frameIndex].AttachBuffer(
            DescriptorType::StorageBuffer, materialBuffer.GetBuffer(frameIndex), 0, -1, 1, 0
        );

    vertexBuffer.FlushBuffer(frameIndex);
    indexBuffer.FlushBuffer(frameIndex);
    meshDataBuffer.FlushBuffer(frameIndex);
    objectBuffer.FlushBuffer(frameIndex);
    instanceMappingBuffer.FlushBuffer(frameIndex);
    drawCallBuffer.FlushBuffer(frameIndex);
    materialBuffer.FlushBuffer(frameIndex);

    descriptorSets[frameIndex].AttachBuffer(
        DescriptorType::StorageBuffer, materialBuffer.GetBuffer(frameIndex), 0, -1, 1, 0
    );
    descriptorSets[frameIndex].AttachBuffer(
        DescriptorType::StorageBuffer, objectBuffer.GetBuffer(frameIndex), 0, -1, 2, 0
    );
    descriptorSets[frameIndex].AttachBuffer(
        DescriptorType::StorageBuffer, drawCallBuffer.GetBuffer(frameIndex), 0, -1, 3, 0
    );

    commandBuffer[frameIndex].Clear().Begin();

    // Shadow pass
    auto passData = data;
    passData.cameraViewProjection = data.lightViewProjection;
    SetPassData(passData);
    gpuRenderSystem.AttachBuffers(
        frameIndex, passBuffer, meshDataBuffer.GetBuffer(frameIndex), objectBuffer.GetBuffer(frameIndex),
        drawCallBuffer.GetBuffer(frameIndex), instanceMappingBuffer.GetBuffer(frameIndex)
    );
    gpuRenderSystem.RecordCommands(commandBuffer[frameIndex], frameIndex);

    commandBuffer[frameIndex].Append(
        cmd::PipelineBarier(
            PipelineStage::ComputeShader, PipelineStage::VertexShader, Dependency::ByRegion,
            {MemoryBarrier(Access::MemoryWrite, Access::MemoryRead)}
        ),
        cmd::BeginRenderpass(
            shadowPass, shadowFramebuffer, {0, 0}, {8192, 8192}, {ClearDepthStencil{1.0f, 0U}}, SubpassContents::Inline
        ),
        cmd::BindVertexBuffers(
            {
                (vg::BufferHandle)vertexBuffer.GetBuffer(frameIndex),
                (vg::BufferHandle)instanceMappingBuffer.GetBuffer(frameIndex),
            },
            {0, 0}
        ),
        cmd::BindIndexBuffer(indexBuffer.GetBuffer(frameIndex), 0, IndexType::Uint32),
        cmd::SetViewport(Viewport(8192, 8192)), cmd::SetScissor(Scissor(8192, 8192)),
        cmd::BindDescriptorSets(
            shadowPass.GetPipelineLayouts()[0], PipelineBindPoint::Graphics, 0, {descriptorSets[frameIndex]}
        )
    );
    int batchMaterialOffset = 0;
    for (int i = 0; i < subpasses.size(); i++) {
        if (i != 0) commandBuffer[frameIndex].Append(cmd::NextSubpass(SubpassContents::Inline));

        if (batchMaterialOffset >= batches.size() || std::get<0>(materialIndices[batchMaterialOffset]) != i) continue;

        int batchMaterialCount = 1;
        for (int j = batchMaterialOffset + 1; j < batches.size(); j++) {
            if (std::get<0>(materialIndices[j]) == i) batchMaterialCount++;
            else break;
        }
        commandBuffer[frameIndex].Append(
            cmd::BindPipeline(shadowPass.GetPipelines()[i]),
            cmd::DrawIndexedIndirect(
                drawCallBuffer.GetBuffer(frameIndex), sizeof(Batch) * batchMaterialOffset, batchMaterialCount,
                sizeof(Batch)
            )
        );
        batchMaterialOffset += batchMaterialCount;
    }

    commandBuffer[frameIndex]
        .Append(
            cmd::EndRenderpass(),

            cmd::PipelineBarier(
                PipelineStage::LateFragmentTests, PipelineStage::FragmentShader, Dependency::ByRegion,
                {ImageMemoryBarrier(
                    shadowImage, ImageLayout::DepthStencilAttachmentOptimal, ImageLayout::DepthStencilReadOnlyOptimal,
                    Access::DepthStencilAttachmentWrite, Access::ShaderRead, ImageSubresource(ImageAspect::Depth)
                )}
            )
        )
        .End()
        .Submit()
        .Await();

    // Color pass
    SetPassData(data);
    descriptorSets[frameIndex].AttachImage(
        DescriptorType::CombinedImageSampler, ImageLayout::DepthStencilAttachmentOptimal, shadowImageView,
        shadowSampler, 4, 0
    );
    commandBuffer[frameIndex].Clear().Begin();
    gpuRenderSystem.AttachBuffers(
        frameIndex, passBuffer, meshDataBuffer.GetBuffer(frameIndex), objectBuffer.GetBuffer(frameIndex),
        drawCallBuffer.GetBuffer(frameIndex), instanceMappingBuffer.GetBuffer(frameIndex)
    );
    gpuRenderSystem.RecordCommands(commandBuffer[frameIndex], frameIndex);

    commandBuffer[frameIndex].Append(
        cmd::PipelineBarier(
            PipelineStage::ComputeShader, PipelineStage::VertexShader, Dependency::ByRegion,
            {MemoryBarrier(Access::MemoryWrite, Access::MemoryRead)}
        ),
        cmd::BeginRenderpass(
            renderPass, framebuffers[imageIndex], {0, 0}, {swapchain.GetWidth(), swapchain.GetHeight()},
            {ClearColor{0, 0, 0, 255}, ClearDepthStencil{1.0f, 0U}}, SubpassContents::Inline
        ),
        cmd::BindVertexBuffers(
            {
                (vg::BufferHandle)vertexBuffer.GetBuffer(frameIndex),
                (vg::BufferHandle)instanceMappingBuffer.GetBuffer(frameIndex),
            },
            {0, 0}
        ),
        cmd::BindIndexBuffer(indexBuffer.GetBuffer(frameIndex), 0, IndexType::Uint32),
        cmd::SetViewport(Viewport(swapchain.GetWidth(), swapchain.GetHeight())),
        cmd::SetScissor(Scissor(swapchain.GetWidth(), swapchain.GetHeight())),
        cmd::BindDescriptorSets(
            renderPass.GetPipelineLayouts()[0], PipelineBindPoint::Graphics, 0, {descriptorSets[frameIndex]}
        )
    );
    batchMaterialOffset = 0;
    for (int i = 0; i < subpasses.size(); i++) {
        if (i != 0) commandBuffer[frameIndex].Append(cmd::NextSubpass(SubpassContents::Inline));

        if (batchMaterialOffset >= batches.size() || std::get<0>(materialIndices[batchMaterialOffset]) != i) continue;

        int batchMaterialCount = 1;
        for (int j = batchMaterialOffset + 1; j < batches.size(); j++) {
            if (std::get<0>(materialIndices[j]) == i) batchMaterialCount++;
            else break;
        }
        commandBuffer[frameIndex].Append(
            cmd::BindPipeline(renderPass.GetPipelines()[i]),
            cmd::DrawIndexedIndirect(
                drawCallBuffer.GetBuffer(frameIndex), sizeof(Batch) * batchMaterialOffset, batchMaterialCount,
                sizeof(Batch)
            )
        );
        batchMaterialOffset += batchMaterialCount;
    }

    commandBuffer[frameIndex]
        .Append(cmd::EndRenderpass())
        .End()
        .Submit(
            {{PipelineStage::ColorAttachmentOutput, imageAvailableSemaphore[frameIndex]}},
            {renderFinishedSemaphore[presentImageIndex]}, inFlightFence[frameIndex]
        );
}
void Renderer::Present(vg::Queue &queue) {
    queue.Present({renderFinishedSemaphore[presentImageIndex]}, {swapchain}, {(unsigned int)presentImageIndex});

    frameIndex = (frameIndex + 1) % MAX_FRAMES_IN_FLIGHT;
}

void Renderer::RecreateRenderpass() {
    if (subpasses.size() == 0 || swapchain.GetImageCount() == 0) return;
    vg::currentDevice->WaitUntilIdle();
    renderPass = RenderPass(
        {Attachment(surface.GetFormat(), ImageLayout::PresentSrc),
         Attachment(depthImage.GetFormat(), ImageLayout::DepthStencilAttachmentOptimal)},
        Vector<PipelineLayout>(PipelineLayout(
            {{vg::DescriptorSetLayoutBinding(
                  0, vg::DescriptorType::UniformBuffer, 1, {vg::ShaderStage::Vertex, vg::ShaderStage::Fragment}
              ),
              vg::DescriptorSetLayoutBinding(1, vg::DescriptorType::StorageBuffer, 1, vg::ShaderStage::Vertex),
              vg::DescriptorSetLayoutBinding(2, vg::DescriptorType::StorageBuffer, 1, vg::ShaderStage::Vertex),
              vg::DescriptorSetLayoutBinding(3, vg::DescriptorType::StorageBuffer, 1, vg::ShaderStage::Vertex),
              vg::DescriptorSetLayoutBinding(
                  4, vg::DescriptorType::CombinedImageSampler, 1, vg::ShaderStage::Fragment
              )}},
            {}
        )),
        subpasses, dependecies
    );

    for (int i = 0; i < subpasses.size(); i++) {
        subpasses[i].colorAttachments = {};
        subpasses[i].depthStencilAttachment->index = 0;
        subpasses[i].graphicsPipeline.shaders.pop_back();
    }
    shadowPass = RenderPass(
        {Attachment(shadowImage.GetFormat(), ImageLayout::DepthStencilAttachmentOptimal)},
        Vector<PipelineLayout>(PipelineLayout(
            {{vg::DescriptorSetLayoutBinding(
                  0, vg::DescriptorType::UniformBuffer, 1, {vg::ShaderStage::Vertex, vg::ShaderStage::Fragment}
              ),
              vg::DescriptorSetLayoutBinding(1, vg::DescriptorType::StorageBuffer, 1, vg::ShaderStage::Vertex),
              vg::DescriptorSetLayoutBinding(2, vg::DescriptorType::StorageBuffer, 1, vg::ShaderStage::Vertex),
              vg::DescriptorSetLayoutBinding(3, vg::DescriptorType::StorageBuffer, 1, vg::ShaderStage::Vertex),
              vg::DescriptorSetLayoutBinding(
                  4, vg::DescriptorType::CombinedImageSampler, 1, vg::ShaderStage::Fragment
              )}},
            {}
        )),
        subpasses, dependecies
    );
    for (int i = 0; i < subpasses.size(); i++) {
        subpasses[i].colorAttachments = {vg::AttachmentReference(0, vg::ImageLayout::ColorAttachmentOptimal)};
        subpasses[i].depthStencilAttachment->index = 1;
        subpasses[i].graphicsPipeline.shaders.resize(2);
        subpasses[i].graphicsPipeline.shaders[1] = &subpasses[i].graphicsPipeline.shaders_[1];
    }
    // Create and allocate descriptor set layouts.
    std::vector<vg::DescriptorSetLayoutHandle> layouts(
        MAX_FRAMES_IN_FLIGHT, renderPass.GetPipelineLayouts()[0].GetDescriptorSets()[0]
    );

    auto newDescriptorPool = DescriptorPool(
        MAX_FRAMES_IN_FLIGHT, {{DescriptorType::UniformBuffer, MAX_FRAMES_IN_FLIGHT},
                               {DescriptorType::StorageBuffer, MAX_FRAMES_IN_FLIGHT * 3},
                               {DescriptorType::CombinedImageSampler, MAX_FRAMES_IN_FLIGHT}}
    );
    descriptorSets = newDescriptorPool.Allocate(layouts);
    std::swap(descriptorPool, newDescriptorPool);

    for (size_t i = 0; i < descriptorSets.size(); i++) {
        descriptorSets[i].AttachBuffer(DescriptorType::UniformBuffer, passBuffer, 0, -1, 0, 0);
        if (materialBuffer.GetBuffer(i) != vg::BufferHandle())
            descriptorSets[i].AttachBuffer(DescriptorType::StorageBuffer, materialBuffer.GetBuffer(i), 0, -1, 1, 0);
    }

    framebuffers.resize(swapchain.GetImageCount());
    for (int i = 0; i < swapchain.GetImageCount(); i++)
        framebuffers[i] = Framebuffer(
            renderPass, {swapchain.GetImageViews()[i], depthImageView}, swapchain.GetWidth(), swapchain.GetHeight()
        );

    shadowFramebuffer = vg::Framebuffer(shadowPass, {shadowImageView}, 8192, 8192);
}
