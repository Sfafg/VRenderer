#include "Renderer.h"
#include "GPURenderSystem.h"
using namespace vg;
using namespace cmd;

struct DrawFromBuffer;

Renderer *currentRenderer;

Renderer::Renderer() {}

Renderer::Renderer(
    uint maxFramesInFlight, const vg::Queue *queue, vg::SurfaceHandle windowSurface, int width, int height,
    const DataArrays &managers
)
    : maxFramesInFlight(maxFramesInFlight), frameIndex(0), dataArrays(managers) {

    surface = Surface(windowSurface, {Format::BGRA8SRGB, ColorSpace::SRGBNL});
    swapchain = Swapchain(surface, maxFramesInFlight, width, height);
    depthImage = Image(
        {swapchain.GetWidth(), swapchain.GetHeight()},
        {Format::D32SFLOAT, Format::D32SFLOATS8UINT, Format::x8D24UNORMPACK}, {FormatFeature::DepthStencilAttachment},
        {ImageUsage::DepthStencilAttachment, ImageUsage::Sampled}
    );
    Allocate(depthImage, {MemoryProperty::DeviceLocal});
    depthImageView = ImageView(depthImage, {ImageAspect::Depth});

    hiZBuffers.resize(maxFramesInFlight);
    for (auto &hiZBuffer : hiZBuffers) hiZBuffer = HiZBuffer(width, height);

    gpuRenderers.resize(maxFramesInFlight);
    for (auto &gpuRenderer : gpuRenderers) gpuRenderer = GPURenderer(maxFramesInFlight);

    shadowhiZBuffers.resize(maxFramesInFlight);
    for (auto &hiZBuffer : shadowhiZBuffers) hiZBuffer = HiZBuffer(width, height);

    shadowgpuRenderers.resize(maxFramesInFlight);
    for (auto &gpuRenderer : shadowgpuRenderers) gpuRenderer = GPURenderer(maxFramesInFlight);

    shadowImage = Image(
        {4096, 4096}, {Format::D32SFLOAT, Format::D32SFLOATS8UINT, Format::x8D24UNORMPACK},
        {FormatFeature::DepthStencilAttachment}, {ImageUsage::DepthStencilAttachment, ImageUsage::Sampled}, 1, 1
    );
    Allocate(shadowImage, {MemoryProperty::DeviceLocal});
    shadowImageView = ImageView(shadowImage, {ImageAspect::Depth});
    shadowSampler = Sampler(
        Filter::Linear, Filter::Linear, SamplerMipmapMode::Nearest, SamplerAddressMode::ClampToEdge,
        SamplerAddressMode::ClampToEdge, SamplerAddressMode::ClampToEdge
    );

    descriptorPool = DescriptorPool(
        maxFramesInFlight * 2, {{DescriptorType::UniformBuffer, maxFramesInFlight * 2},
                                {DescriptorType::StorageBuffer, maxFramesInFlight * 3 * 2},
                                {DescriptorType::CombinedImageSampler, maxFramesInFlight * 2}}
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
        lightBuffer = vg::Buffer(sizeof(LightData), vg::BufferUsage::UniformBuffer);
        vg::Allocate(lightBuffer, vg::MemoryProperty::HostVisible);
    }
}

Renderer::~Renderer() {}

void Renderer::MakeCurrent() {
    Material::materialArray = dataArrays.materialArray;
    Mesh::meshArray = dataArrays.meshArray;
    BatchArray::batchArray = dataArrays.batchArray;
    currentRenderer = this;
}

void Renderer::SetLightData(const LightData &data) {
    void *p = lightBuffer.MapMemory();
    memcpy(p, &data, sizeof(data));
}

