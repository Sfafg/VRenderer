#include "RenderGraph.h"
#include "CmdBuffer.h"
#include "Enums.h"
#include "Flags.h"
#include "Handle.h"
#include "Shader.h"
#include "Structs.h"
#include "Synchronization.h"
#include <cstddef>
#include <tuple>
#include <unordered_map>
#include <vector>

RenderGraph::RenderGraph() {}

void RenderGraph::AddResource(Resource &&resource) { resources[resource.name] = std::move(resource); }

void RenderGraph::AddPass(Pass &&pass) { passes.emplace_back(std::move(pass)); }

bool RenderGraph::Compile() {

    std::vector<std::vector<int>> dependencies(passes.size());
    std::vector<std::vector<int>> dependents(passes.size());

    std::unordered_map<std::string, int> resourceWriters;

    for (int i = 0; i < passes.size(); i++) {
        const auto &pass = passes[i];

        for (const auto &input : pass.inputResources) {
            auto it = resourceWriters.find(input);
            if (it != resourceWriters.end()) {
                dependencies[i].push_back(it->second);
                dependents[it->second].push_back(i);
            }
        }

        for (const auto &output : pass.outputResources) resourceWriters[output] = i;
    }

    std::vector<bool> visited(passes.size(), false);
    std::vector<bool> inStack(passes.size(), false);

    auto visit = [&](this auto &&self, int node) {
        if (inStack[node]) return false; // Cyclic dependency.
        if (visited[node]) return true;

        inStack[node] = true;

        for (auto dependent : dependents[node]) self(dependent);

        inStack[node] = false;
        visited[node] = true;
        executionOrder.push_back(node);

        return true;
    };

    for (int i = 0; i < passes.size(); i++)
        if (!visited[i])
            if (!visit(i)) return false;

    for (int i = 0; i < passes.size(); i++) {
        for (auto dep : dependencies[i]) {
            semaphores.emplace_back(vg::Semaphore());
            semaphoreSignalWaitPairs.emplace_back(dep, i);
        }
    }

    for (auto &[name, resources] : resources) {
        // allocate stuff
    }

    return true;
}

void RenderGraph::Execute(vg::CmdBuffer &cmdBuffer, vg::Queue *queue) {
    std::vector<vg::SemaphoreHandle> waitSemaphores;
    std::vector<vg::Flags<vg::PipelineStage>> waitStages;
    std::vector<vg::SemaphoreHandle> signalSemaphores;

    for (auto passIndex : executionOrder) {
        const auto &pass = passes[passIndex];

        waitSemaphores.clear();
        waitStages.clear();
        signalSemaphores.clear();

        for (int i = 0; i < semaphoreSignalWaitPairs.size(); i++) {
            if (semaphoreSignalWaitPairs[i].second == passIndex) {
                waitSemaphores.push_back(semaphores[i]);
                waitStages.push_back(vg::PipelineStage::ColorAttachmentOutput);
            } else if (semaphoreSignalWaitPairs[i].first == passIndex) {
                signalSemaphores.push_back(semaphores[i]);
            }
        }

        cmdBuffer.Begin();

        for (const auto &input : pass.inputResources) {
            auto &resource = resources[input];

            if (resource.type == Resource::Type::Image) {

                vg::ImageMemoryBarrier barrier(
                    resource.image, resource.initialLayout, resource.finalLayout, vg::Access::MemoryWrite,
                    vg::Access::ShaderRead,
                    vg::ImageSubresource(vg::ImageAspect::Color, 0, resource.image.GetMipLevels())
                );

                cmdBuffer.Append(
                    vg::cmd::PipelineBarier(
                        vg::PipelineStage::AllCommands, vg::PipelineStage::FragmentShader, vg::Dependency::ByRegion,
                        {barrier}
                    )
                );
            }
        }

        for (const auto &output : pass.outputResources) {
            auto &resource = resources[output];

            if (resource.type == Resource::Type::Image) {

                vg::ImageMemoryBarrier barrier(
                    resource.image, resource.initialLayout, vg::ImageLayout::ShaderReadOnlyOptimal,
                    vg::Access::MemoryRead, vg::Access::ShaderRead,
                    vg::ImageSubresource(vg::ImageAspect::Color, 0, resource.image.GetMipLevels())
                );

                cmdBuffer.Append(
                    vg::cmd::PipelineBarier(
                        vg::PipelineStage::AllCommands, vg::PipelineStage::ColorAttachmentOutput,
                        vg::Dependency::ByRegion, {barrier}
                    )
                );
            }
        }

        pass.execute(cmdBuffer);

        for (const auto &output : pass.outputResources) {
            auto &resource = resources[output];

            if (resource.type == Resource::Type::Image) {

                vg::ImageMemoryBarrier barrier(
                    resource.image, resource.initialLayout, vg::ImageLayout::ColorAttachmentOptimal,
                    vg::Access::MemoryRead, vg::Access::MemoryRead,
                    vg::ImageSubresource(vg::ImageAspect::Color, 0, resource.image.GetMipLevels())
                );

                cmdBuffer.Append(
                    vg::cmd::PipelineBarier(
                        vg::PipelineStage::AllCommands, vg::PipelineStage::ColorAttachmentOutput,
                        vg::Dependency::ByRegion, {barrier}
                    )
                );
            }
        }

        cmdBuffer.End();
    }

    std::vector<std::tuple<vg::Flags<vg::PipelineStage>, vg::SemaphoreHandle>> waitStages_;
    for (int i = 0; i < waitStages.size(); i++) waitStages_.push_back({waitStages[i], waitSemaphores[i]});

    queue->Submit(vg::SubmitInfo(waitStages_, signalSemaphores, {cmdBuffer}));
}
Resource &RenderGraph::GetResource(const std::string &name) { return resources.at(name); }
const Resource &RenderGraph::GetResource(const std::string &name) const { return resources.at(name); }
