#include "Renderer.h"
#include "GPURenderSystem.h"
using namespace vg;

// TODO: Shadow pass, merge vertex shaders that do not differ.

Renderer *currentRenderer;

Renderer::Renderer() {}

Renderer::Renderer(
    uint maxFramesInFlight, const vg::Queue *queue, vg::SurfaceHandle windowSurface, int width, int height,
    const Managers &managers
)
    : maxFramesInFlight(maxFramesInFlight), frameIndex(0), managers(managers) {
    surface = Surface(windowSurface, {Format::BGRA8SRGB, ColorSpace::SRGBNL});
    swapchain = Swapchain(surface, maxFramesInFlight, width, height);
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
        Filter::Linear, Filter::Linear, SamplerMipmapMode::Nearest, SamplerAddressMode::ClampToEdge,
        SamplerAddressMode::ClampToEdge, SamplerAddressMode::ClampToEdge
    );

    descriptorPool = DescriptorPool(
        maxFramesInFlight, {{DescriptorType::UniformBuffer, maxFramesInFlight},
                            {DescriptorType::StorageBuffer, maxFramesInFlight * 3},
                            {DescriptorType::CombinedImageSampler, maxFramesInFlight}}
    );

    commandBuffer = std::vector<CmdBuffer>(maxFramesInFlight);
    renderFinishedSemaphore = std::vector<Semaphore>(swapchain.GetImageCount()),
    imageAvailableSemaphore = std::vector<Semaphore>(maxFramesInFlight);
    inFlightFence = std::vector<Fence>(maxFramesInFlight);

    for (int i = 0; i < renderFinishedSemaphore.size(); i++) {
        renderFinishedSemaphore[i] = Semaphore();
        for (int i = 0; i < maxFramesInFlight; i++) {
            commandBuffer[i] = CmdBuffer(*queue);
            imageAvailableSemaphore[i] = Semaphore();
            inFlightFence[i] = Fence(true);
        }
        passBuffer = vg::Buffer(sizeof(PassData), vg::BufferUsage::UniformBuffer);
        vg::Allocate(passBuffer, vg::MemoryProperty::HostVisible);

        gpuRenderSystem = GPURenderSystem(maxFramesInFlight);
    }
}

Renderer::~Renderer() {}

void Renderer::SetPassData(const PassData &data) {
    void *p = passBuffer.MapMemory();
    memcpy(p, &data, sizeof(data));
}

