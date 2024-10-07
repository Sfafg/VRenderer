#include "DepthPyramid.h"
#include <glm/glm.hpp>

DepthPyramid::DepthPyramid(uint32_t width, uint32_t height)
{
    vg::Shader vertexPass = vg::Shader(vg::ShaderStage::Vertex, "resources/shaders/passthrough.vert.spv");
    depthPrepass = vg::RenderPass(
        {
            vg::Attachment(vg::Format::D32SFLOAT, vg::ImageLayout::DepthAttachmentOptimal)
        },
        {
            vg::Subpass(
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
                {}, vg::AttachmentReference(0, vg::ImageLayout::DepthStencilAttachmentOptimal)
            )
        },
        { vg::SubpassDependency(-1, 0, vg::PipelineStage::ColorAttachmentOutput, vg::PipelineStage::ColorAttachmentOutput, 0, vg::Access::DepthStencilAttachmentWrite, {}) }
    );
}

void DepthPyramid::SetSize(uint32_t width, uint32_t height)
{
    if (pyramidImage.GetDimensions()[0] == width && pyramidImage.GetDimensions()[1] == height)
        return;

    // int mipLevels = (uint32_t) std::floor(std::log2(std::min({ width,height })));

    pyramidImage = vg::Image({ width / 2,height / 2 }, vg::Format::R32SFLOAT, { vg::ImageUsage::Storage ,vg::ImageUsage::Sampled }, 1);
    vg::Allocate(pyramidImage, { vg::MemoryProperty::DeviceLocal });
    pyramidImageView = vg::ImageView(pyramidImage, vg::ImageSubresource(vg::ImageAspect::Color, 0, 1));
    depthFramebuffer = vg::Framebuffer(depthPrepass, { pyramidImageView }, width, height, 1);
}

// void DepthPyramid::Generate(vg::CmdBuffer& commandBuffer)
// {
//     commandBuffer.Append(
//         vg::cmd::PipelineBarier(
//             vg::PipelineStage::ComputeShader, vg::PipelineStage::ComputeShader,
//             {
//                 { pyramidImage, vg::ImageLayout::General, vg::Access::ShaderRead,vg::Access::ShaderRead, {vg::ImageAspect::Color,0,(uint32_t) reductionDescriptors.size()}},
//                 { depthImage,vg::ImageLayout::DepthStencilAttachmentOptimal, vg::ImageLayout::ShaderReadOnlyOptimal, vg::Access::ShaderRead,vg::Access::ShaderRead, {vg::ImageAspect::Depth,0,1} }
//             })
//     );

//     vg::cmd::PipelineBarier barrier(
//         vg::PipelineStage::ComputeShader, vg::PipelineStage::ComputeShader,
//         {
//              {pyramidImage, vg::ImageLayout::General, vg::ImageLayout::ShaderReadOnlyOptimal, vg::Access::ShaderWrite, vg::Access::ShaderWrite, {vg::ImageAspect::Color,0,1}}
//         }
//     );
//     uint32_t pyramidWidth = pyramidImage.GetDimensions()[0], pyramidHeight = pyramidImage.GetDimensions()[1];
//     for (int i = 0; i < reductionDescriptors.size(); i++)
//     {
//         if (i == 0)
//             reductionDescriptors[i].AttachImage(vg::DescriptorType::CombinedImageSampler, vg::ImageLayout::ShaderReadOnlyOptimal, depthView, reductionSampler, 0, 0);
//         else
//             reductionDescriptors[i].AttachImage(vg::DescriptorType::CombinedImageSampler, vg::ImageLayout::ShaderReadOnlyOptimal, reductionImageViews[i - 1], reductionSampler, 0, 0);

//         reductionDescriptors[i].AttachImage(vg::DescriptorType::StorageImage, vg::ImageLayout::General, reductionImageViews[i], reductionSampler, 1, 0);
//         barrier.imageMemoryBarriers[0].subresourceRange.baseMipLevel = i;
//         commandBuffer.Append(
//             vg::cmd::BindPipeline(computePipeline),
//             vg::cmd::BindDescriptorSets(computePipeline.GetPipelineLayout(), vg::PipelineBindPoint::Compute, 0, { reductionDescriptors[i] }),
//             vg::cmd::PushConstants(computePipeline.GetPipelineLayout(), vg::ShaderStage::Compute, 0, std::make_tuple(pyramidHeight, pyramidWidth)),
//             vg::cmd::Dispatch(ceil(pyramidWidth / 16.0f), ceil(pyramidHeight / 16.0f), 1),
//             barrier
//         );
//         pyramidWidth = std::max(pyramidWidth / 2, 1U);
//         pyramidHeight = std::max(pyramidHeight / 2, 1U);
//     }

//     commandBuffer.Append(
//         vg::cmd::PipelineBarier(
//             vg::PipelineStage::ComputeShader, vg::PipelineStage::ComputeShader,
//             {
//                 {depthImage, vg::ImageLayout::ShaderReadOnlyOptimal, vg::ImageLayout::DepthStencilAttachmentOptimal, vg::Access::ShaderWrite,vg::Access::ShaderWrite, {vg::ImageAspect::Depth,0,1}}
//             })
//     );
// }