void Renderer::RenderFrame(
    vg::Queue &queue, const glm::mat4 &cameraViewProjection, const glm::vec3 &cameraPosition, float nearPlane,
    float farPlane, const Renderer::LightData &data, bool updateDrawInstructions
) {
    auto &bManager = *dataArrays.batchArray;
    auto &materialManager = *dataArrays.materialArray;
    auto &meshManager = *dataArrays.meshArray;

    auto &materialBuffer = materialManager.materialBuffer;
    auto &vertexBuffer = meshManager.vertexBuffer;
    auto &indexBuffer = meshManager.indexBuffer;
    auto &meshDataBuffer = meshManager.meshDataBuffer;
    auto &objectBuffer = bManager.objectBuffer;
    auto &drawCallBuffer = bManager.drawCallBuffer;
    auto &batchBuffer = bManager.batchBuffer;

    if (bManager.batches.size() == 0) return;

    inFlightFence[frameIndex].Await(true);

    auto [imageIndex, result] = swapchain.GetNextImageIndex(imageAvailableSemaphore[frameIndex]);
    SetLightData(data);

    CopyBuffer copyComands[9] = {CopyBuffer(), CopyBuffer(), CopyBuffer(), CopyBuffer(), CopyBuffer(),
                                 CopyBuffer(), CopyBuffer(), CopyBuffer(), CopyBuffer()};
    materialBuffer.FlushBuffer(frameIndex, &copyComands[0]);
    vertexBuffer.FlushBuffer(frameIndex, &copyComands[1]);
    indexBuffer.FlushBuffer(frameIndex, &copyComands[2]);
    meshDataBuffer.FlushBuffer(frameIndex, &copyComands[3]);
    objectBuffer.FlushBuffer(frameIndex, &copyComands[4]);
    drawCallBuffer.FlushBuffer(frameIndex, &copyComands[6]);
    batchBuffer.FlushBuffer(frameIndex, &copyComands[7]);
    materialBuffer.FlushBuffer(frameIndex, &copyComands[8]);

    gpuRenderers[frameIndex].UpdateBuffers(BatchArray::GetTotalInstanceCount(), drawCallBuffer.GetBuffer(frameIndex));
    shadowgpuRenderers[frameIndex].UpdateBuffers(
        BatchArray::GetTotalInstanceCount(), drawCallBuffer.GetBuffer(frameIndex)
    );

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
        DescriptorType::StorageBuffer, gpuRenderers[frameIndex].drawCalls, 0, -1, 3, 0
    );
    gpuRenderers[frameIndex].AttachBuffers(
        hiZBuffers[frameIndex].view, hiZBuffers[frameIndex].sampler, meshDataBuffer.GetBuffer(frameIndex),
        objectBuffer.GetBuffer(frameIndex), batchBuffer.GetBuffer(frameIndex), drawCallBuffer.GetBuffer(frameIndex)
    );

    shadowPassDescriptorSets[frameIndex].AttachBuffer(
        DescriptorType::StorageBuffer, materialBuffer.GetBuffer(frameIndex), 0, -1, 1, 0
    );
    shadowPassDescriptorSets[frameIndex].AttachBuffer(
        DescriptorType::StorageBuffer, materialBuffer.GetBuffer(frameIndex), 0, -1, 1, 0
    );
    shadowPassDescriptorSets[frameIndex].AttachBuffer(
        DescriptorType::StorageBuffer, objectBuffer.GetBuffer(frameIndex), 0, -1, 2, 0
    );
    shadowPassDescriptorSets[frameIndex].AttachBuffer(
        DescriptorType::StorageBuffer, shadowgpuRenderers[frameIndex].drawCalls, 0, -1, 3, 0
    );
    shadowgpuRenderers[frameIndex].AttachBuffers(
        shadowhiZBuffers[frameIndex].view, hiZBuffers[frameIndex].sampler, meshDataBuffer.GetBuffer(frameIndex),
        objectBuffer.GetBuffer(frameIndex), batchBuffer.GetBuffer(frameIndex), drawCallBuffer.GetBuffer(frameIndex)
    );

    commandBuffer[frameIndex].Clear().Begin();

    // Data transfer.
    vg::CmdBuffer transferCommands(queue);
    transferCommands.Begin();

    for (int i = 0; i < 8; i++)
        if (copyComands[i].regions.size() != 0) transferCommands.Append(std::move(copyComands[i]));
    transferCommands.End().Submit().Await();

    // Depth prepass.
    commandBuffer[frameIndex].Append(
        BeginRenderpass(
            depthOnlyPass, shadowFramebuffer, {0, 0}, {4096, 4096}, {ClearDepthStencil{1.0f, 0U}},
            SubpassContents::Inline
        ),
        PushConstants(
            depthOnlyPass.GetPipelineLayouts()[0], ShaderStage::Vertex, 0,
            std::make_tuple(0, cameraPosition, data.lightViewProjection)
        ),
        BindMeshBuffers(shadowgpuRenderers[frameIndex].instanceMapping), SetViewport(Viewport(4096, 4096)),
        SetScissor(Scissor(4096, 4096)),
        BindDescriptorSets(
            depthOnlyPass.GetPipelineLayouts()[0], PipelineBindPoint::Graphics, 0,
            {shadowPassDescriptorSets[frameIndex]}
        ),
        DrawFromBuffer(&depthOnlyPass, shadowgpuRenderers[frameIndex].drawCalls), EndRenderpass()
    );

    commandBuffer[frameIndex].Append(
        HiZBuffer::Reduce(shadowhiZBuffers[frameIndex], shadowImageView),
        HiZBuffer::PipelineBarrier(
            shadowhiZBuffers[frameIndex], vg::PipelineStage::ComputeShader, vg::ImageLayout::ShaderReadOnlyOptimal,
            vg::Access::ShaderRead
        )
    );
    // Shadow pass
    if (updateDrawInstructions)
        commandBuffer[frameIndex].Append(
            GPURenderer::WriteInstructions(
                shadowgpuRenderers[frameIndex], farPlane, nearPlane, cameraPosition, data.lightViewProjection
            )
        );
    commandBuffer[frameIndex].Append(
        PipelineBarier(
            PipelineStage::ComputeShader, PipelineStage::VertexShader, Dependency::ByRegion,
            {MemoryBarrier(Access::MemoryWrite, Access::MemoryRead)}
        ),
        BeginRenderpass(
            depthOnlyPass, shadowFramebuffer, {0, 0}, {4096, 4096}, {ClearDepthStencil{1.0f, 0U}},
            SubpassContents::Inline
        ),
        PushConstants(
            depthOnlyPass.GetPipelineLayouts()[0], ShaderStage::Vertex, 0,
            std::make_tuple(0, cameraPosition, data.lightViewProjection)
        ),
        BindMeshBuffers(shadowgpuRenderers[frameIndex].instanceMapping), SetViewport(Viewport(4096, 4096)),
        SetScissor(Scissor(4096, 4096)),
        BindDescriptorSets(
            depthOnlyPass.GetPipelineLayouts()[0], PipelineBindPoint::Graphics, 0,
            {shadowPassDescriptorSets[frameIndex]}
        ),
        DrawFromBuffer(&depthOnlyPass, shadowgpuRenderers[frameIndex].drawCalls), EndRenderpass()
    );

    commandBuffer[frameIndex].Append(PipelineBarier(
        PipelineStage::LateFragmentTests, PipelineStage::FragmentShader, Dependency::ByRegion,
        {ImageMemoryBarrier(
            shadowImage, ImageLayout::DepthStencilReadOnlyOptimal, Access::DepthStencilAttachmentWrite,
            Access::ShaderRead, ImageSubresource(ImageAspect::Depth)
        )}
    ));

    // Depth prepass.
    commandBuffer[frameIndex].Append(
        BeginRenderpass(
            depthOnlyPass, depthPrepassFramebuffer, {0, 0}, {swapchain.GetWidth(), swapchain.GetHeight()},
            {ClearDepthStencil{1.0f, 0U}}, SubpassContents::Inline
        ),
        PushConstants(
            depthOnlyPass.GetPipelineLayouts()[0], ShaderStage::Vertex, 0,
            std::make_tuple(0, cameraPosition, cameraViewProjection)
        ),
        BindMeshBuffers(gpuRenderers[frameIndex].instanceMapping),
        SetViewport(Viewport(swapchain.GetWidth(), swapchain.GetHeight())),
        SetScissor(Scissor(swapchain.GetWidth(), swapchain.GetHeight())),
        BindDescriptorSets(
            depthOnlyPass.GetPipelineLayouts()[0], PipelineBindPoint::Graphics, 0, {descriptorSets[frameIndex]}
        ),
        DrawFromBuffer(&depthOnlyPass, gpuRenderers[frameIndex].drawCalls), EndRenderpass()
    );

    commandBuffer[frameIndex].Append(
        HiZBuffer::Reduce(hiZBuffers[frameIndex], depthImageView),
        HiZBuffer::PipelineBarrier(
            hiZBuffers[frameIndex], vg::PipelineStage::ComputeShader, vg::ImageLayout::ShaderReadOnlyOptimal,
            vg::Access::ShaderRead
        )
    );

    // Color pass
    if (updateDrawInstructions)
        commandBuffer[frameIndex].Append(
            GPURenderer::WriteInstructions(
                gpuRenderers[frameIndex], farPlane, nearPlane, cameraPosition, cameraViewProjection
            )
        );

    commandBuffer[frameIndex]
        .Append(
            GPURenderer::PipelineBarrier(gpuRenderers[frameIndex], PipelineStage::VertexShader, Access::MemoryRead),
            BeginRenderpass(
                renderPass, framebuffers[imageIndex], {0, 0}, {swapchain.GetWidth(), swapchain.GetHeight()},
                {ClearColor{65 / 255.f, 135 / 255.f, 245 / 255.f, 255 / 255.f}, ClearDepthStencil{1.0f, 0U}},
                SubpassContents::Inline
            ),
            PushConstants(
                renderPass.GetPipelineLayouts()[0], ShaderStage::Vertex, 0,
                std::make_tuple(0, cameraPosition, cameraViewProjection)
            ),
            BindMeshBuffers(gpuRenderers[frameIndex].instanceMapping),
            SetViewport(Viewport(swapchain.GetWidth(), swapchain.GetHeight())),
            SetScissor(Scissor(swapchain.GetWidth(), swapchain.GetHeight())),
            BindDescriptorSets(
                renderPass.GetPipelineLayouts()[0], PipelineBindPoint::Graphics, 0, {descriptorSets[frameIndex]}
            ),
            DrawFromBuffer(&renderPass, gpuRenderers[frameIndex].drawCalls), EndRenderpass()
        )
        .End()
        .Submit(
            {{PipelineStage::ColorAttachmentOutput, imageAvailableSemaphore[frameIndex]}},
            {renderFinishedSemaphore[imageIndex]}, inFlightFence[frameIndex]
        );

    queue.Present({renderFinishedSemaphore[imageIndex]}, {swapchain}, {(unsigned int)imageIndex});
    frameIndex = (frameIndex + 1) % maxFramesInFlight;
}