void Renderer::RenderFrame(Queue &queue, const glm::mat4 &cameraViewProjection, const PassData &data) {
    auto &bManager = *managers.batchManager;
    auto &matManager = *managers.materialManager;
    auto &meshManager = *managers.meshManager;

    auto &materialBuffer = matManager.materialBuffer;
    auto &vertexBuffer = meshManager.vertexBuffer;
    auto &indexBuffer = meshManager.indexBuffer;
    auto &meshDataBuffer = meshManager.meshDataBuffer;
    auto &objectBuffer = bManager.objectBuffer;
    auto &instanceMappingBuffer = bManager.instanceMappingBuffer;
    auto &drawCallBuffer = bManager.drawCallBuffer;
    auto &batchBuffer = bManager.batchBuffer;

    if (bManager.batches.size() == 0) return;

    inFlightFence[frameIndex].Await(true);

    auto [imageIndex, result] = swapchain.GetNextImageIndex(imageAvailableSemaphore[frameIndex]);
    SetPassData(data);

    cmd::CopyBuffer copyComands[9] = {cmd::CopyBuffer(), cmd::CopyBuffer(), cmd::CopyBuffer(),
                                      cmd::CopyBuffer(), cmd::CopyBuffer(), cmd::CopyBuffer(),
                                      cmd::CopyBuffer(), cmd::CopyBuffer(), cmd::CopyBuffer()};
    bool anyNeedsReataching = false;
    anyNeedsReataching |= materialBuffer.FlushBuffer(frameIndex, &copyComands[0]);
    anyNeedsReataching |= vertexBuffer.FlushBuffer(frameIndex, &copyComands[1]);
    anyNeedsReataching |= indexBuffer.FlushBuffer(frameIndex, &copyComands[2]);
    anyNeedsReataching |= meshDataBuffer.FlushBuffer(frameIndex, &copyComands[3]);
    anyNeedsReataching |= objectBuffer.FlushBuffer(frameIndex, &copyComands[4]);
    anyNeedsReataching |= instanceMappingBuffer.FlushBuffer(frameIndex, &copyComands[5]);
    anyNeedsReataching |= drawCallBuffer.FlushBuffer(frameIndex, &copyComands[6]);
    anyNeedsReataching |= batchBuffer.FlushBuffer(frameIndex, &copyComands[7]);
    anyNeedsReataching |= materialBuffer.FlushBuffer(frameIndex, &copyComands[8]);

    if (anyNeedsReataching) {
        descriptorSets[frameIndex].AttachBuffer(
            DescriptorType::StorageBuffer, materialBuffer.GetBuffer(frameIndex), 0, -1, 1, 0
        );
        descriptorSets[frameIndex].AttachBuffer(
            DescriptorType::StorageBuffer, materialBuffer.GetBuffer(frameIndex), 0, -1, 1, 0
        );
        descriptorSets[frameIndex].AttachBuffer(
            DescriptorType::StorageBuffer, objectBuffer.GetBuffer(frameIndex), 0, -1, 2, 0
        );
        descriptorSets[frameIndex].AttachBuffer(
            DescriptorType::StorageBuffer, drawCallBuffer.GetBuffer(frameIndex), 0, -1, 3, 0
        );
        gpuRenderSystem.AttachBuffers(
            frameIndex, passBuffer, meshDataBuffer.GetBuffer(frameIndex), objectBuffer.GetBuffer(frameIndex),
            batchBuffer.GetBuffer(frameIndex), drawCallBuffer.GetBuffer(frameIndex),
            instanceMappingBuffer.GetBuffer(frameIndex)
        );
    }

    // Shadow pass
    commandBuffer[frameIndex].Clear().Begin();

    vg::CmdBuffer transferCommands(queue);
    transferCommands.Begin();

    for (int i = 0; i < 8; i++)
        if (copyComands[i].regions.size() != 0) transferCommands.Append(std::move(copyComands[i]));
    transferCommands.Append(
        cmd::PipelineBarier(
            PipelineStage::Transfer, PipelineStage::ComputeShader, Dependency::ByRegion,
            {MemoryBarrier(Access::MemoryWrite, Access::MemoryRead)}
        )
    );
    transferCommands.End().Submit().Await();

    gpuRenderSystem.RecordCommands(commandBuffer[frameIndex], data.lightViewProjection, frameIndex);

    commandBuffer[frameIndex].Append(
        cmd::PipelineBarier(
            PipelineStage::ComputeShader, PipelineStage::VertexShader, Dependency::ByRegion,
            {MemoryBarrier(Access::MemoryWrite, Access::MemoryRead)}
        ),
        cmd::BeginRenderpass(
            shadowPass, shadowFramebuffer, {0, 0}, {8192, 8192}, {ClearDepthStencil{1.0f, 0U}}, SubpassContents::Inline
        ),
        cmd::PushConstants(
            shadowPass.GetPipelineLayouts()[0], ShaderStage::Vertex, 0, std::make_tuple(0, data.lightViewProjection)
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
        ),
        cmd::BindPipeline(shadowPass.GetPipelines()[0])
    );

    for (int i = 0; i < bManager.drawCalls.size(); i++) {
        if (i != 0 &&
            std::get<0>(bManager.drawCallMaterialIndices[i - 1]) != std::get<0>(bManager.drawCallMaterialIndices[i])) {
            commandBuffer[frameIndex].Append(
                cmd::NextSubpass(SubpassContents::Inline),
                cmd::BindPipeline(shadowPass.GetPipelines()[std::get<0>(bManager.drawCallMaterialIndices[i])])
            );
        }

        commandBuffer[frameIndex].Append(
            cmd::PushConstants(shadowPass.GetPipelineLayouts()[0], ShaderStage::Vertex, sizeof(glm::mat4), i),
            cmd::DrawIndexedIndirect(
                drawCallBuffer.GetBuffer(frameIndex), sizeof(BatchManager::DrawCall) * i, 1,
                sizeof(BatchManager::DrawCall)
            )
        );
    }

    // int batchMaterialOffset = 0;
    // for (int i = 0; i < matManager.subpasses.size(); i++) {
    //     if (i != 0) commandBuffer[frameIndex].Append(cmd::NextSubpass(SubpassContents::Inline));

    //     if (batchMaterialOffset >= bManager.batches.size() ||
    //         std::get<0>(bManager.drawCallMaterialIndices[batchMaterialOffset]) != i)
    //         continue;

    //     int batchMaterialCount = 1;
    //     for (int j = batchMaterialOffset + 1; j < bManager.batches.size(); j++) {
    //         if (std::get<0>(bManager.drawCallMaterialIndices[j]) == i) batchMaterialCount++;
    //         else break;
    //     }
    //     commandBuffer[frameIndex].Append(
    //         cmd::BindPipeline(shadowPass.GetPipelines()[i]),
    //         cmd::DrawIndexedIndirect(
    //             drawCallBuffer.GetBuffer(frameIndex), sizeof(BatchManager::DrawCall) * batchMaterialOffset,
    //             batchMaterialCount, sizeof(BatchManager::DrawCall)
    //         )
    //     );
    //     batchMaterialOffset += batchMaterialCount;
    // }

    commandBuffer[frameIndex].Append(
        cmd::EndRenderpass(),

        cmd::PipelineBarier(
            PipelineStage::LateFragmentTests, PipelineStage::FragmentShader, Dependency::ByRegion,
            {ImageMemoryBarrier(
                shadowImage, ImageLayout::DepthStencilAttachmentOptimal, ImageLayout::DepthStencilReadOnlyOptimal,
                Access::DepthStencilAttachmentWrite, Access::ShaderRead, ImageSubresource(ImageAspect::Depth)
            )}
        )
    );

    // Color pass
    gpuRenderSystem.RecordCommands(commandBuffer[frameIndex], cameraViewProjection, frameIndex);
    commandBuffer[frameIndex].Append(
        cmd::PipelineBarier(
            PipelineStage::ComputeShader, PipelineStage::VertexShader, Dependency::ByRegion,
            {MemoryBarrier(Access::MemoryWrite, Access::MemoryRead)}
        ),
        cmd::BeginRenderpass(
            renderPass, framebuffers[imageIndex], {0, 0}, {swapchain.GetWidth(), swapchain.GetHeight()},
            {ClearColor{0, 0, 0, 255}, ClearDepthStencil{1.0f, 0U}}, SubpassContents::Inline
        ),
        cmd::PushConstants(
            shadowPass.GetPipelineLayouts()[0], ShaderStage::Vertex, 0, std::make_tuple(0, cameraViewProjection)
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
        ),
        cmd::BindPipeline(renderPass.GetPipelines()[0])
    );

    for (int i = 0; i < bManager.drawCalls.size(); i++) {
        if (i != 0 &&
            std::get<0>(bManager.drawCallMaterialIndices[i - 1]) != std::get<0>(bManager.drawCallMaterialIndices[i])) {
            commandBuffer[frameIndex].Append(
                cmd::NextSubpass(SubpassContents::Inline),
                cmd::BindPipeline(renderPass.GetPipelines()[std::get<0>(bManager.drawCallMaterialIndices[i])])
            );
        }

        commandBuffer[frameIndex].Append(
            cmd::PushConstants(renderPass.GetPipelineLayouts()[0], ShaderStage::Vertex, sizeof(glm::mat4), i),
            cmd::DrawIndexedIndirect(
                drawCallBuffer.GetBuffer(frameIndex), sizeof(BatchManager::DrawCall) * i, 1,
                sizeof(BatchManager::DrawCall)
            )
        );
    }

    // batchMaterialOffset = 0;
    // for (int i = 0; i < matManager.subpasses.size(); i++) {
    //     if (i != 0) commandBuffer[frameIndex].Append(cmd::NextSubpass(SubpassContents::Inline));

    //     if (batchMaterialOffset >= bManager.batches.size() ||
    //         std::get<0>(bManager.drawCallMaterialIndices[batchMaterialOffset]) != i)
    //         continue;

    //     int batchMaterialCount = 1;
    //     for (int j = batchMaterialOffset + 1; j < bManager.batches.size(); j++) {
    //         if (std::get<0>(bManager.drawCallMaterialIndices[j]) == i) batchMaterialCount++;
    //         else break;
    //     }
    //     commandBuffer[frameIndex].Append(
    //         cmd::BindPipeline(renderPass.GetPipelines()[i]),
    //         cmd::DrawIndexedIndirect(
    //             drawCallBuffer.GetBuffer(frameIndex), sizeof(BatchManager::Batch) * batchMaterialOffset,
    //             batchMaterialCount, sizeof(BatchManager::Batch)
    //         )
    //     );
    //     batchMaterialOffset += batchMaterialCount;
    // }

    commandBuffer[frameIndex]
        .Append(cmd::EndRenderpass())
        .End()
        .Submit(
            {{PipelineStage::ColorAttachmentOutput, imageAvailableSemaphore[frameIndex]}},
            {renderFinishedSemaphore[imageIndex]}, inFlightFence[frameIndex]
        );

    queue.Present({renderFinishedSemaphore[imageIndex]}, {swapchain}, {(unsigned int)imageIndex});
    frameIndex = (frameIndex + 1) % maxFramesInFlight;
}

