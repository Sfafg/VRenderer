#include "HiZBuffer.h"
#include <assert.h>
using namespace vg;
using namespace cmd;

HiZBuffer::HiZBuffer(uint width, uint height) {

    computePipeline = ComputePipeline(
        Shader(ShaderStage::Compute, "resources/shaders/Reduction.comp.spv"),
        PipelineLayout(
            {{{0, DescriptorType::StorageBuffer, 1, ShaderStage::Compute},
              {1, DescriptorType::CombinedImageSampler, 1, ShaderStage::Compute},
              {2, DescriptorType::StorageImage, 12, ShaderStage::Compute}}},
            {{ShaderStage::Compute, 0, sizeof(int) * 4}}
        )
    );

    image = Image(
        {width / 2, height / 2}, {Format::R32SFLOAT}, {FormatFeature::ColorAttachment},
        {ImageUsage::Sampled, ImageUsage::Storage}, -1
    );
    Allocate(image, {MemoryProperty::DeviceLocal});

    sampler = Sampler(
        Filter::Linear, Filter::Linear, SamplerMipmapMode::Nearest, SamplerAddressMode::ClampToEdge,
        SamplerAddressMode::ClampToEdge, SamplerAddressMode::ClampToEdge, 0, 0, 1000, SamplerReduction::Max
    );
    mips.resize(image.GetMipLevels());
    view = ImageView(image, ImageSubresource(ImageAspect::Color, 0, image.GetMipLevels()));
    for (int i = 0; i < mips.size(); i++) mips[i] = ImageView(image, ImageSubresource(ImageAspect::Color, i));

    descriptorPool = DescriptorPool(
        1, {{DescriptorType::StorageBuffer, 1},
            {DescriptorType::CombinedImageSampler, 1},
            {DescriptorType::StorageImage, 12}}
    );

    descriptors = std::move(descriptorPool.Allocate(computePipeline.GetPipelineLayout().GetDescriptorSets()[0])[0]);

    counter = Buffer(sizeof(uint), {BufferUsage::StorageBuffer, BufferUsage::TransferDst});
    Allocate(counter, MemoryProperty::DeviceLocal);

    descriptors.AttachBuffer(DescriptorType::StorageBuffer, counter, 0, -1, 0, 0);

    for (int i = 0; i < 12; i++) {
        descriptors.AttachImage(
            DescriptorType::StorageImage, ImageLayout::General, mips[std::min(i, (int)mips.size() - 1)], sampler, 2, i
        );
    }
}

HiZBuffer::Reduce::Reduce(HiZBuffer &buffer, const ImageView &depthView) : buffer(buffer), depthView(depthView) {}

void HiZBuffer::Reduce::operator()(CmdBuffer &commandBuffer) const {
    assert(buffer.mips.size() < 12);

    buffer.descriptors.AttachImage(
        DescriptorType::CombinedImageSampler, ImageLayout::DepthStencilReadOnlyOptimal, depthView, buffer.sampler, 1, 0
    );
    int w = buffer.image.GetDimensions()[0] * 2;
    int h = buffer.image.GetDimensions()[1] * 2;

    commandBuffer.Append(
        PipelineBarier(
            PipelineStage::FragmentShader, PipelineStage::ComputeShader,
            {ImageMemoryBarrier(
                buffer.image, ImageLayout::General, Access::ShaderWrite, Access::ShaderRead,
                ImageSubresource(ImageAspect::Color, 0, buffer.mips.size())
            )}
        ),
        FillBuffer(buffer.counter, 0, sizeof(uint), 0), BindPipeline(buffer.computePipeline),
        PipelineBarier(
            PipelineStage::Transfer, PipelineStage::ComputeShader,
            {MemoryBarrier(Access::TransferWrite, Access::ShaderWrite)}
        ),
        BindDescriptorSets(
            buffer.computePipeline.GetPipelineLayout(), PipelineBindPoint::Compute, 0, {buffer.descriptors}
        ),
        PushConstants(
            buffer.computePipeline.GetPipelineLayout(), ShaderStage::Compute, 0,
            std::make_tuple(0, (int)buffer.mips.size(), h, w)
        ),
        Dispatch(std::ceil(w / 64.0f), std::ceil(h / 64.0f), 1)
    );
}

HiZBuffer::PipelineBarrier::PipelineBarrier(
    HiZBuffer &buffer, PipelineStage dstStage, ImageLayout targetLayout, Flags<Access> dstAccessMask
)
    : buffer(buffer), dstStage(dstStage), targetLayout(targetLayout), dstAccessMask(dstAccessMask) {}

void HiZBuffer::PipelineBarrier::operator()(CmdBuffer &commandBuffer) const {
    commandBuffer.Append(PipelineBarier(
        PipelineStage::ComputeShader, dstStage,
        {ImageMemoryBarrier(
            buffer.image, ImageLayout::General, targetLayout, Access::ShaderWrite, dstAccessMask,
            ImageSubresource(ImageAspect::Color, 0, buffer.mips.size())
        )}
    ));
}
