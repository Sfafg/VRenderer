#include "Material.h"
#include "Renderer.h"
#include <cassert>

Material::Material(vg::Subpass &&subpass, vg::SubpassDependency &&dependecy, const void *materialData, int byteSize)
    : variant(0) {

    // TODO: Zrobić lepiej.
    if (materialBuffer.backBuffer.GetSize() == 0) materialBuffer = RenderBuffer(16, vg::BufferUsage::StorageBuffer);

    subpasses.emplace_back(std::move(subpass));
    dependecies.emplace_back(std::move(dependecy));

    if (materialData) {
        index = materialBuffer.Allocate(byteSize, byteSize);
        materialBuffer.Write(index, materialData, byteSize);
    }
    materials.push_back(this);

    Renderer::RecreateRenderpass();
}

Material::Material(Material *material, const void *materialData, int byteSize)
    : index(material->index), variant(materialBuffer.sizes[material->index] / byteSize) {
    assert(byteSize == materialBuffer.sizes[material->index]);
    materialBuffer.Reallocate(material->index, materialBuffer.sizes[material->index] + byteSize);
    materialBuffer.Write(index, materialData, byteSize, variant * byteSize);
    materials.push_back(this);
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
                      vg::Shader(vg::ShaderStage::Fragment, fragmentShaderPath)},
                  vertexInput, inputAssembly, vg::Tesselation(), viewportState, rasterizer, vg::Multisampling(),
                  depthStencil, colorBlending, dynamicState, childrenCount
              ),
              {}, colorAttachments, {}, {}, {}
          ),
          std::move(dependecy), materialData, dataSize
      ) {}

Material::Material() : index(-1), variant(0) {}
Material::Material(Material &&o) : Material() {
    std::swap(index, o.index);
    std::swap(variant, o.variant);
    if (index != (uint16_t)-1) materials[index] = this;
}
Material &Material::operator=(Material &&o) {
    if (this == &o) return *this;

    if (index != (uint16_t)-1 && o.index != (uint16_t)-1) std::swap(materials[index], materials[o.index]);
    else if (o.index != (uint16_t)-1) materials[o.index] = this;

    std::swap(index, o.index);
    std::swap(variant, o.variant);

    return *this;
}
Material::~Material() {
    if (index == (uint16_t)-1) return;

    // Tutaj powinno usuwać tylko swój wariant.
    materialBuffer.Deallocate(index);
    materials.erase(materials.begin() + index);
    for (int i = index; i < materials.size(); i++) materials[i]->index--;
    index = -1;
}
std::vector<vg::Subpass> Material::subpasses;
std::vector<vg::SubpassDependency> Material::dependecies;
std::vector<Material *> Material::materials;
RenderBuffer Material::materialBuffer;
