#include <unordered_map>
#include "GPUDrivenRendererSystem.h"
#include "Settings.h"

std::tuple<vg::Subpass, vg::SubpassDependency> CreateSubpass(Material&& material, int childrenCount)
{
    return std::make_tuple(
        vg::Subpass(
            vg::GraphicsPipeline(
                { {{0,vg::DescriptorType::StorageBuffer,1,vg::ShaderStage::Vertex}} },
                { vg::PushConstantRange({vg::ShaderStage::Vertex, vg::ShaderStage::Compute},0, sizeof(glm::mat4)) },
                std::move(*material.shaders),
                {
                    { {0, sizeof(glm::vec3)}, {1,sizeof(int),vg::InputRate::Instance} },
                    { {0, 0, vg::Format::RGB32SFLOAT}, {1,1,vg::Format::R32UINT} }
                },
                std::move(*material.inputAssembly), vg::Tesselation(), vg::ViewportState(vg::Viewport(), vg::Scissor()), std::move(*material.rasterizer),
                std::move(*material.multisampling), std::move(*material.depthStencil), std::move(*material.colorBlending), std::move(*material.dynamicStates),
                childrenCount
            ),
            {}, { vg::AttachmentReference(0, vg::ImageLayout::ColorAttachmentOptimal) },
            {}, vg::AttachmentReference(1, vg::ImageLayout::DepthStencilAttachmentOptimal)
        ),
        vg::SubpassDependency(0, 1, vg::PipelineStage::ColorAttachmentOutput, vg::PipelineStage::ColorAttachmentOutput, 0, vg::Access::ColorAttachmentWrite, {})
    );
}
std::tuple<vg::Subpass, vg::SubpassDependency> CreateSecondarySubpass(Material&& material, int parentIndex)
{
    return std::make_tuple(vg::Subpass(
        vg::GraphicsPipeline(
            parentIndex, std::nullopt, std::nullopt,
            std::move(*material.shaders), std::nullopt,
            std::move(*material.inputAssembly), std::nullopt, std::nullopt, std::move(*material.rasterizer),
            std::move(*material.multisampling), std::move(*material.depthStencil),
            std::move(*material.colorBlending), std::move(*material.dynamicStates)
        ),
        {}, { vg::AttachmentReference(0, vg::ImageLayout::ColorAttachmentOptimal) },
        {}, vg::AttachmentReference(1, vg::ImageLayout::DepthStencilAttachmentOptimal)
    ),
        vg::SubpassDependency(-1, 0, vg::PipelineStage::ColorAttachmentOutput, vg::PipelineStage::ColorAttachmentOutput, 0, vg::Access::ColorAttachmentWrite, {})
    );
}

GPUDrivenRendererSystem::GPUDrivenRendererSystem() :queue(nullptr), frameIndex(0) {}

