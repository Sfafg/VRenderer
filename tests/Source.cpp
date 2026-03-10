#include "Handle.h"

#include "glm/ext/quaternion_trigonometric.hpp"
#include "glm/fwd.hpp"
#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#include <fstream>
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

// TODO: LOD support.
// TODO: MultiDrawIndirect.
// TODO: Fix occlusion culling stability (probably reduction sampling).
// TODO: Multiple windows support.
// TODO: Optymize.

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
    MeshArray meshManager(maxFramesInFlight);
    MaterialArray materialArray(maxFramesInFlight);
    BatchArray batchManager(maxFramesInFlight, 50);
    Renderer renderer(
        maxFramesInFlight, &generalQueue, windowSurface, w, h, {&meshManager, &materialArray, &batchManager}
    );
    renderer.MakeCurrent();

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
    Debug::Init();

    std::vector<Material> materials;
    std::vector<Mesh> meshes;
    std::vector<RenderObject> renderObjects;
    Load::Model("resources/TreeOnMountain.fbx", &material, &renderObjects, &materials, &meshes);

    glm::vec3 cameraPos(0, -1, 0);
    glm::quat cameraRotation(1, 0, 0, 0);

    float nearPlane = 0.01f;
    float farPlane = 200.0f;
    glm::mat4 proj = glm::perspective(glm::radians(90.0f), w / (float)h, nearPlane, farPlane);
    proj[1][1] *= -1;

    glm::mat4 lightProj = glm::ortho(-100.f, 140.f, -100.f, 100.f, -400.f, 400.f);
    glm::mat4 lightView = glm::lookAt(glm::vec3(100), glm::vec3(0), glm::vec3(0, 0, 1));

    Debug::color = glm::vec4(randf(0.2, 1), randf(0.2, 1), randf(0.2, 1), 1);
    Debug::DrawCube(glm::vec3(randf(-5, 5), randf(-5, 5), randf(-5, 5)), glm::vec3(randf(0.02, 0.08)), 10000);
    Debug::Reserve(&batchManager, "Cube", false, 1e5);
    for (int i = 0; i < 1e5; i++) {
        Debug::color = glm::vec4(randf(0.2, 1), randf(0.2, 1), randf(0.2, 1), 1);
        Debug::DrawCube(glm::vec3(randf(-5, 5), randf(-5, 5), randf(-5, 5)), glm::vec3(randf(0.02, 0.08)), 1000);
    }
    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();
        if (glfwGetKey(window, GLFW_KEY_ESCAPE)) glfwSetWindowShouldClose(window, true);

        static float t = 0;
        t += 0.01;
        Debug::color = glm::vec4(0, 0, 1, 0.5);
        Debug::DrawSphere(glm::vec3(3, 4, sin(t + 1)), 1);
        Debug::color = glm::vec4(0, 1, 0, 0.5);
        Debug::DrawSphere(glm::vec3(3, 2, sin(t + 2)), 1);
        Debug::color = glm::vec4(1, 0, 0, 0.5);
        Debug::DrawSphere(glm::vec3(3, 0, sin(t + 3)), 1);

        // Debug::Reserve(&batchManager, "Cube", false, Debug::ObjectCount(&batchManager, "Cube", false));
        cameraRotation = GetRotation(window, cameraRotation, 0.001f);
        cameraPos += cameraRotation * GetMoveDirection(window, 0.4f);

        glm::mat4 view = glm::lookAt(
            cameraPos, cameraPos + cameraRotation * glm::vec3(0, 1, 0), cameraRotation * glm::vec3(0, 0, 1)
        );

        Debug::Frame();
        renderer.RenderFrame(
            generalQueue, proj * view, cameraPos, nearPlane, farPlane,
            {.lightViewProjection = lightProj * lightView,
             .lightDirection = glm::vec3(-1),
             .lightColor = glm::vec3(1, 1, 1)},
            !glfwGetKey(window, GLFW_KEY_SPACE)
        );
    }
    Debug::Destroy();
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
    std::vector<const char *> extensions(glfwExtensions, glfwExtensions + glfwExtensionCount);
    extensions.push_back("VK_KHR_get_physical_device_properties2");
    return vg::Instance(extensions, [](vg::MessageSeverity severity, const char *message) {
        static bool first = true;
        std::ofstream logFile("logs.txt", first ? std::ofstream::trunc : std::ofstream::app);
        first = false;
        switch (severity) {
        case vg::MessageSeverity::Info: logFile << "Info: "; break;
        case vg::MessageSeverity::Warning: logFile << "Warning: "; break;
        case vg::MessageSeverity::Error: logFile << "Error: "; break;
        default: break;
        }
        logFile << message << "\n\n";

        if (severity < vg::MessageSeverity::Warning) return;
        std::cout << '\n' << message << '\n' << '\n';
    });
}
vg::Device CreateDevice(vg::SurfaceHandle windowSurface, vg::Queue &generalQueue) {
    vg::DeviceFeatures deviceFeatures(
        {vg::Feature::LogicOp, vg::Feature::SampleRateShading, vg::Feature::FillModeNonSolid,
         vg::Feature::MultiDrawIndirect}
    );
    return vg::Device(
        {&generalQueue}, {"VK_KHR_swapchain", "VK_EXT_sampler_filter_minmax"}, deviceFeatures, windowSurface,
        [](auto id, auto supportedQueues, auto supportedExtensions, auto type, vg::DeviceLimits limits,
           vg::DeviceFeatures features) { return (type == vg::DeviceType::Dedicated); }
    );
}
glm::vec3 GetMoveDirection(GLFWwindow *window, float speed) {
    glm::vec3 direction(0);
    if (glfwGetKey(window, GLFW_KEY_R) == GLFW_PRESS) direction += glm::vec3(0, 0, 1);
    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) direction += glm::vec3(0, 0, -1);
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) direction += glm::vec3(-1, 0, 0);
    if (glfwGetKey(window, GLFW_KEY_F) == GLFW_PRESS) direction += glm::vec3(1, 0, 0);
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) direction += glm::vec3(0, -1, 0);
    if (glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS) direction += glm::vec3(0, 1, 0);
    if (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS) speed *= 0.1;
    if (glfwGetKey(window, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS) speed *= 3;

    if (length(direction) > 0.0f) direction = normalize(direction);
    return direction * speed * 0.3f;
}

