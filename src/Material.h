#pragma once
#include "VG/VG.h"
#include <iostream>
#include "RenderBuffer.h"

/**
 * @brief Class used to represent a material used for rendering.
 */
class Material {
    friend class Renderer;

    // TODO: W destruktorze zmienić aby usuwała się tylko porcja danych wariantu, chyba że jest to ostatni wariant
    // materiału to cały region.
    static std::vector<vg::Subpass> subpasses;
    static std::vector<vg::SubpassDependency> dependecies;
    static std::vector<Material *> materials;
    static RenderBuffer materialBuffer;

    uint16_t index;
    uint16_t variant;

    Material(vg::Subpass &&subpass, vg::SubpassDependency &&dependecy, const void *materialData, int byteSize);

  public:
    struct MaterialCreateInfo {
        vg::Primitive primitive = vg::Primitive::Triangles;
        bool primitiveRestart = false;
        vg::ViewportState viewportState = vg::ViewportState(vg::Viewport(0, 0), vg::Scissor(0, 0));
        bool depthClamp = false;
        bool discard = false;
        vg::PolygonMode polygonMode = vg::PolygonMode::Fill;
        vg::CullMode cullMode = vg::CullMode::Back;
        vg::FrontFace frontFace = vg::FrontFace::CounterClockwise;
        vg::DepthBias depthBias;
        float lineWidth = 1.0f;
        bool depthTestEnable = true;
        bool depthWriteEnable = true;
        vg::CompareOp depthCompareOp = vg::CompareOp::Less;
        bool depthBoundsTestEnable = false;
        bool stencilTestEnable = false;
        vg::StencilOpState front = {};
        vg::StencilOpState back = {};
        float minDepthBounds = 0;
        float maxDepthBounds = 0;
        bool enableLogicOp = true;
        vg::LogicOp logicOp = vg::LogicOp::Copy;
        uint32_t attachmentCount = 0;
        std::vector<vg::ColorBlend> attachments = {vg::ColorBlend(
            vg::BlendFactor::SrcAlpha, vg::BlendFactor::OneMinusSrcAlpha, vg::BlendOp::Add, vg::BlendFactor::One,
            vg::BlendFactor::Zero, vg::BlendOp::Add, vg::ColorComponent::RGBA
        )};
        float blendConsts[4] = {0, 0, 0, 0};
        std::vector<vg::DynamicState> dynamicState = {vg::DynamicState::Viewport, vg::DynamicState::Scissor};
        std::vector<vg::AttachmentReference> colorAttachments = {
            vg::AttachmentReference(0, vg::ImageLayout::ColorAttachmentOptimal)
        };
    };

  public:
    Material(Material *parentMaterial, const void *materialData = nullptr, int byteSize = 0);

    Material(
        const char *vertexShaderPath, const char *fragmentShaderPath, vg::VertexLayout &&vertexInput,
        vg::InputAssembly &&inputAssembly, vg::ViewportState &&viewportState, vg::Rasterizer &&rasterizer,
        vg::DepthStencil &&depthStencil, vg::ColorBlending &&colorBlending,
        const std::vector<vg::DynamicState> &dynamicState, const std::vector<vg::AttachmentReference> &colorAttachments,
        uint32_t childrenCount, vg::SubpassDependency &&dependency, const void *materialData = nullptr, int byteSize = 0
    );

    Material(
        const char *vertexShaderPath, const char *fragmentShaderPath, vg::VertexLayout &&vertexInput,
        MaterialCreateInfo &&createInfo, vg::SubpassDependency &&dependency, const void *materialData = nullptr,
        int byteSize = 0
    );

    template <typename T>
    Material(
        const char *vertexShaderPath, const char *fragmentShaderPath, vg::VertexLayout &&vertexInput,
        vg::InputAssembly &&inputAssembly, vg::ViewportState &&viewportState, vg::Rasterizer &&rasterizer,
        vg::DepthStencil &&depthStencil, vg::ColorBlending &&colorBlending,
        const std::vector<vg::DynamicState> &dynamicState, const std::vector<vg::AttachmentReference> &colorAttachments,
        uint32_t childrenCount, vg::SubpassDependency &&dependency, const T &materialData
    );

    template <typename T>
    Material(
        const char *vertexShaderPath, const char *fragmentShaderPath, vg::VertexLayout &&vertexInput,
        MaterialCreateInfo &&createInfo, vg::SubpassDependency &&dependency, const T &materialData
    );

    template <typename T> Material(Material *material, const T &materialData);

    Material();
    Material(Material &&);
    Material &operator=(Material &&);
    Material(const Material &) = delete;
    Material &operator=(const Material &) = delete;
    ~Material();

    void Write(const void *data, uint32_t dataSize, uint32_t offset = 0);
    template <typename T> void Write(const T &data, uint32_t offset = 0);

    void Read(void *data, uint32_t readSize, uint32_t offset = 0);
    template <typename T> T Write(uint32_t offset = 0);
};
template <typename T>
Material::Material(
    const char *vertexShaderPath, const char *fragmentShaderPath, vg::VertexLayout &&vertexInput,
    MaterialCreateInfo &&createInfo, vg::SubpassDependency &&dependency, const T &materialData
)
    : Material(
          vertexShaderPath, fragmentShaderPath, std::move(vertexInput), std::move(createInfo), std::move(dependency),
          &materialData, sizeof(T)
      ) {}

template <typename T>
Material::Material(
    const char *vertexShaderPath, const char *fragmentShaderPath, vg::VertexLayout &&vertexInput,
    vg::InputAssembly &&inputAssembly, vg::ViewportState &&viewportState, vg::Rasterizer &&rasterizer,
    vg::DepthStencil &&depthStencil, vg::ColorBlending &&colorBlending,
    const std::vector<vg::DynamicState> &dynamicState, const std::vector<vg::AttachmentReference> &colorAttachments,
    uint32_t childrenCount, vg::SubpassDependency &&dependency, const T &materialData
)
    : Material(
          vertexShaderPath, fragmentShaderPath, std::move(vertexInput), std::move(inputAssembly),
          std::move(viewportState), std::move(rasterizer), std::move(depthStencil), std::move(colorBlending),
          std::move(dynamicState), std::move(colorAttachments), childrenCount, std::move(dependency), &materialData,
          sizeof(T)
      ) {}

template <typename T>
Material::Material(Material *material, const T &materialData) : Material(material, &materialData, sizeof(T)) {}

template <typename T> void Material::Write(const T &data, uint32_t offset) { Write(&data, sizeof(data), offset); }

template <typename T> T Material::Write(uint32_t offset) {
    T t;
    Write(&t, sizeof(t), offset);
    return t;
}