GPUDrivenRendererSystem::GPUDrivenRendererSystem(std::span<Material> materials, Span<const vg::Attachment> attachments, uint32_t frameCount, const vg::Queue* queue)
    :queue(queue), frameIndex(0)
{
    std::vector<vg::Subpass> subpasses(materials.size());
    std::vector<vg::SubpassDependency> subpassDependencies(materials.size());

    vg::Shader vertexPass = vg::Shader(vg::ShaderStage::Vertex, "resources/shaders/passthrough.vert.spv");
    subpasses[0] = vg::Subpass(
        vg::GraphicsPipeline(
            { {{0,vg::DescriptorType::StorageBuffer,1,vg::ShaderStage::Vertex}} },
            { vg::PushConstantRange({vg::ShaderStage::Vertex, vg::ShaderStage::Compute},0, sizeof(glm::mat4)) },
            { &vertexPass },
            {
                { {0, sizeof(glm::vec3)}, {1,sizeof(int),vg::InputRate::Instance} },
                { {0, 0, vg::Format::RGB32SFLOAT}, {1,1,vg::Format::R32UINT} }
            },
            vg::InputAssembly(vg::Primitive::Triangles),
            vg::Tesselation(),
            vg::ViewportState(vg::Viewport(), vg::Scissor()),
            vg::Rasterizer(),
            vg::Multisampling(1),
            vg::DepthStencil(true, true, vg::CompareOp::Less),
            vg::ColorBlending(),
            { vg::DynamicState::Viewport,vg::DynamicState::Scissor }
        ),
        {}, {},
        {}, vg::AttachmentReference(1, vg::ImageLayout::DepthStencilAttachmentOptimal)
    );
    subpassDependencies[0] = vg::SubpassDependency(-1, 0, vg::PipelineStage::ColorAttachmentOutput, vg::PipelineStage::ColorAttachmentOutput, 0, vg::Access::DepthStencilAttachmentWrite, {});

    if (subpasses.size() > 1)
        std::tie(subpasses[1], subpassDependencies[1]) = CreateSubpass(std::move(materials[0]), subpasses.size() - 1);
    for (int i = 1; i < materials.size(); i++)
        std::tie(subpasses[i + 1], subpassDependencies[i + 1]) = CreateSecondarySubpass(std::move(materials[i]), 0);

    renderPass = vg::RenderPass(attachments, subpasses, subpassDependencies);

    vg::Shader renderShader(vg::ShaderStage::Compute, "resources/shaders/Renderer.comp.spv");
    computeRenderer = vg::ComputePipeline(
        renderShader,
        {
            {
                 {0,vg::DescriptorType::StorageBuffer,1,vg::ShaderStage::Compute},
                 {1,vg::DescriptorType::StorageBuffer,1,vg::ShaderStage::Compute},
                 {2,vg::DescriptorType::StorageBuffer,1,vg::ShaderStage::Compute},
                 {3,vg::DescriptorType::CombinedImageSampler,1,vg::ShaderStage::Compute}
            }
        },
        {
            vg::PushConstantRange({vg::ShaderStage::Vertex, vg::ShaderStage::Compute},0, sizeof(glm::mat4) * 2)
        }
    );

    descriptorPool = vg::DescriptorPool(36 * frameCount,
        {
            { vg::DescriptorType::StorageBuffer, 4 * frameCount },
            { vg::DescriptorType::CombinedImageSampler, 16 * frameCount },
            { vg::DescriptorType::SampledImage, 16 * frameCount }
        });
    descriptorPool.Allocate(std::vector<vg::DescriptorSetLayoutHandle>(frameCount, computeRenderer.GetPipelineLayout().GetDescriptorSets()[0]), &computeDescriptor);
    descriptorPool.Allocate(std::vector<vg::DescriptorSetLayoutHandle>(frameCount, renderPass.GetPipelineLayouts()[0].GetDescriptorSets()[0]), &renderDescriptor);

    indicesBuffer = BufferArray<uint32_t>(1, { vg::BufferUsage::IndexBuffer });
    verticesBuffer = BufferArray<glm::vec3>(1, { vg::BufferUsage::VertexBuffer });

    objectsBuffer.resize(frameCount);
    instanceBuffer.resize(frameCount);
    mappingBuffer.resize(frameCount);
    instructionsBuffer.resize(frameCount);
    for (int i = 0; i < frameCount; i++)
    {
        objectsBuffer[i] = BufferArray<RenderObject>(1, { vg::BufferUsage::StorageBuffer });
        instanceBuffer[i] = BufferArray<glm::mat4>(1, { vg::BufferUsage::StorageBuffer });
        mappingBuffer[i] = BufferArray<uint32_t>(1, { vg::BufferUsage::StorageBuffer, vg::BufferUsage::VertexBuffer });
        instructionsBuffer[i] = BufferArray<vg::cmd::DrawIndexed>(1, { vg::BufferUsage::StorageBuffer, vg::BufferUsage::IndirectBuffer });
    }
}

GPUDrivenRendererSystem::GPUDrivenRendererSystem(GPUDrivenRendererSystem&& rhs) noexcept
    :GPUDrivenRendererSystem()
{
    *this = std::move(rhs);
}

