#include "glm/ext/quaternion_trigonometric.hpp"
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
#define GLM_ENABLE_EXPERIMENTAL
#include "Renderer.h"
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/quaternion.hpp>
#include <math.h>

float randf(float min = 0, float max = 1) { return rand() / (float)RAND_MAX * (max - min) + min; }

GLFWwindow *CreateWindow() {
#ifndef NDEBUG
#ifdef __linux__
    glfwInitHint(GLFW_PLATFORM, GLFW_PLATFORM_X11);
#endif
#endif
    glfwInit();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    // glfwWindowHint(GLFW_DECORATED, GLFW_FALSE);
    return glfwCreateWindow(1920, 1080, "VRendererTest", nullptr, nullptr);
}

vg::Queue generalQueue;
vg::Device renderDevice;
vg::SurfaceHandle InitVulkan(GLFWwindow *window) {
    uint32_t glfwExtensionCount = 0;
    const char **glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);
    vg::instance =
        vg::Instance({glfwExtensions, glfwExtensionCount}, [](vg::MessageSeverity severity, const char *message) {
            if (severity < vg::MessageSeverity::Warning) return;
            std::cout << message << '\n' << '\n';
        });

    vg::SurfaceHandle windowSurface;
    glfwCreateWindowSurface(
        *(VkInstance *)&vg::instance, (GLFWwindow *)window, nullptr, (VkSurfaceKHR *)&windowSurface
    );
    vg::DeviceFeatures deviceFeatures(
        {vg::Feature::LogicOp, vg::Feature::SampleRateShading, vg::Feature::FillModeNonSolid,
         vg::Feature::MultiDrawIndirect}
    );
    generalQueue = vg::Queue({vg::QueueType::General}, 1.0f);
    renderDevice = vg::Device(
        {&generalQueue}, {"VK_KHR_swapchain"}, deviceFeatures, windowSurface,
        [](auto id, auto supportedQueues, auto supportedExtensions, auto type, vg::DeviceLimits limits,
           vg::DeviceFeatures features) { return (type == vg::DeviceType::Dedicated); }
    );
    vg::currentDevice = &renderDevice;
    return windowSurface;
}

