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
#include "DebugRendering.h"
#include "AssimpLoader.h"

float randf(float min = 0, float max = 1) { return rand() / (float)RAND_MAX * (max - min) + min; }

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

    const uint maxFramesInFlight = 2;
    MeshManager meshManager(maxFramesInFlight);
    MaterialManager materialManager(maxFramesInFlight);
    BatchManager batchManager(maxFramesInFlight);
    Renderer renderer(
        maxFramesInFlight, &generalQueue, windowSurface, w, h, {&meshManager, &materialManager, &batchManager}
    );
    currentRenderer = &renderer;

    Material material(
        false, "resources/shaders/shader.vert.spv", "resources/shaders/shader.frag.spv",
        vg::VertexLayout(
            {{0, sizeof(float) * 6}, {1, sizeof(uint), vg::InputRate::Instance}},
            {{0, 0, vg::Format::RGB32SFLOAT},
             {1, 0, vg::Format::RGB32SFLOAT, sizeof(float) * 3},
             {2, 1, vg::Format::R32UINT}}
        ),
        {.cullMode = vg::CullMode::Back},
        vg::SubpassDependency(
            -1, 0, vg::PipelineStage::ColorAttachmentOutput, vg::PipelineStage::ColorAttachmentOutput, 0,
            vg::Access::ColorAttachmentWrite, {}
        ),
        std::make_tuple(glm::vec3(0), 1.0f, glm::vec4(1.0f, 1.0f, 1.0f, 1.0f))
    );
    Debug::Init();

    std::vector<Material> materials;
    std::vector<Mesh> meshes;
    std::vector<RenderObject> renderObjects;
    Load::Model("resources/TreeOnMountain.fbx", &material, &renderObjects, &materials, &meshes);

    glm::vec3 cameraPos(0, -1, 0);
    glm::quat cameraRotation(1, 0, 0, 0);
    glm::mat4 proj = glm::perspective(glm::radians(90.0f), w / (float)h, 0.01f, 1000.0f);
    proj[1][1] *= -1;

    glm::mat4 lightProj = glm::ortho(-100.f, 140.f, -100.f, 100.f, -400.f, 400.f);
    glm::mat4 lightView = glm::lookAt(glm::vec3(100), glm::vec3(0), glm::vec3(0, 0, 1));
    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();
        if (glfwGetKey(window, GLFW_KEY_ESCAPE)) glfwSetWindowShouldClose(window, true);

        cameraRotation = GetRotation(window, cameraRotation, 0.001f);
        cameraPos += cameraRotation * GetMoveDirection(window, 0.4f);

        glm::mat4 view = glm::lookAt(
            cameraPos, cameraPos + cameraRotation * glm::vec3(0, 1, 0), cameraRotation * glm::vec3(0, 0, 1)
        );

        static float t = 0;
        t += 0.01;
        Debug::color = glm::vec4(0, 0, 1, 0.5);
        Debug::DrawSphere(glm::vec3(2, 4, 0), 1);

        Debug::color = glm::vec4(0, 1, 0, 0.5);
        Debug::DrawSphere(glm::vec3(2, 2, 0), 1);

        Debug::color = glm::vec4(1, 0, 0, 0.5);
        Debug::DrawSphere(glm::vec3(2, 0, 0), 1);

        Debug::color = glm::vec4(1);
        Debug::DrawSphere(glm::vec3(2, 0, -sin(t) * 4 - 6), 1);
        Debug::DrawSphere(cameraPos, 1);
        Debug::DrawArrow(glm::vec3(0), glm::normalize(glm::vec3(1)));

        Debug::color = glm::vec4(1, 0, 0, 1);
        Debug::DrawArrow(glm::vec3(0), glm::vec3(1, 0, 0));
        Debug::color = glm::vec4(0, 1, 0, 1);
        Debug::DrawArrow(glm::vec3(0), glm::vec3(0, 1, 0));
        Debug::color = glm::vec4(0, 0, 1, 1);
        Debug::DrawArrow(glm::vec3(0), glm::vec3(0, 0, 1));

        Debug::Frame();
        renderer.RenderFrame(
            generalQueue, proj * view,
            {.lightViewProjection = lightProj * lightView,
             .cameraPosition = cameraPos,
             .lightDirection = glm::vec3(-1),
             .lightColor = glm::vec3(1, 1, 1)}
        );
    }
    // Debug::Destroy();
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
        std::cout << message << '\n' << '\n';
        exit(0);
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
glm::vec3 GetMoveDirection(GLFWwindow *window, float speed) {
    glm::vec3 direction(0);
    if (glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS) direction += glm::vec3(0, 0, 1);
    if (glfwGetKey(window, GLFW_KEY_Q) == GLFW_PRESS) direction += glm::vec3(0, 0, -1);
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) direction += glm::vec3(-1, 0, 0);
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) direction += glm::vec3(1, 0, 0);
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) direction += glm::vec3(0, -1, 0);
    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) direction += glm::vec3(0, 1, 0);

    if (length(direction) > 0.0f) direction = normalize(direction);
    return direction * speed;
}

glm::quat GetRotation(GLFWwindow *window, glm::quat cameraRotation, float speed) {
    static glm::dvec2 lastMouseP(-100, -100);
    if (lastMouseP == glm::dvec2(-100, -100)) glfwGetCursorPos(window, &lastMouseP.x, &lastMouseP.y);

    glm::dvec2 mouseP;
    glfwGetCursorPos(window, &mouseP.x, &mouseP.y);
    glm::dvec2 mouseDelta = mouseP - lastMouseP;
    lastMouseP = mouseP;
    mouseDelta = glm::dvec2(0, 0);
    if (glfwGetKey(window, GLFW_KEY_I) == GLFW_PRESS) mouseDelta -= glm::dvec2(0, 1);
    if (glfwGetKey(window, GLFW_KEY_K) == GLFW_PRESS) mouseDelta += glm::dvec2(0, 1);
    if (glfwGetKey(window, GLFW_KEY_J) == GLFW_PRESS) mouseDelta -= glm::dvec2(1, 0);
    if (glfwGetKey(window, GLFW_KEY_L) == GLFW_PRESS) mouseDelta += glm::dvec2(1, 0);
    mouseDelta *= speed * 10;

    // if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_1))
    {
        cameraRotation = glm::angleAxis((float)-mouseDelta.x, cameraRotation * glm::vec3(0, 0, 1)) * cameraRotation;
        cameraRotation = glm::angleAxis((float)-mouseDelta.y, cameraRotation * glm::vec3(1, 0, 0)) * cameraRotation;
        cameraRotation = glm::normalize(cameraRotation);
    }

    return cameraRotation;
}