GPUDrivenRendererSystem& GPUDrivenRendererSystem::operator=(GPUDrivenRendererSystem&& rhs)
{
    if (this != &rhs)
    {
        std::swap(renderPass, rhs.renderPass);
        std::swap(computeRenderer, rhs.computeRenderer);

        std::swap(descriptorPool, rhs.descriptorPool);
        std::swap(computeDescriptor, rhs.computeDescriptor);
        std::swap(renderDescriptor, rhs.renderDescriptor);
        std::swap(queue, rhs.queue);
        std::swap(batchMap, rhs.batchMap);
        std::swap(batches, rhs.batches);
        std::swap(objectsBuffer, rhs.objectsBuffer);
        std::swap(instanceBuffer, rhs.instanceBuffer);
        std::swap(mappingBuffer, rhs.mappingBuffer);
        std::swap(instructionsBuffer, rhs.instructionsBuffer);
        std::swap(indicesBuffer, rhs.indicesBuffer);
        std::swap(verticesBuffer, rhs.verticesBuffer);
        std::swap(meshReferances, rhs.meshReferances);

        std::swap(depthPyramid, rhs.depthPyramid);
    }

    return *this;
}

GPUDrivenRendererSystem::~GPUDrivenRendererSystem()
{
    depthPyramid.clear();
    computeDescriptor.clear();
    renderDescriptor.clear();
    computeRenderer.~ComputePipeline();
    renderPass.~RenderPass();
}

uint32_t GPUDrivenRendererSystem::AddMesh(Span<const uint32_t> triangles, Span<const glm::vec3> vertices)
{
    glm::vec3 center(0.0f);
    float radius = 0.0f;
    for (const glm::vec3& vertex : vertices)
    {
        center += vertex;
        radius = std::max(radius, glm::dot(vertex, vertex));
    }
    center /= vertices.size();
    radius = std::sqrt(std::max(radius, glm::dot(center, center)));
    meshReferances.push_back(MeshReferance(center, radius, verticesBuffer.size(), indicesBuffer.size(), triangles.size()));

    indicesBuffer.insert_range(triangles, indicesBuffer.size());
    verticesBuffer.insert_range(vertices, verticesBuffer.size());

    return meshReferances.size() - 1;
}

void GPUDrivenRendererSystem::ReserveRenderObjects(int count)
{
    for (int i = 0; i < objectsBuffer.size(); i++)
    {
        objectsBuffer[i].reserve(count);
        instanceBuffer[i].reserve(count);
    }
}

void GPUDrivenRendererSystem::AddRenderObject(uint32_t mesh, uint32_t material, const glm::mat4& matrix)
{
    uint32_t objectID = objectsBuffer[frameIndex].size();
    uint32_t batchID = 0;
    int ID = (material << 16) | mesh;
    if (batchMap.contains(ID))
    {
        batchID = batchMap[ID];
        batches[batchID].objectCount++;
    }
    else
    {
        batchID = batchMap.size();
        batches.push_back(Batch(mesh, 1));
        batchMap[ID] = batchID;
    }

    glm::vec3 center = matrix * glm::vec4(meshReferances[batches[batchID].meshID].boundingCenter, 1.0f);
    float maxScale = 0.0f;
    for (int j = 0; j < 3; j++)
        maxScale = std::max(maxScale, glm::length(matrix[j]));

    float radius = maxScale * meshReferances[batches[batchID].meshID].boundingRadius;
    for (int i = 0; i < objectsBuffer.size(); i++)
    {
        objectsBuffer[i].push_back(RenderObject(center, radius, objectID, batchID));
        instanceBuffer[i].push_back(matrix);
    }
}

