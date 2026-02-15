#include "glm/fwd.hpp"
#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
extern "C" {
typedef struct VkInstance_T *VkInstance;
typedef struct VkSurfaceKHR_T *VkSurfaceKHR;
int glfwCreateWindowSurface(VkInstance instance, GLFWwindow *window, const void *allocator, VkSurfaceKHR *surface);
}
#include <iostream>
#include <span>
#include "Renderer.h"

struct RAIIGLFW {
    RAIIGLFW();
    ~RAIIGLFW();
};

GLFWwindow *CreateWindow();
vg::SurfaceHandle CreateWindowSurface(GLFWwindow *window);
vg::Instance CreateInstance();
vg::Device CreateDevice(vg::SurfaceHandle windowSurface, vg::Queue &generalQueue);
glm::vec3 GetMoveDirection(GLFWwindow *window, float speed = 0.4f);
glm::quat GetRotation(GLFWwindow *window, glm::quat cameraRotation, float speed = 0.001f);

void Check(
    const std::string &test, BatchArray &batchArray, uint transparencyBucketCount, uint firstTransparentDrawCall,
    uint transparentDrawCallsCount, uint totalObjects, const std::vector<BatchArray::DrawCall> &drawCalls,
    const std::vector<std::tuple<uint, uint>> &drawCallMaterialIndices, const std::vector<uint> &instanceMappingData,
    const std::vector<BatchArray::Batch> &batches, const std::vector<char> &objectData
) {
    std::string s;
    std::string extra;

    try {
        if (!(transparencyBucketCount == batchArray.transparencyBucketCount &&
              firstTransparentDrawCall == batchArray.firstTransparentDrawCall &&
              transparentDrawCallsCount == batchArray.transparentDrawCallsCount &&
              totalObjects == batchArray.totalObjects)) {
            s = ", Counter mismatch";

            extra += "\tTransparencyBucketCount: " + std::to_string(batchArray.transparencyBucketCount) +
                     "\n\tFirstTransparentDrawCall: " + std::to_string(batchArray.firstTransparentDrawCall) +
                     "\n\tTransparentDrawCallsCount: " + std::to_string(batchArray.transparentDrawCallsCount) +
                     "\n\tTotalObjects: " + std::to_string(batchArray.totalObjects) + "\n";
        }

        if (batchArray.drawCalls.size() != drawCalls.size() ||
            memcmp(batchArray.drawCalls.data(), drawCalls.data(), sizeof(drawCalls[0]) * drawCalls.size()) != 0) {
            s += ", DrawCall mismatch";
            for (auto &&d : batchArray.drawCalls) {

                extra += std::string("DrawCall:\n") + std::string("\tIndexCount: ") + std::to_string(d.indexCount) +
                         std::string("\n") + std::string("\tInstanceCount: ") + std::to_string(d.instanceCount) +
                         std::string("\n") + std::string("\tFirstIndex: ") + std::to_string(d.firstIndex) +
                         std::string("\n") + std::string("\tVertexOffset: ") + std::to_string(d.vertexOffset) +
                         std::string("\n") + std::string("\tFirstInstance: ") + std::to_string(d.firstInstance) +
                         std::string("\n") + std::string("\tMaterialIndex: ") + std::to_string(d.materialIndex) +
                         std::string("\n") + std::string("\tMeshIndex: ") + std::to_string(d.meshIndex) +
                         std::string("\n\n");
            }
        }

        if (batchArray.drawCallMaterialIndices.size() != drawCallMaterialIndices.size() ||
            memcmp(
                batchArray.drawCallMaterialIndices.data(), drawCallMaterialIndices.data(),
                sizeof(std::tuple<uint, uint>) * drawCallMaterialIndices.size()
            ) != 0) {
            s += ", drawCallMaterialIndices mismatch";

            for (auto &&d : batchArray.drawCallMaterialIndices) {

                extra += std::string("MaterialIndex:\n") + std::string("\tget<0>: ") + std::to_string(std::get<0>(d)) +
                         std::string("\n") + std::string("\tget<1>: ") + std::to_string(std::get<1>(d)) +
                         std::string("\n\n");
            }
        }
        if (batchArray.instanceMappingBuffer.GetSize() != instanceMappingData.size() * sizeof(uint) ||
            memcmp(
                batchArray.instanceMappingBuffer.GetData(), instanceMappingData.data(),
                sizeof(uint) * instanceMappingData.size()
            ) != 0) {
            s += ", instanceMappingData mismatch";
            extra += std::string("InstanceMapping:\n");
            auto data = (uint *)batchArray.instanceMappingBuffer.GetData();
            for (auto &&d : std::span(data, data + batchArray.instanceMappingBuffer.GetSize() / sizeof(uint))) {
                extra += std::to_string(d) + std::string(", ");
            }
            extra = extra.substr(0, extra.size() - 1);
            extra += "\n";
        }
        if (batchArray.batches.size() != batches.size() ||
            memcmp(batchArray.batches.data(), batches.data(), sizeof(BatchArray::Batch) * batches.size()) != 0) {
            s += ", batches mismatch";

            int id = 0;
            for (auto &&d : batchArray.batches) {

                if (memcmp(&d, &batches[id++], sizeof(BatchArray::Batch)) != 0)
                    extra += std::string("Batch:") + std::to_string(id - 1) + std::string("\n\tObjectDataOffset: ") +
                             std::to_string(d.objectDataOffset) + std::string("\n\tFirstObjectIndex: ") +
                             std::to_string(d.firstObjectIndex) + std::string("\n\tObjectDataElementSize: ") +
                             std::to_string(d.objectDataElementSize) + std::string("\n\tDrawCall: ") +
                             std::to_string(d.drawCall) + std::string("\n\n");
            }
        }
        if (batchArray.objectBuffer.GetSize() != objectData.size() ||
            memcmp(batchArray.objectBuffer.GetData(), objectData.data(), objectData.size()) != 0) {
            s += ", objectBuffer mismatch";

            extra += "BatchData: ";
            char *p = (char *)batchArray.objectBuffer.GetData();
            for (auto &&d : std::span(p, p + batchArray.objectBuffer.GetSize())) {
                extra += std::to_string((int)d) + std::string(",");
            }
            extra = extra.substr(0, extra.size() - 1);
            extra += "\n";
        }

        if (extra.size() != 0) std::cout << extra << "\n";
        std::cout << test << ": ";
        if (s.size() == 0) std::cout << "\033[32mPassed.\033[0m\n";
        else {
            std::cout << "\033[31mFailed:\n\t" << s.substr(2) << ".\033[0m\n\n";
            exit(1);
            std::cout << "\033[31mFailed:\n\t" << s.substr(2) << ".\033[0m\n\n";
        }
    } catch (...) {
        std::cout << test << ": ";
        std::cout << "\033[31mFailed.\n\t\033[0m\n\n";
        // throw;
    }
}