int main() {
    auto window = CreateWindow();
    int w, h;
    glfwGetFramebufferSize(window, &w, &h);

    auto windowSurface = InitVulkan(window);

    Material mat1(
        "resources/shaders/shader.vert.spv", "resources/shaders/shader.frag.spv",
        vg::VertexLayout(
            {{0, sizeof(float) * 2}, {1, sizeof(Batch::InstanceMapping), vg::InputRate::Instance}},
            {{0, 0, vg::Format::RG32SFLOAT},
             {1, 1, vg::Format::R32UINT},
             {2, 1, vg::Format::R32UINT, offsetof(Batch::InstanceMapping, batchIndex)}}
        ),
        {.cullMode = vg::CullMode::None},
        vg::SubpassDependency(
            -1, 0, vg::PipelineStage::ColorAttachmentOutput, vg::PipelineStage::ColorAttachmentOutput, 0,
            vg::Access::ColorAttachmentWrite, {}
        ),
        glm::vec4(0.2f, 0.2f, 0.9f, 1.0f)
    );
    Material mat2(&mat1, glm::vec4(0.6f, 0.6f, 0.6f, 1.0f));
    Material mat3(&mat1, glm::vec4(1.0f, 0.0f, 0.0f, 1.0f));
    Material mat4(&mat1, glm::vec4(1.0f, 0.0f, 1.0f, 1.0f));
    Material mat1_1(
        "resources/shaders/shader1.vert.spv", "resources/shaders/shader1.frag.spv",
        vg::VertexLayout(
            {{0, sizeof(float) * 5}, {1, sizeof(Batch::InstanceMapping), vg::InputRate::Instance}},
            {{0, 0, vg::Format::RG32SFLOAT},
             {1, 0, vg::Format::RGB32SFLOAT, sizeof(float) * 2},
             {2, 1, vg::Format::R32UINT},
             {3, 1, vg::Format::R32UINT, offsetof(Batch::InstanceMapping, batchIndex)}}
        ),
        {.cullMode = vg::CullMode::None},
        vg::SubpassDependency(
            0, 1, vg::PipelineStage::ColorAttachmentOutput, vg::PipelineStage::ColorAttachmentOutput, 0,
            vg::Access::ColorAttachmentWrite, {}
        ),
        glm::vec4(-0.5, 0.5, 0.3, 0.3)
    );
    Material mat1_2(&mat1_1, glm::vec4(0.5, 0.5, 0.2, 0.3));

    Renderer::Init(window, &generalQueue, windowSurface, w, h);

    Mesh testMesh(4, new glm::vec2[]{{0, 0}, {1, 0}, {1, 1}, {0, 1}}, 6, new int[]{0, 1, 2, 2, 3, 0});
    Mesh testMesh1(4, new glm::vec2[]{{0, 0}, {0.5, 0}, {1, 1}, {0, 1}}, 6, new int[]{0, 1, 2, 2, 3, 0});
    Mesh testMesh1_1(
        4, sizeof(float) * 5, new float[]{-1, -1, 1, 1, 1, 0, -1, 1, 1, 0, 0, 0, 0, 1, 1, -1, 0, 1, 0, 1}, 6,
        sizeof(int), new int[]{0, 1, 2, 2, 3, 0}
    );

    auto [batchID, batchExists] = Batch::Add(&mat1, &testMesh, sizeof(glm::mat4));
    Batch::Add(&mat2, &testMesh1, sizeof(glm::mat4));
    Batch::AddLOD(batchID, &mat3, &testMesh);
    Batch::AddLOD(batchID, &mat4, &testMesh1);

    const uint objectCount = 5;
    const float spawnScale = 0.1;
    RenderObject renderObjects[objectCount];
    RenderObject renderObjects2[objectCount];
    for (int i = 0; i < objectCount; i++) {
        glm::mat4 matrix = glm::translate(
            glm::scale(glm::mat4(1), glm::vec3(0.03)) *
                glm::rotate(glm::mat4(1), randf(0, 100), glm::vec3(randf(), randf(), randf())),
            glm::vec3(randf(-30, 30), randf(-30, 30), randf(-30, 30)) * spawnScale
        );

        renderObjects[i] = RenderObject(&testMesh, &mat1, matrix);
        renderObjects2[i] = RenderObject(&testMesh1, &mat2, matrix);
    }
    RenderObject renderObject1(&testMesh1_1, &mat1_1, 0.3f);
    RenderObject renderObject2(&testMesh1_1, &mat1_2, 1.0f);

    glm::dvec2 lastMouseP;
    glfwGetCursorPos(window, &lastMouseP.x, &lastMouseP.y);
    glm::vec3 cameraPos(0, 0, 0.1f);
    glm::quat cameraRotation(1, 0, 0, 0);
    glm::mat4 proj = glm::perspective(glm::radians(45.0f), w / (float)h, 0.01f, 100.0f);

    float t = 0;
    int n = 0;
    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();
        if (glfwGetKey(window, GLFW_KEY_ESCAPE)) glfwSetWindowShouldClose(window, true);

        {
            glm::dvec2 mouseP;
            glfwGetCursorPos(window, &mouseP.x, &mouseP.y);
            glm::dvec2 mouseDelta = mouseP - lastMouseP;
            lastMouseP = mouseP;
            mouseDelta *= 0.001f;
            if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_1)) {
                cameraRotation =
                    glm::angleAxis((float)-mouseDelta.x, cameraRotation * glm::vec3(0, 0, 1)) * cameraRotation;
                cameraRotation =
                    glm::angleAxis((float)mouseDelta.y, cameraRotation * glm::vec3(1, 0, 0)) * cameraRotation;
                cameraRotation = glm::normalize(cameraRotation);
            }

            glm::vec3 direction(0.0f);

            if (glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS) direction += glm::vec3(0, 0, -1);
            if (glfwGetKey(window, GLFW_KEY_Q) == GLFW_PRESS) direction += glm::vec3(0, 0, 1);
            if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) direction += glm::vec3(-1, 0, 0);
            if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) direction += glm::vec3(1, 0, 0);
            if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) direction += glm::vec3(0, -1, 0);
            if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) direction += glm::vec3(0, 1, 0);

            if (length(direction) > 0.0f) direction = normalize(direction);

            cameraPos += cameraRotation * direction * 0.01f;
        }
        for (int i = 0; i < objectCount / 10; i++)
            renderObjects[(n + i) % objectCount].SetBatchData(
                glm::translate(
                    renderObjects[(n + i) % objectCount].ReadBatchData<glm::mat4>(),
                    glm::vec3(randf(-0.6, 0.6), randf(-0.6, 0.6), randf(-0.6, 0.6))
                )
            );
        n += objectCount / 10;

        glm::mat4 view = glm::lookAt(
            cameraPos, cameraPos + cameraRotation * glm::vec3(0, 1, 0), cameraRotation * glm::vec3(0, 0, 1)
        );
        // testMesh1.WriteVertexData(
        //     new float[]{float(cos(t * 1.8) * 0.8 - 1), float(sin(t * 1.8) * 0.8 - 1), float(sin(t * 1.8) * 0.5 +
        //     0.5)}, sizeof(float) * 3, 0
        // );
        // if (t == 0) {
        //     testMesh.AppendVertices(new float[]{1.2, 1.2}, sizeof(float) * 2);
        //     testMesh.AppendIndices(new int[]{2, 3, 4}, sizeof(int) * 3);

        //     testMesh1.AppendVertices(new float[]{-1.2, 0.5, 0, 0, 0}, sizeof(float) * 5);
        //     testMesh1.AppendIndices(new int[]{2, 3, 4}, sizeof(int) * 3);
        // }
        // if (t == 0.01f) {
        //     testMesh.EraseIndices(3);
        //     testMesh.EraseVertices(2);
        // }
        t += 0.01;
        // mat3.Write((float)abs(sin(t)));
        Renderer::SetPassData({.viewProjection = proj * view});
        Renderer::RenderFrame();
        Renderer::Present(generalQueue);
    }
    Renderer::Destroy();
    glfwTerminate();
}