void Renderer::RecreateRenderpass() {
    auto &matManager = *managers.materialManager;

    if (matManager.subpasses.size() == 0 || swapchain.GetImageCount() == 0) return;

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
            {{ShaderStage::Vertex, 0, sizeof(glm::mat4) + sizeof(uint)}}
        )),
        matManager.subpasses, matManager.dependecies
    );

    for (int i = 0; i < matManager.subpasses.size(); i++) {
        matManager.subpasses[i].colorAttachments = {};
        matManager.subpasses[i].depthStencilAttachment->index = 0;
        matManager.subpasses[i].graphicsPipeline.shaders.pop_back();
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
            {{ShaderStage::Vertex, 0, sizeof(glm::mat4) + sizeof(uint)}}

        )),
        matManager.subpasses, matManager.dependecies
    );
    for (int i = 0; i < matManager.subpasses.size(); i++) {
        matManager.subpasses[i].colorAttachments = {
            vg::AttachmentReference(0, vg::ImageLayout::ColorAttachmentOptimal)
        };
        matManager.subpasses[i].depthStencilAttachment->index = 1;
        matManager.subpasses[i].graphicsPipeline.shaders.resize(2);
        matManager.subpasses[i].graphicsPipeline.shaders[1] = &matManager.subpasses[i].graphicsPipeline.shaders_[1];
    }
    // Create and allocate descriptor set layouts.
    std::vector<vg::DescriptorSetLayoutHandle> layouts(
        maxFramesInFlight, renderPass.GetPipelineLayouts()[0].GetDescriptorSets()[0]
    );

    auto newDescriptorPool = DescriptorPool(
        maxFramesInFlight, {{DescriptorType::UniformBuffer, maxFramesInFlight},
                            {DescriptorType::StorageBuffer, maxFramesInFlight * 3},
                            {DescriptorType::CombinedImageSampler, maxFramesInFlight}}
    );
    descriptorSets = newDescriptorPool.Allocate(layouts);
    std::swap(descriptorPool, newDescriptorPool);

    for (size_t i = 0; i < descriptorSets.size(); i++) {
        descriptorSets[i].AttachBuffer(DescriptorType::UniformBuffer, passBuffer, 0, -1, 0, 0);
        if (matManager.materialBuffer.GetBuffer(i) != vg::BufferHandle())
            descriptorSets[i].AttachBuffer(
                DescriptorType::StorageBuffer, matManager.materialBuffer.GetBuffer(i), 0, -1, 1, 0
            );

        descriptorSets[i].AttachImage(
            DescriptorType::CombinedImageSampler, ImageLayout::DepthStencilReadOnlyOptimal, shadowImageView,
            shadowSampler, 4, 0
        );
    }

    framebuffers.resize(swapchain.GetImageCount());
    for (int i = 0; i < swapchain.GetImageCount(); i++)
        framebuffers[i] = Framebuffer(
            renderPass, {swapchain.GetImageViews()[i], depthImageView}, swapchain.GetWidth(), swapchain.GetHeight()
        );

    shadowFramebuffer = vg::Framebuffer(shadowPass, {shadowImageView}, 8192, 8192);
}
