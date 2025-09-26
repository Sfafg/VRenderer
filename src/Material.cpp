#include "Material.h"
#include "Renderer.h"
#include "Batch.h"
#include <cassert>

Material::Material(vg::Subpass &&subpass, vg::SubpassDependency &&dependecy, const void *materialData, int byteSize)
    : variant(0) {
    assert(currentRenderer && "Current Renderer needs to be assigned!");
    auto &renderer = *currentRenderer;

    renderer.subpasses.emplace_back(std::move(subpass));
    renderer.dependecies.emplace_back(std::move(dependecy));

    index = renderer.materialBuffer.Allocate(byteSize, byteSize);
    if (materialData) renderer.materialBuffer.Write(index, materialData, byteSize);
    renderer.materials.push_back({this});
    renderer.RecreateRenderpass();
}

Material::Material(Material *material, const void *materialData, int byteSize) : index(material->index) {
    assert(currentRenderer && "Current Renderer needs to be assigned!");
    auto &renderer = *currentRenderer;
    assert(byteSize == renderer.materialBuffer.Alignment(material->index));

    variant = renderer.materialBuffer.sizes[material->index] / byteSize;
    renderer.materialBuffer.Reallocate(material->index, renderer.materialBuffer.sizes[material->index] + byteSize);
    if (materialData) renderer.materialBuffer.Write(index, materialData, byteSize, variant * byteSize);
    renderer.materials[index].push_back(this);
}

Material::Material(
    const char *vertexShaderPath, const char *fragmentShaderPath, vg::VertexLayout &&vertexInput,
    MaterialCreateInfo &&createInfo, vg::SubpassDependency &&dependency, const void *materialData, int dataSize
)
    : Material(
          vertexShaderPath, fragmentShaderPath, std::move(vertexInput),
          vg::InputAssembly(createInfo.primitive, createInfo.primitiveRestart), std::move(createInfo.viewportState),
          vg::Rasterizer(
              createInfo.depthClamp, createInfo.discard, createInfo.polygonMode, createInfo.cullMode,
              createInfo.frontFace, createInfo.depthBias, createInfo.lineWidth
          ),
          createInfo.stencilTestEnable
              ? vg::DepthStencil(
                    createInfo.depthTestEnable, createInfo.depthWriteEnable, createInfo.depthCompareOp,
                    createInfo.front, createInfo.back, createInfo.depthBoundsTestEnable, createInfo.minDepthBounds,
                    createInfo.maxDepthBounds
                )
              : vg::DepthStencil(
                    createInfo.depthTestEnable, createInfo.depthWriteEnable, createInfo.depthCompareOp,
                    createInfo.depthBoundsTestEnable, createInfo.minDepthBounds, createInfo.maxDepthBounds
                ),
          vg::ColorBlending(
              createInfo.enableLogicOp, createInfo.logicOp, createInfo.blendConsts, createInfo.attachments
          ),
          createInfo.dynamicState, createInfo.colorAttachments, 0, std::move(dependency), materialData, dataSize
      ) {}

Material::Material(
    const char *vertexShaderPath, const char *fragmentShaderPath, vg::VertexLayout &&vertexInput,
    vg::InputAssembly &&inputAssembly, vg::ViewportState &&viewportState, vg::Rasterizer &&rasterizer,
    vg::DepthStencil &&depthStencil, vg::ColorBlending &&colorBlending,
    const std::vector<vg::DynamicState> &dynamicState, const std::vector<vg::AttachmentReference> &colorAttachments,
    uint32_t childrenCount, vg::SubpassDependency &&dependecy, const void *materialData, int dataSize
)
    : Material(
          vg::Subpass(
              vg::GraphicsPipeline(
                  0,
                  Vector{
                      vg::Shader(vg::ShaderStage::Vertex, vertexShaderPath),
                      vg::Shader(vg::ShaderStage::Fragment, fragmentShaderPath)
                  },
                  vertexInput, inputAssembly, vg::Tesselation(), viewportState, rasterizer, vg::Multisampling(),
                  depthStencil, colorBlending, dynamicState, childrenCount
              ),
              {}, colorAttachments, {}, vg::AttachmentReference(1, vg::ImageLayout::DepthStencilAttachmentOptimal), {}
          ),
          std::move(dependecy), materialData, dataSize
      ) {}

Material::Material() : index(-1), variant(0) {}

Material::Material(Material &&o) : Material() {
    assert(currentRenderer && "Current Renderer needs to be assigned!");
    auto &renderer = *currentRenderer;

    std::swap(index, o.index);
    std::swap(variant, o.variant);
    if (index != (uint16_t)-1) renderer.materials[index][variant] = this;
    if (o.index != (uint16_t)-1) renderer.materials[o.index][o.variant] = &o;
}

Material &Material::operator=(Material &&o) {
    if (this == &o) return *this;
    assert(currentRenderer && "Current Renderer needs to be assigned!");
    auto &renderer = *currentRenderer;

    std::swap(index, o.index);
    std::swap(variant, o.variant);

    if (index != (uint16_t)-1) renderer.materials[index][variant] = this;
    if (o.index != (uint16_t)-1) renderer.materials[o.index][o.variant] = &o;

    return *this;
}

Material::~Material() {
    if (index == (uint16_t)-1) return;
    assert(currentRenderer && "Current Renderer needs to be assigned!");
    auto &renderer = *currentRenderer;

    bool lastVariant = renderer.materialBuffer.Size(index) == renderer.materialBuffer.Alignment(index);

    renderer.materials[index].erase(renderer.materials[index].begin() + variant);
    if (lastVariant) {
        renderer.materials.erase(renderer.materials.begin() + index);
        Batch::NotifyMaterialDestroy(index);
        renderer.materialBuffer.Deallocate(index);
        renderer.subpasses.erase(renderer.subpasses.begin() + index);
        renderer.dependecies.erase(renderer.dependecies.begin() + index);
        for (int i = index; i < renderer.materials.size(); i++)
            for (int j = 0; j < renderer.materials[i].size(); j++) renderer.materials[i][j]->index--;

        renderer.RecreateRenderpass();
    } else {
        Batch::NotifyVariantDestroy(index, variant);
        uint32_t variantSize = renderer.materialBuffer.Alignment(index);
        uint32_t variantOffset = variant * variantSize;
        renderer.materialBuffer.Erase(index, variantSize, variantOffset);

        // Zaktualizuj numery wariantów dla materiałów o wyższych wariantach
        for (int j = 0; j < renderer.materials[index].size(); j++) renderer.materials[index][j]->index--;
    }
    index = -1;
}

void Material::Write(const void *data, uint32_t dataSize, uint32_t offset) {
    assert(currentRenderer && "Current Renderer needs to be assigned!");
    auto &renderer = *currentRenderer;

    renderer.materialBuffer.Write(index, data, dataSize, offset + renderer.materialBuffer.Alignment(index) * variant);
}

void Material::Read(void *data, uint32_t readSize, uint32_t offset) {
    assert(currentRenderer && "Current Renderer needs to be assigned!");
    auto &renderer = *currentRenderer;

    renderer.materialBuffer.Read(index, data, readSize, offset + renderer.materialBuffer.Alignment(index) * variant);
}
