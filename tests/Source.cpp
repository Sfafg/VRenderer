#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <GLFW/glfw3.h>
#include <iostream>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/quaternion.hpp>
#include <math.h>
#include "VG/VG.h"
#include "ECS.h"
#include "Renderer.h"
#include "Profiler/Profiler.h"
#include "Settings.h"
using namespace ECS;

std::tuple<std::vector<glm::vec3>, std::vector<uint32_t>> GenerateSphereMesh()
{
    std::vector<glm::vec3> vertices;
    std::vector<uint32_t> indices;

    const int resolution = 8;
    for (int inclination = 0; inclination <= resolution; inclination++)
    {
        for (int azimuth = 0; azimuth < resolution * 2; azimuth++)
        {
            float a = inclination / (float) resolution * glm::pi<float>();
            float b = azimuth / (float) resolution * glm::pi<float>();

            glm::vec3 p;
            p.x = sin(a) * cos(b);
            p.y = sin(a) * sin(b);
            p.z = cos(a);
            vertices.push_back(p);
        }
    }

    for (int i = 0; i < resolution; i++)
    {
        for (int j = 0; j < resolution * 2; j++)
        {
            indices.push_back(i * resolution * 2 + j);
            indices.push_back(i * resolution * 2 + j + 1);
            indices.push_back((i + 1) * resolution * 2 + j);

            indices.push_back(i * resolution * 2 + j + 1);
            indices.push_back(i * resolution * 2 + j + 1);
            indices.push_back((i + 1) * resolution * 2 + j + 1);
        }
    }

    return { vertices,indices };
}
glm::quat fromTo(glm::vec3 a, glm::vec3 b)
{
    a = glm::normalize(a);
    b = glm::normalize(b);

    glm::vec3 rotationAxis = glm::cross(a, b);
    float dotProduct = glm::dot(a, b);

    if (dotProduct >= 1.0f)
        return a;

    float angle = acos(dotProduct);

    return glm::angleAxis(angle, glm::normalize(rotationAxis));
}
int main()
{
    glfwInit();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_DECORATED, GLFW_FALSE);
    GLFWwindow* window = glfwCreateWindow(1920, 1080, "VRendererTest", nullptr, nullptr);
    // glfwSetWindowPos(window, -1920, 0);
    int w, h; glfwGetFramebufferSize(window, &w, &h);

    vg::instance = vg::Instance({ "VK_KHR_surface", "VK_KHR_win32_surface" },
        [](vg::MessageSeverity severity, const char* message) {
            if (severity >= vg::MessageSeverity::Warning)
                std::cout << message << "\n\n";
        });

    vg::SurfaceHandle windowSurface = vg::Window::CreateWindowSurface(vg::instance, window);
    vg::DeviceFeatures deviceFeatures({ vg::Feature::LogicOp,vg::Feature::SampleRateShading, vg::Feature::FillModeNonSolid , vg::Feature::MultiDrawIndirect });
    vg::Queue generalQueue({ vg::QueueType::General }, 1.0f);
    vg::Device rendererDevice({ &generalQueue }, { "VK_KHR_swapchain","VK_EXT_sampler_filter_minmax" }, deviceFeatures, windowSurface,
        [](auto id, auto supportedQueues, auto supportedExtensions, auto type, vg::DeviceLimits limits, vg::DeviceFeatures features) {
            return (type == vg::DeviceType::Dedicated);
        });
    vg::currentDevice = &rendererDevice;

    vg::Shader vertexShader(vg::ShaderStage::Vertex, "resources/shaders/shader.vert.spv");
    vg::Shader fragmentShader(vg::ShaderStage::Fragment, "resources/shaders/shader.frag.spv");
    Renderer::materials.emplace_back(
        Material(
            std::vector<vg::Shader*>{ &vertexShader, & fragmentShader },
            vg::InputAssembly(vg::Primitive::Triangles, false),
            vg::Rasterizer(false, vg::PolygonMode::Fill, vg::CullMode::None),
            vg::Multisampling(1, false),
            vg::DepthStencil(true, true, vg::CompareOp::Less),
            vg::ColorBlending(true, vg::LogicOp::Copy, { 0,0,0,0 }, { vg::ColorBlend() }),
            { vg::DynamicState::Viewport, vg::DynamicState::Scissor }
        ));

    Renderer::Init(window, windowSurface, 1920, 1080);
    Renderer::renderSystem.AddMesh(
        { 0,1,2,1,2,3 },
        { { glm::vec3{-0.5f,-0.5f,0},glm::vec3{-0.5f, 0.5f,0},glm::vec3{ 0.5f,-0.5f,0},glm::vec3{ 0.5f, 0.5f,0} } }
    );

    auto [vertices, triangles] = GenerateSphereMesh();
    Renderer::renderSystem.AddMesh(triangles, vertices);
    Entity entity1 = Entity::AddEntity(Transform({ 100,0,0 }, { 0.25f,0.25f,0.25f }), MeshArray(0, 1));
    Entity entity2 = Entity::AddEntity(Transform({ 100,0,0 }, { 0.25f,0.25f,0.25f }), MeshArray(0, 1));
    Entity entity3 = Entity::AddEntity(Transform({ 100,0,0 }, { 0.25f,0.25f,0.25f }), MeshArray(0, 1));
    Entity entity4 = Entity::AddEntity(Transform({ 100,0,0 }, { 0.25f,0.25f,0.25f }), MeshArray(0, 1));
    Entity entity5 = Entity::AddEntity(Transform({ 100,0,0 }, { 0.25f,0.25f,0.25f }), MeshArray(0, 1));
    Entity entity6 = Entity::AddEntity(Transform({ 100,0,0 }, { 0.25f,0.25f,0.25f }), MeshArray(0, 1));
    Entity entity7 = Entity::AddEntity(Transform({ 100,0,0 }, { 0.25f,0.25f,0.25f }), MeshArray(0, 1));
    Entity entity8 = Entity::AddEntity(Transform({ 100,0,0 }, { 0.25f,0.25f,0.25f }), MeshArray(0, 1));
    Entity entity = Entity::AddEntity(Transform({ 0,0,0 }, { 10,10,10 }, glm::rotate(glm::quat(1, 0, 0, 0), glm::radians(90.0f), glm::vec3{ 1.0f,0.0f,0.0f })), MeshArray(0, 0));
    std::vector<Entity> entities(1024);
    Renderer::renderSystem.ReserveRenderObjects(entities.size() + 4);
    for (auto&& i : entities)
    {
        auto rFloat = [](float min, float max) {return rand() / float(RAND_MAX) * (max - min) + min; };
        glm::vec3 pos(rFloat(-50, 50), rFloat(-50, 50), rFloat(-50, 50));
        glm::vec3 scale(rFloat(0.5, 1), rFloat(0.5, 1), rFloat(0.5, 1));
        glm::vec3 axis(rFloat(-1, 1), rFloat(-1, 1), rFloat(-1, 1));
        float angle = rFloat(0, 360);
        axis = glm::normalize(axis);
        i = Entity::AddEntity(Transform(pos, scale, glm::rotate(glm::quat(1, 0, 0, 0), angle, axis)), MeshArray(0, 0));
    }

    Transform cameraTransform({ 0,-2,0 });
    static float fov = glm::radians(70.0f);
    glfwSetScrollCallback(window, [](GLFWwindow* window, double scrollx, double scrolly)
        {
            fov = fov - glm::radians((float) pow(scrolly, 3));
            if (fov < glm::radians(0.0f)) fov = glm::radians(0.0f);
            if (fov > glm::radians(180.0f)) fov = glm::radians(180.0f);
        });
    glm::vec<2, double> lastMousePos;
    glfwGetCursorPos(window, &lastMousePos.x, &lastMousePos.y);

    bool enterReleased = true;
    while (!glfwWindowShouldClose(window))
    {
        Profiler::BeginFrame();
        {
            PROFILE_NAMED_FUNCTION("Frame Total");
            glfwPollEvents();
            if (glfwGetKey(window, GLFW_KEY_ESCAPE)) glfwSetWindowShouldClose(window, true);

            if (glfwGetKey(window, GLFW_KEY_ENTER))
            {
                if (enterReleased)
                {
                    enterReleased = false;
                    Settings::stopCameraOcclusionUpdate = !Settings::stopCameraOcclusionUpdate;
                }
            }
            else
                enterReleased = true;

            {
                glm::vec<2, double> mousePos;
                glfwGetCursorPos(window, &mousePos.x, &mousePos.y);
                auto mouseDelta = (lastMousePos - mousePos) * 0.002 * (double) fov;
                lastMousePos = mousePos;

                if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_1))
                {
                    auto rotation = glm::rotate(cameraTransform.rotation, (float) mouseDelta.x, { 0,0,1 });
                    cameraTransform.rotation = glm::rotate(rotation, (float) mouseDelta.y, { 1,0,0 });
                }

                float speed = 0.1f;
                if (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT))
                    speed *= 0.2;
                if (glfwGetKey(window, GLFW_KEY_LEFT_CONTROL))
                    speed *= 3;

                if (glfwGetKey(window, GLFW_KEY_W))
                    cameraTransform.position += cameraTransform.Forward() * speed;

                if (glfwGetKey(window, GLFW_KEY_S))
                    cameraTransform.position -= cameraTransform.Forward() * speed;

                if (glfwGetKey(window, GLFW_KEY_D))
                    cameraTransform.position += cameraTransform.Right() * speed;

                if (glfwGetKey(window, GLFW_KEY_A))
                    cameraTransform.position -= cameraTransform.Right() * speed;

                if (glfwGetKey(window, GLFW_KEY_E))
                    cameraTransform.position += cameraTransform.Up() * speed;

                if (glfwGetKey(window, GLFW_KEY_Q))
                    cameraTransform.position -= cameraTransform.Up() * speed;
            }

            Renderer::DrawFrame(cameraTransform, fov);
            Renderer::Present(generalQueue);
        }
        Profiler::EndFrame();
    }
    Renderer::Destroy();
    glfwTerminate();
}