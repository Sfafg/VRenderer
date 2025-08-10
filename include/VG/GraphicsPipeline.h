#pragma once
#include "Handle.h"
#include "Structs.h"
#include "Shader.h"
#include <vector>
#include <optional>
#include <iostream>

namespace vg {
class GraphicsPipeline {

  public:
    GraphicsPipeline(
        uint32_t pipelineLayout, const std::vector<Shader *> &shaders, const VertexLayout &vertexInput,
        const InputAssembly &inputAssembly, const Tesselation &tesselation, const ViewportState &viewportState,
        const Rasterizer &rasterizer, const Multisampling &multisampling, const DepthStencil &depthStencil,
        const ColorBlending &colorBlending, const std::vector<DynamicState> &dynamicState, uint32_t childrenCount = 0
    );

    GraphicsPipeline(
        uint32_t pipelineLayout, std::vector<Shader> &&shaders, const VertexLayout &vertexInput,
        const InputAssembly &inputAssembly, const Tesselation &tesselation, const ViewportState &viewportState,
        const Rasterizer &rasterizer, const Multisampling &multisampling, const DepthStencil &depthStencil,
        const ColorBlending &colorBlending, const std::vector<DynamicState> &dynamicState, uint32_t childrenCount = 0
    );

    GraphicsPipeline(
        uint32_t parentIndex, std::optional<uint32_t> pipelineLayout = std::nullopt,
        const std::optional<std::vector<Shader *>> &shaders = std::nullopt,
        const std::optional<VertexLayout> &vertexInput = std::nullopt,
        const std::optional<InputAssembly> &inputAssembly = std::nullopt,
        const std::optional<Tesselation> &tesselation = std::nullopt,
        const std::optional<ViewportState> &viewportState = std::nullopt,
        const std::optional<Rasterizer> &rasterizer = std::nullopt,
        const std::optional<Multisampling> &multisampling = std::nullopt,
        const std::optional<DepthStencil> &depthStencil = std::nullopt,
        const std::optional<ColorBlending> &colorBlending = std::nullopt,
        const std::optional<std::vector<DynamicState>> &dynamicState = std::nullopt, uint32_t childrenCount = 0
    );

    GraphicsPipeline(
        uint32_t parentIndex, std::optional<uint32_t> pipelineLayout = std::nullopt,
        std::optional<std::vector<Shader>> &&shaders = std::nullopt,
        const std::optional<VertexLayout> &vertexInput = std::nullopt,
        const std::optional<InputAssembly> &inputAssembly = std::nullopt,
        const std::optional<Tesselation> &tesselation = std::nullopt,
        const std::optional<ViewportState> &viewportState = std::nullopt,
        const std::optional<Rasterizer> &rasterizer = std::nullopt,
        const std::optional<Multisampling> &multisampling = std::nullopt,
        const std::optional<DepthStencil> &depthStencil = std::nullopt,
        const std::optional<ColorBlending> &colorBlending = std::nullopt,
        const std::optional<std::vector<DynamicState>> &dynamicState = std::nullopt, uint32_t childrenCount = 0
    );

    GraphicsPipeline(
        GraphicsPipeline *parent, std::optional<uint32_t> pipelineLayout = std::nullopt,
        const std::optional<std::vector<Shader *>> &shaders = std::nullopt,
        const std::optional<VertexLayout> &vertexInput = std::nullopt,
        const std::optional<InputAssembly> &inputAssembly = std::nullopt,
        const std::optional<Tesselation> &tesselation = std::nullopt,
        const std::optional<ViewportState> &viewportState = std::nullopt,
        const std::optional<Rasterizer> &rasterizer = std::nullopt,
        const std::optional<Multisampling> &multisampling = std::nullopt,
        const std::optional<DepthStencil> &depthStencil = std::nullopt,
        const std::optional<ColorBlending> &colorBlending = std::nullopt,
        const std::optional<std::vector<DynamicState>> &dynamicState = std::nullopt, uint32_t childrenCount = 0
    );

    GraphicsPipeline(
        GraphicsPipeline *parent, std::optional<uint32_t> pipelineLayout = std::nullopt,
        std::optional<std::vector<Shader>> &&shaders = std::nullopt,
        const std::optional<VertexLayout> &vertexInput = std::nullopt,
        const std::optional<InputAssembly> &inputAssembly = std::nullopt,
        const std::optional<Tesselation> &tesselation = std::nullopt,
        const std::optional<ViewportState> &viewportState = std::nullopt,
        const std::optional<Rasterizer> &rasterizer = std::nullopt,
        const std::optional<Multisampling> &multisampling = std::nullopt,
        const std::optional<DepthStencil> &depthStencil = std::nullopt,
        const std::optional<ColorBlending> &colorBlending = std::nullopt,
        const std::optional<std::vector<DynamicState>> &dynamicState = std::nullopt, uint32_t childrenCount = 0
    );

    GraphicsPipeline();
    GraphicsPipeline(GraphicsPipeline &&other) noexcept;
    GraphicsPipeline(const GraphicsPipeline &other) = delete;
    ~GraphicsPipeline();

