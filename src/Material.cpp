#include "Material.h"
#include "Renderer.h"
#include "Batch.h"
#include <cassert>
#include <iostream>

MaterialArray *Material::materialArray = nullptr;

MaterialArray::MaterialArray(int maxFramesInFlight) {
    materialBuffer = RenderBuffer(maxFramesInFlight, vg::BufferUsage::StorageBuffer, 0);
}

MaterialArray::MaterialArray() {}

MaterialArray::MaterialArray(MaterialArray &&o) : MaterialArray() {
    std::swap(materialBuffer, o.materialBuffer);
    std::swap(subpasses, o.subpasses);
    std::swap(dependecies, o.dependecies);
    std::swap(materials, o.materials);
}

MaterialArray &MaterialArray::operator=(MaterialArray &&o) {
    if (this == &o) return *this;

    std::swap(materialBuffer, o.materialBuffer);
    std::swap(subpasses, o.subpasses);
    std::swap(dependecies, o.dependecies);
    std::swap(materials, o.materials);

    return *this;
}

MaterialArray::~MaterialArray() {}

Material::Material(bool isTransparent, vg::Subpass &&subpass, vg::SubpassDependency &&dependecy, ByteView materialData)
    : variant(0) {
    uint byteSize = materialData.Size();
    const void *data = materialData.Ptr();

    assert(materialArray && "Current materialArray needs to be assigned!");

    materialArray->subpasses.emplace_back(std::move(subpass));
    materialArray->dependecies.emplace_back(std::move(dependecy));

    index = materialArray->materialBuffer.Allocate(byteSize, byteSize);
    if (data) materialArray->materialBuffer.Write(index, data, byteSize);
    materialArray->materials.push_back({this});
    materialArray->isTransparent.push_back(isTransparent);
    Renderer::RecreateRenderpass();
}

Material::Material(Material *material, ByteView materialData) : index(material->index) {
    uint byteSize = materialData.Size();
    const void *data = materialData.Ptr();

    assert(materialArray && "Current materialArray needs to be assigned!");
    assert(byteSize == materialArray->materialBuffer.Alignment(material->index));

    variant = materialArray->materialBuffer.sizes[material->index] / byteSize;
    materialArray->materialBuffer.Reallocate(
        material->index, materialArray->materialBuffer.sizes[material->index] + byteSize
    );
    if (data) materialArray->materialBuffer.Write(index, data, byteSize, variant * byteSize);
    materialArray->materials[index].push_back(this);
}

Material::Material(
    bool isTransparent, const char *vertexShaderPath, const char *fragmentShaderPath, vg::VertexLayout &&vertexInput,
    CreateInfo &&createInfo, vg::SubpassDependency &&dependency, ByteView materialData
)
    : Material(
          isTransparent, vertexShaderPath, fragmentShaderPath, std::move(vertexInput),
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
          createInfo.dynamicState, createInfo.colorAttachments, 0, std::move(dependency), materialData
      ) {}

Material::Material(
    bool isTransparent, const char *vertexShaderPath, const char *fragmentShaderPath, vg::VertexLayout &&vertexInput,
    vg::InputAssembly &&inputAssembly, vg::ViewportState &&viewportState, vg::Rasterizer &&rasterizer,
    vg::DepthStencil &&depthStencil, vg::ColorBlending &&colorBlending,
    const std::vector<vg::DynamicState> &dynamicState, const std::vector<vg::AttachmentReference> &colorAttachments,
    uint32_t childrenCount, vg::SubpassDependency &&dependecy, ByteView materialData
)
    : Material(
          isTransparent,
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
          std::move(dependecy), materialData
      ) {}

Material::Material() : index(-1), variant(0) {}

Material::Material(Material &&o) : Material() {
    assert(materialArray && "Current materialArray needs to be assigned!");

    std::swap(index, o.index);
    std::swap(variant, o.variant);
    if (index != (uint16_t)-1) materialArray->materials[index][variant] = this;
    if (o.index != (uint16_t)-1) materialArray->materials[o.index][o.variant] = &o;
}

Material &Material::operator=(Material &&o) {
    if (this == &o) return *this;
    assert(materialArray && "Current materialArray needs to be assigned!");

    std::swap(index, o.index);
    std::swap(variant, o.variant);

    if (index != (uint16_t)-1) materialArray->materials[index][variant] = this;
    if (o.index != (uint16_t)-1) materialArray->materials[o.index][o.variant] = &o;

    return *this;
}

Material::~Material() {
    if (index == (uint16_t)-1) return;
    assert(materialArray && "Current materialArray needs to be assigned!");
    assert(BatchArray::batchArray && "Current batchArray needs to be assigned!");

    bool lastVariant = materialArray->materialBuffer.Size(index) == materialArray->materialBuffer.Alignment(index);

    materialArray->materials[index].erase(materialArray->materials[index].begin() + variant);
    if (lastVariant) {
        materialArray->materials.erase(materialArray->materials.begin() + index);
        materialArray->isTransparent.erase(materialArray->isTransparent.begin() + index);

        BatchArray::batchArray->NotifyMaterialDestroy(index);
        materialArray->materialBuffer.Deallocate(index);
        materialArray->subpasses.erase(materialArray->subpasses.begin() + index);
        materialArray->dependecies.erase(materialArray->dependecies.begin() + index);
        for (int i = index; i < materialArray->materials.size(); i++)
            for (int j = 0; j < materialArray->materials[i].size(); j++) materialArray->materials[i][j]->index--;

        Renderer::RecreateRenderpass();
    } else {
        BatchArray::batchArray->NotifyVariantDestroy(index, variant);
        uint32_t variantSize = materialArray->materialBuffer.Alignment(index);
        uint32_t variantOffset = variant * variantSize;
        materialArray->materialBuffer.Erase(index, variantSize, variantOffset);

        // Zaktualizuj numery wariantów dla materiałów o wyższych wariantach
        for (int j = 0; j < materialArray->materials[index].size(); j++) materialArray->materials[index][j]->index--;
    }
    index = -1;
}

void Material::Write(ByteView materialData, uint32_t offset) {
    assert(materialArray && "Current materialArray needs to be assigned!");

    materialArray->materialBuffer.Write(
        index, materialData.Ptr(), materialData.Size(),
        offset + materialArray->materialBuffer.Alignment(index) * variant
    );
}

void Material::Read(void *data, uint32_t readSize, uint32_t offset) {
    assert(materialArray && "Current materialArray needs to be assigned!");

    materialArray->materialBuffer.Read(
        index, data, readSize, offset + materialArray->materialBuffer.Alignment(index) * variant
    );
}

bool Material::IsTransparent() {
    assert(materialArray && "MaterialArray needs to be assigned!");
    return materialArray->isTransparent[index];
}