void GPUDrivenRendererSystem::GetRenderCommands(const vg::Image& depthImage, const vg::ImageView& depthView, vg::CmdBuffer& buffer, const glm::mat4& cameraView, const glm::mat4& cameraProjection, const vg::Framebuffer& framebuffer, uint32_t width, uint32_t height)
{
    if (objectsBuffer[frameIndex].size() == 0)
        return;

    static int a = 0;
    if (a < 2)
    {
        a++;
        mappingBuffer[frameIndex].resize(objectsBuffer[frameIndex].size());
        instructionsBuffer[frameIndex].resize(batches.size());
        uint32_t offset = 0;
        for (int j = 0; j < batches.size(); j++)
        {
            auto& ref = meshReferances[batches[j].meshID];
            instructionsBuffer[frameIndex][j] = vg::cmd::DrawIndexed(ref.triangleCount, 0, ref.triangleStart, ref.vertexStart, offset);
            offset += batches[j].objectCount;
        }

        computeDescriptor[frameIndex].AttachBuffer(vg::DescriptorType::StorageBuffer, objectsBuffer[frameIndex], 0, objectsBuffer[frameIndex].byte_size(), 0, 0);
        computeDescriptor[frameIndex].AttachBuffer(vg::DescriptorType::StorageBuffer, instructionsBuffer[frameIndex], 0, batches.size() * sizeof(vg::cmd::DrawIndexed), 1, 0);
        computeDescriptor[frameIndex].AttachBuffer(vg::DescriptorType::StorageBuffer, mappingBuffer[frameIndex], 0, mappingBuffer[frameIndex].byte_size(), 2, 0);
        renderDescriptor[frameIndex].AttachBuffer(vg::DescriptorType::StorageBuffer, instanceBuffer[frameIndex], 0, instanceBuffer[frameIndex].byte_size(), 0, 0);
    }
    int total = 0;
    if (!Settings::stopCameraOcclusionUpdate)
        for (int j = 0; j < batches.size(); j++)
        {
            total += instructionsBuffer[frameIndex][j].instanceCount;
            instructionsBuffer[frameIndex][j].instanceCount = 0;
        }

    static glm::mat4 projection;
    static glm::mat4 view;
    if (!Settings::stopCameraOcclusionUpdate)
    {
        projection = cameraProjection;
        view = cameraView;
    }

    // if (depthPyramid.size() != objectsBuffer.size())
    // {
    //     depthPyramid.resize(objectsBuffer.size());
    //     for (int i = 0; i < depthPyramid.size(); i++)
    //     {
    //         depthPyramid[i] = DepthPyramid(descriptorPool, depthImage, depthView);
    //         computeDescriptor[i].AttachImage(vg::DescriptorType::CombinedImageSampler, vg::ImageLayout::ShaderReadOnlyOptimal, depthPyramid[frameIndex].pyramidImageView, depthPyramid[frameIndex].reductionSampler, 3, 0);
    //     }
    // }
    // depthPyramid[frameIndex].Generate(depthImage, depthView, buffer);



    buffer.Append(
        vg::cmd::PushConstants(computeRenderer.GetPipelineLayout(), { vg::ShaderStage::Vertex, vg::ShaderStage::Compute }, 0, std::make_tuple(view, projection)),
        vg::cmd::BindPipeline(computeRenderer),
        vg::cmd::BindDescriptorSets(computeRenderer.GetPipelineLayout(), vg::PipelineBindPoint::Compute, 0, { computeDescriptor[frameIndex] }),
        vg::cmd::Dispatch((objectsBuffer[frameIndex].size() / 1024.0f), 1, 1),
        vg::cmd::PipelineBarier(vg::PipelineStage::ComputeShader, vg::PipelineStage::DrawIndirect, { vg::MemoryBarrier(vg::Access::ShaderWrite, vg::Access::IndirectCommandRead) })
    );

    // buffer.Append(
    //     vg::cmd::PushConstants(computeRenderer.GetPipelineLayout(), { vg::ShaderStage::Vertex, vg::ShaderStage::Compute }, 0, cameraProjection * cameraView),
    //     vg::cmd::BeginRenderpass(renderPass, framebuffer, { 0, 0 }, { width, height }, { vg::ClearColor{ 0,0,0,255 },vg::ClearDepthStencil{1.0f,0U} }, vg::SubpassContents::Inline),
    //     vg::cmd::BindPipeline(renderPass.GetPipelines()[0]),
    //     vg::cmd::BindDescriptorSets(renderPass.GetPipelineLayouts()[0], vg::PipelineBindPoint::Graphics, 0, { renderDescriptor[frameIndex] }),
    //     vg::cmd::BindVertexBuffers(std::vector<vg::BufferHandle>{ verticesBuffer, mappingBuffer[frameIndex] }, { 0, 0 }, 0),
    //     vg::cmd::BindIndexBuffer(indicesBuffer, 0, vg::IndexType::Uint32),
    //     vg::cmd::SetViewport(vg::Viewport(width, height)),
    //     vg::cmd::SetScissor(vg::Scissor(width, height)),
    //     vg::cmd::DrawIndexedIndirect(instructionsBuffer[frameIndex], 0, batches.size(), sizeof(vg::cmd::DrawIndexed)),
    //     vg::cmd::EndRenderpass()
    // );

    frameIndex = (frameIndex + 1) % renderDescriptor.size();
}