glm::quat GetRotation(GLFWwindow *window, glm::quat cameraRotation, float speed) {
    static glm::dvec2 lastMouseP(-100, -100);
    if (lastMouseP == glm::dvec2(-100, -100)) glfwGetCursorPos(window, &lastMouseP.x, &lastMouseP.y);

    glm::dvec2 mouseP;
    glfwGetCursorPos(window, &mouseP.x, &mouseP.y);
    glm::dvec2 mouseDelta = mouseP - lastMouseP;
    lastMouseP = mouseP;
    mouseDelta = glm::dvec2(0, 0);
    float roll = 0;
    if (glfwGetKey(window, GLFW_KEY_I) == GLFW_PRESS) mouseDelta -= glm::dvec2(0, 1);
    if (glfwGetKey(window, GLFW_KEY_K) == GLFW_PRESS) mouseDelta += glm::dvec2(0, 1);
    if (glfwGetKey(window, GLFW_KEY_J) == GLFW_PRESS) mouseDelta -= glm::dvec2(1, 0);
    if (glfwGetKey(window, GLFW_KEY_L) == GLFW_PRESS) mouseDelta += glm::dvec2(1, 0);
    if (glfwGetKey(window, GLFW_KEY_U) == GLFW_PRESS) roll -= 1;
    if (glfwGetKey(window, GLFW_KEY_O) == GLFW_PRESS) roll += 1;
    mouseDelta *= speed * 20;
    roll *= speed * 20;

    // if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_1))
    {
        cameraRotation = glm::angleAxis((float)-mouseDelta.x, cameraRotation * glm::vec3(0, 0, 1)) * cameraRotation;
        cameraRotation = glm::angleAxis((float)-mouseDelta.y, cameraRotation * glm::vec3(1, 0, 0)) * cameraRotation;
        cameraRotation = glm::angleAxis((float)-roll, cameraRotation * glm::vec3(0, 1, 0)) * cameraRotation;
        cameraRotation = glm::normalize(cameraRotation);
    }

    return cameraRotation;
}
