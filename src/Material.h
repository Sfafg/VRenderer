#pragma once
#include "VG/VG.h"
#include <vector>
#include "RenderBuffer.h"
#include "ByteView.h"

class MaterialArray {
  public:
    MaterialArray(int maxFramesInFlight);

    MaterialArray();
    MaterialArray(MaterialArray &&);
    MaterialArray &operator=(MaterialArray &&);
    MaterialArray(const MaterialArray &) = delete;
    MaterialArray &operator=(const MaterialArray &) = delete;
    ~MaterialArray();

  private:
    friend class Renderer;
    friend class Material;
    friend class BatchArray;
    RenderBuffer materialBuffer;
    std::vector<vg::Subpass> subpasses;
    std::vector<vg::SubpassDependency> dependecies;
    std::vector<bool> isTransparent;
    std::vector<std::vector<class Material *>> materials;
};

/**
 * @brief Class used to represent a material used for rendering.
 */
class Material {
  public:
    friend class Renderer;
    friend class RenderObject;
    friend class Batch;

    static MaterialArray *materialArray;

    uint16_t index;
    uint16_t variant;

    Material(bool isTransparent, vg::Subpass &&subpass, vg::SubpassDependency &&dependecy, ByteView data);

  public:
    struct CreateInfo {
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

    Material(Material *parentMaterial, ByteView materialData = ByteView());

    Material(
        bool isTransparent, const char *vertexShaderPath, const char *fragmentShaderPath,
        vg::VertexLayout &&vertexInput, vg::InputAssembly &&inputAssembly, vg::ViewportState &&viewportState,
        vg::Rasterizer &&rasterizer, vg::DepthStencil &&depthStencil, vg::ColorBlending &&colorBlending,
        const std::vector<vg::DynamicState> &dynamicState, const std::vector<vg::AttachmentReference> &colorAttachments,
        uint32_t childrenCount, vg::SubpassDependency &&dependency, ByteView materialData = ByteView()
    );

    Material(
        bool isTransparent, const char *vertexShaderPath, const char *fragmentShaderPath,
        vg::VertexLayout &&vertexInput, CreateInfo &&createInfo, vg::SubpassDependency &&dependency,
        ByteView materialData = ByteView()
    );

    Material();
    Material(Material &&);
    Material &operator=(Material &&);
    Material(const Material &) = delete;
    Material &operator=(const Material &) = delete;
    ~Material();

    void Write(ByteView data, uint32_t offset = 0);
    void Read(void *data, uint32_t readSize, uint32_t offset = 0);
    template <typename T> T Read(uint32_t offset = 0) {
        T t;
        Read(&t, sizeof(t), offset);
        return t;
    }
    bool IsTransparent();
};
