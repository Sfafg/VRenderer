#include "glm/ext/quaternion_trigonometric.hpp"
#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
extern "C" {
typedef struct VkInstance_T *VkInstance;
typedef struct VkSurfaceKHR_T *VkSurfaceKHR;
int glfwCreateWindowSurface(VkInstance instance, GLFWwindow *window, const void *allocator, VkSurfaceKHR *surface);
}
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <iostream>
#define GLM_ENABLE_EXPERIMENTAL
#include "Renderer.h"
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/quaternion.hpp>
#include <math.h>

float randf(float min = 0, float max = 1) { return rand() / (float)RAND_MAX * (max - min) + min; }

GLFWwindow *CreateWindow() {
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
            // TODO: Zrobić żeby errory były bardziej kompaktowe np.
            // Validation Error: [ VUID-vkCmdDrawIndexed-None-08114 ] Object 0: handle = 0x2cfba2000000001c, type =
            // VK_OBJECT_TYPE_DESCRIPTOR_SET; | Mess
            // ageID = 0x3f6fd836 | Descriptor set VkDescriptorSet 0x2cfba2000000001c[] encountered the following
            // validation error at vkCmdDrawIndexed ti me: Descriptor in binding #1 index 0 is using buffer VkBuffer
            // 0xcb3ee80000000007[] that is invalid or has been destroyed. The Vulkan spec states: Descriptors in each
            // bound descriptor set, specified via vkCmdBindDescriptorSets, must be valid if they are statically used by
            // the VkPipeline bound to the pipeline bind point used by this command and the bound VkPipeline was not
            // created with VK_PIPELINE_CREATE_DESCRIPT OR_BUFFER_BIT_EXT
            // (https://vulkan.lunarg.com/doc/view/1.3.261.1/windows/1.3-extensions/vkspec.html#VUID-vkCmdDrawIndexed-None-08114)
            // na
            // Validation Error: [vkCmdDrawIndexed] handle = 0x2cfba2000000001c, type =
            // VK_OBJECT_TYPE_DESCRIPTOR_SET;
            // Descriptor set VkDescriptorSet 0x2cfba2000000001c[] encountered the following
            // validation error at vkCmdDrawIndexed ti me: Descriptor in binding #1 index 0 is using buffer VkBuffer
            // 0xcb3ee80000000007[] that is invalid or has been destroyed. Descriptors in each
            // bound descriptor set, specified via vkCmdBindDescriptorSets, must be valid if they are statically used by
            // the VkPipeline bound to the pipeline bind point used by this command and the bound VkPipeline was not
            // created with VK_PIPELINE_CREATE_DESCRIPT OR_BUFFER_BIT_EXT
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
        {&generalQueue}, {"VK_KHR_swapchain", "VK_EXT_sampler_filter_minmax"}, deviceFeatures, windowSurface,
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
            {vg::VertexBinding(0, sizeof(float) * 2), vg::VertexBinding(1, sizeof(glm::mat4), vg::InputRate::Instance)},
            {vg::VertexAttribute(0, 0, vg::Format::RG32SFLOAT), vg::VertexAttribute(1, 1, vg::Format::RGBA32SFLOAT),
             vg::VertexAttribute(2, 1, vg::Format::RGBA32SFLOAT, sizeof(glm::vec4)),
             vg::VertexAttribute(3, 1, vg::Format::RGBA32SFLOAT, sizeof(glm::vec4) * 2),
             vg::VertexAttribute(4, 1, vg::Format::RGBA32SFLOAT, sizeof(glm::vec4) * 3)}
        ),
        {.cullMode = vg::CullMode::None},
        vg::SubpassDependency(
            -1, 0, vg::PipelineStage::ColorAttachmentOutput, vg::PipelineStage::ColorAttachmentOutput, 0,
            vg::Access::ColorAttachmentWrite, {}
        ),
        0.2f
    );

    Material mat2(
        "resources/shaders/shader1.vert.spv", "resources/shaders/shader1.frag.spv",
        vg::VertexLayout(
            {vg::VertexBinding(0, sizeof(float) * 5)},
            {{0, 0, vg::Format::RG32SFLOAT}, {1, 0, vg::Format::RGB32SFLOAT, sizeof(float) * 2}}
        ),
        {.cullMode = vg::CullMode::None},
        vg::SubpassDependency(
            0, 1, vg::PipelineStage::ColorAttachmentOutput, vg::PipelineStage::ColorAttachmentOutput, 0,
            vg::Access::ColorAttachmentWrite, {}
        ),
        glm::vec4(-0.5, 0.5, 0.3, 0.3)
    );
    Material mat3(&mat1, 0.6f);
    Material mat4(&mat2, glm::vec4(0.5, 0.5, 0.2, 0.3));

    Renderer::Init(window, &generalQueue, windowSurface, w, h);

    Mesh testMesh(4, new glm::vec2[]{{0, 0}, {1, 0}, {1, 1}, {0, 1}}, 6, new int[]{0, 1, 2, 2, 3, 0});
    Mesh testMesh2(4, new glm::vec2[]{{0, 0}, {0.5, 0}, {1, 1}, {0, 1}}, 6, new int[]{0, 1, 2, 2, 3, 0});
    Mesh testMesh1(
        4, sizeof(float) * 5, new float[]{-1, -1, 1, 1, 1, 0, -1, 1, 1, 0, 0, 0, 0, 1, 1, -1, 0, 1, 0, 1}, 6,
        sizeof(int), new int[]{0, 1, 2, 2, 3, 0}
    );

    glm::dvec2 lastMouseP;
    glfwGetCursorPos(window, &lastMouseP.x, &lastMouseP.y);
    glm::vec3 cameraPos(0, 0, 0.1f);
    glm::quat cameraRotation(1, 0, 0, 0);
    glm::mat4 proj = glm::perspective(glm::radians(45.0f), w / (float)h, 0.01f, 100.0f);
    // proj[1][1] *= -1;

    vg::Buffer instanceBuffer(sizeof(glm::mat4) * 1000, vg::BufferUsage::VertexBuffer);
    vg::Allocate(instanceBuffer, vg::MemoryProperty::HostVisible);
    glm::mat4 *instanceData = (glm::mat4 *)instanceBuffer.MapMemory();
    for (int i = 0; i < 1000; i++) {
        instanceData[i] = glm::translate(
            glm::rotate(glm::mat4(1), randf(0, 100), glm::vec3(randf(), randf(), randf())),
            glm::vec3(randf(-10, 10), randf(-10, 10), randf(-10, 10))
        );
    }

    float t = 0;
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

        glm::mat4 view = glm::lookAt(
            cameraPos, cameraPos + cameraRotation * glm::vec3(0, 1, 0), cameraRotation * glm::vec3(0, 0, 1)
        );
        t += 0.01;
        Material::materialBuffer.Write(mat3.index, (float)sin(t), Material::materialBuffer.Size(mat3.index) * 0);
        Renderer::SetPassData({.viewProjection = proj * view});
        Renderer::StartFrame();
        Renderer::Draw(testMesh, mat1, instanceBuffer, 1000);
        Renderer::Draw(testMesh2, mat3, instanceBuffer, 1000);
        Renderer::Draw(testMesh2, mat4);
        Renderer::Draw(testMesh1, mat2);
        Renderer::EndFrame();
        Renderer::Present(generalQueue);
    }
    Renderer::Destroy();
    glfwTerminate();
}