void Renderer::RecreateRenderpass() {
    assert(currentRenderer && "Current currentRenderer needs to be assigned!");
    currentRenderer->_RecreateRenderpass();
}
void Renderer::_RecreateRenderpass() {
    auto &materialManager = *dataArrays.materialArray;

    if (materialManager.subpasses.size() == 0 || swapchain.GetImageCount() == 0) return;

    vg::currentDevice->WaitUntilIdle();
    std::vector<vg::SubpassDependency> dependencies(materialManager.subpasses.size());
    for (int i = 0; i < materialManager.subpasses.size(); i++) {
        dependencies[i] = vg::SubpassDependency(
            i - 1, i, vg::PipelineStage::ColorAttachmentOutput, vg::PipelineStage::ColorAttachmentOutput, 0,
            vg::Access::ColorAttachmentWrite, {}
        );
    }
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
            {{ShaderStage::Vertex, 0, sizeof(glm::mat4) + sizeof(glm::vec3) + sizeof(uint)}}
        )),
        materialManager.subpasses, dependencies
    );

    for (int i = 0; i < materialManager.subpasses.size(); i++) {
        materialManager.subpasses[i].colorAttachments = {};
        materialManager.subpasses[i].depthStencilAttachment->index = 0;
        materialManager.subpasses[i].graphicsPipeline.shaders.pop_back();
    }
    depthOnlyPass = RenderPass(
        {Attachment(shadowImage.GetFormat(), ImageLayout::DepthStencilReadOnlyOptimal)},
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
            {{ShaderStage::Vertex, 0, sizeof(glm::mat4) + sizeof(glm::vec3) + sizeof(uint)}}

        )),
        materialManager.subpasses, dependencies
    );
    for (int i = 0; i < materialManager.subpasses.size(); i++) {
        materialManager.subpasses[i].colorAttachments = {
            vg::AttachmentReference(0, vg::ImageLayout::ColorAttachmentOptimal)
        };
        materialManager.subpasses[i].depthStencilAttachment->index = 1;
        materialManager.subpasses[i].graphicsPipeline.shaders.resize(2);
        materialManager.subpasses[i].graphicsPipeline.shaders[1] =
            &materialManager.subpasses[i].graphicsPipeline.shaders_[1];
    }
    // Create and allocate descriptor set layouts.
    std::vector<vg::DescriptorSetLayoutHandle> layouts(
        maxFramesInFlight, renderPass.GetPipelineLayouts()[0].GetDescriptorSets()[0]
    );

    auto newDescriptorPool = DescriptorPool(
        maxFramesInFlight * 2, {{DescriptorType::UniformBuffer, maxFramesInFlight * 2},
                                {DescriptorType::StorageBuffer, maxFramesInFlight * 3 * 2},
                                {DescriptorType::CombinedImageSampler, maxFramesInFlight * 2}}
    );
    descriptorSets = newDescriptorPool.Allocate(layouts);
    shadowPassDescriptorSets = newDescriptorPool.Allocate(layouts);
    std::swap(descriptorPool, newDescriptorPool);

    for (size_t i = 0; i < descriptorSets.size(); i++) {
        descriptorSets[i].AttachBuffer(DescriptorType::UniformBuffer, lightBuffer, 0, -1, 0, 0);
        shadowPassDescriptorSets[i].AttachBuffer(DescriptorType::UniformBuffer, lightBuffer, 0, -1, 0, 0);
        if (materialManager.materialBuffer.GetBuffer(i) != vg::BufferHandle())
            descriptorSets[i].AttachBuffer(
                DescriptorType::StorageBuffer, materialManager.materialBuffer.GetBuffer(i), 0, -1, 1, 0
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

    depthPrepassFramebuffer =
        vg::Framebuffer(depthOnlyPass, {depthImageView}, swapchain.GetWidth(), swapchain.GetHeight());
    shadowFramebuffer = vg::Framebuffer(depthOnlyPass, {shadowImageView}, 4096, 4096);
}

void Renderer::DrawFromBuffer::operator()(vg::CmdBuffer &cmdBuffer) const {
    auto &bManager = *currentRenderer->dataArrays.batchArray;

    cmdBuffer.Append(BindPipeline(renderPass->GetPipelines()[0]));
    int subpassIndex = 0;
    for (int i = 0; i < bManager.drawCalls.size(); i++) {
        int materialIndex = std::get<0>(bManager.drawCallMaterialIndices[i]);
        for (; subpassIndex < materialIndex; subpassIndex++) {
            cmdBuffer.Append(NextSubpass(SubpassContents::Inline));
            if (subpassIndex == materialIndex - 1)
                cmdBuffer.Append(BindPipeline(renderPass->GetPipelines()[materialIndex]));
        }

        cmdBuffer.Append(
            PushConstants(
                renderPass->GetPipelineLayouts()[0], ShaderStage::Vertex, sizeof(glm::mat4) + sizeof(glm::vec3), i
            ),
            DrawIndexedIndirect(drawBuffer, sizeof(BatchArray::DrawCall) * i, 1, sizeof(BatchArray::DrawCall))
        );
    }
    for (; subpassIndex < currentRenderer->dataArrays.materialArray->subpasses.size() - 1; subpassIndex++)
        cmdBuffer.Append(NextSubpass(SubpassContents::Inline));
}

void Renderer::BindMeshBuffers::operator()(vg::CmdBuffer &cmdBuffer) const {
    auto &meshManager = *currentRenderer->dataArrays.meshArray;
    auto &batchManger = *currentRenderer->dataArrays.batchArray;
    auto frameIndex = currentRenderer->frameIndex;
    cmdBuffer.Append(
        BindVertexBuffers(
            {
                (vg::BufferHandle)meshManager.vertexBuffer.GetBuffer(frameIndex),
                instanceMapping,
            },
            {0, 0}
        ),
        BindIndexBuffer(meshManager.indexBuffer.GetBuffer(frameIndex), 0, IndexType::Uint32)
    );
}