int main() {
    RAIIGLFW raiiGLFW;
    auto window = CreateWindow();
    int w, h;
    glfwGetFramebufferSize(window, &w, &h);

    auto instance = CreateInstance();
    vg::instance = &instance;
    auto windowSurface = CreateWindowSurface(window);
    auto generalQueue = vg::Queue({vg::QueueType::General}, 1.0f);
    auto renderDevice = CreateDevice(windowSurface, generalQueue);
    vg::currentDevice = &renderDevice;
    std::cout << "\n\n\n";

    const uint maxFramesInFlight = 2;
    MeshArray meshManager(maxFramesInFlight);
    MaterialArray materialArray(maxFramesInFlight);
    BatchArray batchManager(maxFramesInFlight, 5);
    Renderer renderer(
        maxFramesInFlight, &generalQueue, windowSurface, w, h, {&meshManager, &materialArray, &batchManager}
    );
    Material::materialArray = &materialArray;
    renderer.MakeCurrent();

    Mesh mesh(glm::vec3(-1), glm::vec3(1), 1, new glm::vec3(0), 3, new glm::ivec3(0, 1, 2));
    Mesh mesh1(glm::vec3(-1), glm::vec3(1), 1, new glm::vec3(0), 3, new glm::ivec3(0, 1, 2));
    Material material(
        false, "resources/shaders/shader.vert.spv", "resources/shaders/shader.frag.spv",
        vg::VertexLayout(
            {{0, sizeof(float) * 6}, {1, sizeof(uint), vg::InputRate::Instance}},
            {{0, 0, vg::Format::RGB32SFLOAT},
             {1, 0, vg::Format::RGB32SFLOAT, sizeof(float) * 3},
             {2, 1, vg::Format::R32UINT}}
        ),
        {.cullMode = vg::CullMode::Back}, std::make_tuple(glm::vec3(0), 1.0f, glm::vec4(1.0f, 1.0f, 1.0f, 1.0f))
    );
    Material material1(
        true, "resources/shaders/shader.vert.spv", "resources/shaders/shader.frag.spv",
        vg::VertexLayout(
            {{0, sizeof(float) * 6}, {1, sizeof(uint), vg::InputRate::Instance}},
            {{0, 0, vg::Format::RGB32SFLOAT},
             {1, 0, vg::Format::RGB32SFLOAT, sizeof(float) * 3},
             {2, 1, vg::Format::R32UINT}}
        ),
        {.cullMode = vg::CullMode::Back}, std::make_tuple(glm::vec3(0), 1.0f, glm::vec4(1.0f, 1.0f, 1.0f, 1.0f))
    );

    Check("Base case", batchManager, 5, 0, 0, 0, {}, {}, {}, {}, {});

    batchManager.Add(&mesh, &material, 4);
    Check(
        "Add case", batchManager, 5, 1, 0, 0, {{0, 0, 0, 0, 0, 0, 0}}, {{0, 0}}, {},
        {// First Batch.
         {0, 0, 4, 0, {-1U, -1U, -1U, -1U}}
        },
        {}
    );

    batchManager.Add(&mesh, &material1, 8);
    Check(
        "Transparent Add case", batchManager, 5, 1, 1, 0,
        {
            // First DrawCall.
            {0, 0, 0, 0, 0, 0, 0},
            // Transparent DrawCalls.
            {0, 0, 0, 0, 0, 1, 0},
            {0, 0, 0, 0, 0, 1, 0},
            {0, 0, 0, 0, 0, 1, 0},
            {0, 0, 0, 0, 0, 1, 0},
            {0, 0, 0, 0, 0, 1, 0},
        },
        {{0, 0}, {1, 0}, {1, 0}, {1, 0}, {1, 0}, {1, 0}}, {},
        {// First Batch.
         {0, 0, 4, 0, {-1U, -1U, -1U, -1U}},
         // Transparent Batch.
         {0, 0, 8, 1, {-1U, -1U, -1U, -1U}}
        },
        {}
    );

    batchManager.Add(&mesh1, &material, 2);
    Check(
        "Add again case", batchManager, 5, 2, 1, 0,
        {
            // First DrawCall.
            {0, 0, 0, 0, 0, 0, 0},
            // Second DrawCall.
            {0, 0, 0, 0, 0, 0, 1},
            // Transparent DrawCalls.
            {0, 0, 0, 0, 0, 1, 0},
            {0, 0, 0, 0, 0, 1, 0},
            {0, 0, 0, 0, 0, 1, 0},
            {0, 0, 0, 0, 0, 1, 0},
            {0, 0, 0, 0, 0, 1, 0},
        },
        {{0, 0}, {0, 0}, {1, 0}, {1, 0}, {1, 0}, {1, 0}, {1, 0}}, {},
        {// First batch.
         {0, 0, 4, 0, {-1U, -1U, -1U, -1U}},
         // Transparent batch.
         {0, 0, 8, 2, {-1U, -1U, -1U, -1U}},
         // Second batch.
         {0, 0, 2, 1, {-1U, -1U, -1U, -1U}}
        },
        {}
    );

    batchManager.Remove(batchManager.Get(&mesh1, &material));
    Check(
        "Remove case", batchManager, 5, 1, 1, 0,
        {
            // First DrawCall.
            {0, 0, 0, 0, 0, 0, 0},
            // Transparent DrawCalls.
            {0, 0, 0, 0, 0, 1, 0},
            {0, 0, 0, 0, 0, 1, 0},
            {0, 0, 0, 0, 0, 1, 0},
            {0, 0, 0, 0, 0, 1, 0},
            {0, 0, 0, 0, 0, 1, 0},
        },
        {{0, 0}, {1, 0}, {1, 0}, {1, 0}, {1, 0}, {1, 0}}, {},
        {// First Batch.
         {0, 0, 4, 0, {-1U, -1U, -1U, -1U}},
         // Transparent Batch.
         {0, 0, 8, 1, {-1U, -1U, -1U, -1U}}
        },
        {}
    );

    batchManager.Add(&mesh1, &material, 2);
    Check(
        "Add after remove case", batchManager, 5, 2, 1, 0,
        {
            // First DrawCall.
            {0, 0, 0, 0, 0, 0, 0},
            // Second DrawCall.
            {0, 0, 0, 0, 0, 0, 1},
            // Transparent DrawCalls.
            {0, 0, 0, 0, 0, 1, 0},
            {0, 0, 0, 0, 0, 1, 0},
            {0, 0, 0, 0, 0, 1, 0},
            {0, 0, 0, 0, 0, 1, 0},
            {0, 0, 0, 0, 0, 1, 0},
        },
        {{0, 0}, {0, 0}, {1, 0}, {1, 0}, {1, 0}, {1, 0}, {1, 0}}, {},
        {// First batch.
         {0, 0, 4, 0, {-1U, -1U, -1U, -1U}},
         // Transparent batch.
         {0, 0, 8, 2, {-1U, -1U, -1U, -1U}},
         // Second batch.
         {0, 0, 2, 1, {-1U, -1U, -1U, -1U}}
        },
        {}
    );

    batchManager.ReserveObjects(batchManager.Get(&mesh1, &material), 2);
    Check(
        "Reserve case", batchManager, 5, 2, 1, 0,
        {
            // First DrawCall.
            {0, 0, 0, 0, 0, 0, 0},
            // Second DrawCall.
            {0, 0, 0, 0, 0, 0, 1},
            // Transparent DrawCalls.
            {0, 0, 0, 0, 2, 1, 0},
            {0, 0, 0, 0, 2, 1, 0},
            {0, 0, 0, 0, 2, 1, 0},
            {0, 0, 0, 0, 2, 1, 0},
            {0, 0, 0, 0, 2, 1, 0},
        },
        {{0, 0}, {0, 0}, {1, 0}, {1, 0}, {1, 0}, {1, 0}, {1, 0}}, {0, 0},
        {{0, 0, 4, 0, {-1U, -1U, -1U, -1U}}, {0, 0, 8, 2, {-1U, -1U, -1U, -1U}}, {0, 0, 2, 1, {-1U, -1U, -1U, -1U}}},
        {0, 0, 0, 0}
    );

    batchManager.ReserveObjects(batchManager.Get(&mesh, &material1), 2);
    Check(
        "Reserve transparent case", batchManager, 5, 2, 1, 0,
        {
            // First DrawCall.
            {0, 0, 0, 0, 0, 0, 0},
            // Second DrawCall.
            {0, 0, 0, 0, 0, 0, 1},
            // Transparent DrawCalls.
            {0, 0, 0, 0, 2, 1, 0},
            {0, 0, 0, 0, 4, 1, 0},
            {0, 0, 0, 0, 6, 1, 0},
            {0, 0, 0, 0, 8, 1, 0},
            {0, 0, 0, 0, 10, 1, 0},
        },
        {{0, 0}, {0, 0}, {1, 0}, {1, 0}, {1, 0}, {1, 0}, {1, 0}}, {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
        {{0, 0, 4, 0, {-1U, -1U, -1U, -1U}}, {0, 0, 8, 2, {-1U, -1U, -1U, -1U}}, {8, 0, 2, 1, {-1U, -1U, -1U, -1U}}},
        {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}
    );

    // If it will not be reserved to 0 then the data will persist, which is an expected behaviour.
    batchManager.ReserveObjects(batchManager.Get(&mesh, &material1), 0);
    batchManager.Remove(batchManager.Get(&mesh, &material1));
    Check(
        "Remove transparent with reserved data case", batchManager, 5, 2, 0, 0,
        {
            // First DrawCall.
            {0, 0, 0, 0, 0, 0, 0},
            // Second DrawCall.
            {0, 0, 0, 0, 0, 0, 1},
        },
        {{0, 0}, {0, 0}}, {0, 0}, {{0, 0, 4, 0, {-1U, -1U, -1U, -1U}}, {0, 0, 2, 1, {-1U, -1U, -1U, -1U}}}, {0, 0, 0, 0}
    );

    // static void SetLOD(uint batchIndex, const std::vector<std::tuple<class Mesh *, class Material *>> &lods);
}

RAIIGLFW::RAIIGLFW() {
#ifndef NDEBUG
#ifdef __linux__
    glfwInitHint(GLFW_PLATFORM, GLFW_PLATFORM_X11);
#endif
#endif
    glfwInit();
}

RAIIGLFW::~RAIIGLFW() { glfwTerminate(); }

GLFWwindow *CreateWindow() {
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    return glfwCreateWindow(1920, 1080, "VRendererTest", nullptr, nullptr);
}

vg::SurfaceHandle CreateWindowSurface(GLFWwindow *window) {
    vg::SurfaceHandle windowSurface;
    glfwCreateWindowSurface(*(VkInstance *)vg::instance, (GLFWwindow *)window, nullptr, (VkSurfaceKHR *)&windowSurface);
    return windowSurface;
}
vg::Instance CreateInstance() {
    uint32_t glfwExtensionCount = 0;
    const char **glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);
    return vg::Instance({glfwExtensions, glfwExtensionCount}, [](vg::MessageSeverity severity, const char *message) {
        if (severity < vg::MessageSeverity::Warning) return;

        switch (severity) {
        case vg::MessageSeverity::Info: std::cout << "Info: "; break;
        case vg::MessageSeverity::Warning: std::cout << "Warning: "; break;
        case vg::MessageSeverity::Error: std::cout << "Error: "; break;
        default: break;
        }

        std::cout << message << '\n' << '\n';
    });
}
vg::Device CreateDevice(vg::SurfaceHandle windowSurface, vg::Queue &generalQueue) {
    vg::DeviceFeatures deviceFeatures(
        {vg::Feature::LogicOp, vg::Feature::SampleRateShading, vg::Feature::FillModeNonSolid,
         vg::Feature::MultiDrawIndirect}
    );
    return vg::Device(
        {&generalQueue}, {"VK_KHR_swapchain"}, deviceFeatures, windowSurface,
        [](auto id, auto supportedQueues, auto supportedExtensions, auto type, vg::DeviceLimits limits,
           vg::DeviceFeatures features) { return (type == vg::DeviceType::Dedicated); }
    );
}