    GraphicsPipeline &operator=(GraphicsPipeline &&other) noexcept;
    GraphicsPipeline &operator=(const GraphicsPipeline &other) = delete;

    operator const GraphicsPipelineHandle &() const;

  private:
    struct CreateInfo {
        uint32_t inheritanceCount;
        std::optional<uint32_t> pipelineLayout;
        std::optional<std::vector<Shader *>> shaders;
        std::optional<std::vector<Shader>> shaders_;
        std::optional<VertexLayout> vertexInput;
        std::optional<InputAssembly> inputAssembly;
        std::optional<Tesselation> tesselation;
        std::optional<ViewportState> viewportState;
        std::optional<Rasterizer> rasterizer;
        std::optional<Multisampling> multisampling;
        std::optional<DepthStencil> depthStencil;
        std::optional<ColorBlending> colorBlending;
        std::optional<std::vector<DynamicState>> dynamicState;
        int parentIndex;
        GraphicsPipeline *parent;

        CreateInfo(
            uint32_t inheritanceCount, std::optional<uint32_t> pipelineLayout,
            const std::optional<std::vector<Shader *>> &shaders, std::optional<VertexLayout> vertexInput,
            const std::optional<InputAssembly> &inputAssembly, std::optional<Tesselation> tesselation,
            const std::optional<ViewportState> &viewportState, std::optional<Rasterizer> rasterizer,
            const std::optional<Multisampling> &multisampling, std::optional<DepthStencil> depthStencil,
            const std::optional<ColorBlending> &colorBlending, std::optional<std::vector<DynamicState>> dynamicState,
            int parentIndex, GraphicsPipeline *parent
        )
            : inheritanceCount(inheritanceCount), pipelineLayout(pipelineLayout), shaders(shaders),
              vertexInput(vertexInput), inputAssembly(inputAssembly), tesselation(tesselation),
              viewportState(viewportState), rasterizer(rasterizer), multisampling(multisampling),
              depthStencil(depthStencil), colorBlending(colorBlending), dynamicState(dynamicState),
              parentIndex(parentIndex), parent(parent) {}

        CreateInfo(
            uint32_t inheritanceCount, std::optional<uint32_t> pipelineLayout,
            std::optional<std::vector<Shader>> &&shaders, std::optional<VertexLayout> vertexInput,
            const std::optional<InputAssembly> &inputAssembly, std::optional<Tesselation> tesselation,
            const std::optional<ViewportState> &viewportState, std::optional<Rasterizer> rasterizer,
            const std::optional<Multisampling> &multisampling, std::optional<DepthStencil> depthStencil,
            const std::optional<ColorBlending> &colorBlending, std::optional<std::vector<DynamicState>> dynamicState,
            int parentIndex, GraphicsPipeline *parent
        )
            : inheritanceCount(inheritanceCount), pipelineLayout(pipelineLayout), shaders_(std::move(shaders)),
              vertexInput(vertexInput), inputAssembly(inputAssembly), tesselation(tesselation),
              viewportState(viewportState), rasterizer(rasterizer), multisampling(multisampling),
              depthStencil(depthStencil), colorBlending(colorBlending), dynamicState(dynamicState),
              parentIndex(parentIndex), parent(parent) {
            if (shaders_) {
                this->shaders = std::vector<Shader *>(shaders_->size());
                for (int i = 0; i < shaders_->size(); i++) (*this->shaders)[i] = &(*shaders_)[i];
            }
        }

        CreateInfo(CreateInfo &&other) = delete;
        ~CreateInfo() = default;

        CreateInfo &operator=(CreateInfo &&other) = delete;
        CreateInfo &operator=(const CreateInfo &other) = delete;
        CreateInfo(const CreateInfo &other) = delete;

        const uint32_t GetPipelineLayout(const GraphicsPipeline *parent) const;
        const std::vector<Shader *> *GetShaders(const GraphicsPipeline *parent) const;
        const VertexLayout *GetVertexInput(const GraphicsPipeline *parent) const;
        const InputAssembly *GetInputAssembly(const GraphicsPipeline *parent) const;
        const Tesselation *GetTesselation(const GraphicsPipeline *parent) const;
        const ViewportState *GetViewportState(const GraphicsPipeline *parent) const;
        const Rasterizer *GetRasterizer(const GraphicsPipeline *parent) const;
        const Multisampling *GetMultisampling(const GraphicsPipeline *parent) const;
        const DepthStencil *GetDepthStencil(const GraphicsPipeline *parent) const;
        const ColorBlending *GetColorBlending(const GraphicsPipeline *parent) const;
        const std::vector<DynamicState> *GetDynamicState(const GraphicsPipeline *parent) const;
        GraphicsPipelineHandle GetParentHandle(const GraphicsPipeline *parent) const;
        void DecrementParentInheritance();
        void UpdateParentInheritance();
    };

  private:
    GraphicsPipelineHandle m_handle;
    CreateInfo *m_createInfo;
    friend class RenderPass;
};
} // namespace vg
