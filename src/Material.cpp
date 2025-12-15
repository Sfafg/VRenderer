#include "Material.h"
#include "Renderer.h"
#include "Batch.h"
#include <cassert>

MaterialManager::MaterialManager(int maxFramesInFlight) {
    materialBuffer = RenderBuffer(maxFramesInFlight, vg::BufferUsage::StorageBuffer, 0);
}

MaterialManager::MaterialManager() {}

MaterialManager::MaterialManager(MaterialManager &&o) : MaterialManager() {
    std::swap(materialBuffer, o.materialBuffer);
    std::swap(subpasses, o.subpasses);
    std::swap(dependecies, o.dependecies);
    std::swap(materials, o.materials);
}

MaterialManager &MaterialManager::operator=(MaterialManager &&o) {
    if (this == &o) return *this;

    std::swap(materialBuffer, o.materialBuffer);
    std::swap(subpasses, o.subpasses);
    std::swap(dependecies, o.dependecies);
    std::swap(materials, o.materials);

    return *this;
}

MaterialManager::~MaterialManager() {}

Material::Material(
    bool isTransparent, vg::Subpass &&subpass, vg::SubpassDependency &&dependecy, const void *materialData, int byteSize
)
    : variant(0) {
    assert(currentRenderer && "Current Renderer needs to be assigned!");
    auto &materialManager = *currentRenderer->managers.materialManager;

    materialManager.subpasses.emplace_back(std::move(subpass));
    materialManager.dependecies.emplace_back(std::move(dependecy));

    index = materialManager.materialBuffer.Allocate(byteSize, byteSize);
    if (materialData) materialManager.materialBuffer.Write(index, materialData, byteSize);
    materialManager.materials.push_back({this});
    currentRenderer->RecreateRenderpass();
}

Material::Material(Material *material, const void *materialData, int byteSize) : index(material->index) {
    assert(currentRenderer && "Current Renderer needs to be assigned!");
    auto &materialManager = *currentRenderer->managers.materialManager;
    assert(byteSize == materialManager.materialBuffer.Alignment(material->index));

    variant = materialManager.materialBuffer.sizes[material->index] / byteSize;
    materialManager.materialBuffer.Reallocate(
        material->index, materialManager.materialBuffer.sizes[material->index] + byteSize
    );
    if (materialData) materialManager.materialBuffer.Write(index, materialData, byteSize, variant * byteSize);
    materialManager.materials[index].push_back(this);
}

Material::Material(
    bool isTransparent, const char *vertexShaderPath, const char *fragmentShaderPath, vg::VertexLayout &&vertexInput,
    MaterialCreateInfo &&createInfo, vg::SubpassDependency &&dependency, const void *materialData, int dataSize
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
          createInfo.dynamicState, createInfo.colorAttachments, 0, std::move(dependency), materialData, dataSize
      ) {}

Material::Material(
    bool isTransparent, const char *vertexShaderPath, const char *fragmentShaderPath, vg::VertexLayout &&vertexInput,
    vg::InputAssembly &&inputAssembly, vg::ViewportState &&viewportState, vg::Rasterizer &&rasterizer,
    vg::DepthStencil &&depthStencil, vg::ColorBlending &&colorBlending,
    const std::vector<vg::DynamicState> &dynamicState, const std::vector<vg::AttachmentReference> &colorAttachments,
    uint32_t childrenCount, vg::SubpassDependency &&dependecy, const void *materialData, int dataSize
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
          std::move(dependecy), materialData, dataSize
      ) {}

Material::Material() : index(-1), variant(0) {}

Material::Material(Material &&o) : Material() {
    assert(currentRenderer && "Current Renderer needs to be assigned!");
    auto &materialManager = *currentRenderer->managers.materialManager;

    std::swap(index, o.index);
    std::swap(variant, o.variant);
    if (index != (uint16_t)-1) materialManager.materials[index][variant] = this;
    if (o.index != (uint16_t)-1) materialManager.materials[o.index][o.variant] = &o;
}

Material &Material::operator=(Material &&o) {
    if (this == &o) return *this;
    assert(currentRenderer && "Current Renderer needs to be assigned!");
    auto &materialManager = *currentRenderer->managers.materialManager;

    std::swap(index, o.index);
    std::swap(variant, o.variant);

    if (index != (uint16_t)-1) materialManager.materials[index][variant] = this;
    if (o.index != (uint16_t)-1) materialManager.materials[o.index][o.variant] = &o;

    return *this;
}

Material::~Material() {
    if (index == (uint16_t)-1) return;
    assert(currentRenderer && "Current Renderer needs to be assigned!");
    auto &materialManager = *currentRenderer->managers.materialManager;
    auto &batchManager = *currentRenderer->managers.batchManager;

    bool lastVariant = materialManager.materialBuffer.Size(index) == materialManager.materialBuffer.Alignment(index);

    materialManager.materials[index].erase(materialManager.materials[index].begin() + variant);
    if (lastVariant) {
        materialManager.materials.erase(materialManager.materials.begin() + index);

        batchManager.NotifyMaterialDestroy(index);
        materialManager.materialBuffer.Deallocate(index);
        materialManager.subpasses.erase(materialManager.subpasses.begin() + index);
        materialManager.dependecies.erase(materialManager.dependecies.begin() + index);
        for (int i = index; i < materialManager.materials.size(); i++)
            for (int j = 0; j < materialManager.materials[i].size(); j++) materialManager.materials[i][j]->index--;

        currentRenderer->RecreateRenderpass();
    } else {
        batchManager.NotifyVariantDestroy(index, variant);
        uint32_t variantSize = materialManager.materialBuffer.Alignment(index);
        uint32_t variantOffset = variant * variantSize;
        materialManager.materialBuffer.Erase(index, variantSize, variantOffset);

        // Zaktualizuj numery wariantów dla materiałów o wyższych wariantach
        for (int j = 0; j < materialManager.materials[index].size(); j++) materialManager.materials[index][j]->index--;
    }
    index = -1;
}

void Material::Write(const void *data, uint32_t dataSize, uint32_t offset) {
    assert(currentRenderer && "Current Renderer needs to be assigned!");
    auto &materialManager = *currentRenderer->managers.materialManager;

    materialManager.materialBuffer.Write(
        index, data, dataSize, offset + materialManager.materialBuffer.Alignment(index) * variant
    );
}

void Material::Read(void *data, uint32_t readSize, uint32_t offset) {
    assert(currentRenderer && "Current Renderer needs to be assigned!");
    auto &materialManager = *currentRenderer->managers.materialManager;

    materialManager.materialBuffer.Read(
        index, data, readSize, offset + materialManager.materialBuffer.Alignment(index) * variant
    );
}
