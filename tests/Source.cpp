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
#include "ObjLoader.h"
#include "Settings.h"
using namespace ECS;
using namespace std::chrono_literals;
std::tuple<std::vector<glm::vec3>, std::vector<glm::vec3>, std::vector<glm::vec2>, std::vector<uint32_t>> GenerateSphereMesh()
{
    std::vector<glm::vec3> vertices;
    std::vector<glm::vec3> normals;
    std::vector<glm::vec2> uvs;
    std::vector<uint32_t> indices;

    const int resolution = 14;
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
            normals.push_back(p);
            uvs.push_back({ 0,0 });
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
            indices.push_back((i + 1) * resolution * 2 + j + 1);
            indices.push_back((i + 1) * resolution * 2 + j);
        }
    }

    return { vertices, normals,uvs,indices };
}
std::tuple<std::vector<glm::vec3>, std::vector<glm::vec3>, std::vector<glm::vec2>, std::vector<uint32_t>> GenerateBoxMesh()
{
    std::vector<glm::vec3> vertices = {
        glm::vec3(-0.5,-0.5,-0.5),
        glm::vec3(-0.5,-0.5, 0.5),
        glm::vec3(-0.5, 0.5,-0.5),
        glm::vec3(-0.5, 0.5, 0.5),
        glm::vec3(0.5,-0.5,-0.5),
        glm::vec3(0.5,-0.5, 0.5),
        glm::vec3(0.5, 0.5,-0.5),
        glm::vec3(0.5, 0.5, 0.5),
    };
    std::vector<glm::vec3> normals = {
        glm::normalize(glm::vec3(-0.5,-0.5,-0.5)),
        glm::normalize(glm::vec3(-0.5,-0.5, 0.5)),
        glm::normalize(glm::vec3(-0.5, 0.5,-0.5)),
        glm::normalize(glm::vec3(-0.5, 0.5, 0.5)),
        glm::normalize(glm::vec3(0.5,-0.5,-0.5)),
        glm::normalize(glm::vec3(0.5,-0.5, 0.5)),
        glm::normalize(glm::vec3(0.5, 0.5,-0.5)),
        glm::normalize(glm::vec3(0.5, 0.5, 0.5)),
    };
    std::vector<glm::vec2> uvs = {
        glm::vec2(0,0),
        glm::vec2(0,0),
        glm::vec2(0,0),
        glm::vec2(0,0),
        glm::vec2(0,0),
        glm::vec2(0,0),
        glm::vec2(0,0),
        glm::vec2(0,0),
    };
    std::vector<uint32_t> indices = {
        2,1,0, 1,2,3,
        5,4,0, 0,1,5,
        2,6,7, 7,3,2,

    };

    return { vertices, normals,uvs,indices };
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
    Profiler::SetHightPriority();
    glfwInit();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_DECORATED, GLFW_FALSE);
    GLFWwindow* window = glfwCreateWindow(1920, 1080, "VRendererTest", nullptr, nullptr);
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
            vg::Rasterizer(false, false, vg::PolygonMode::Fill, vg::CullMode::Back, vg::FrontFace::Clockwise),
            vg::Multisampling(1, false),
            vg::DepthStencil(true, true, vg::CompareOp::Less),
            vg::ColorBlending(true, vg::LogicOp::Copy, { 0,0,0,0 }, { vg::ColorBlend() }),
            { vg::DynamicState::Viewport, vg::DynamicState::Scissor }
        ));
    Renderer::materials[0].AddVariant(VariantData{ 1.0f, 0.45f, 0.0f });
    Renderer::materials[0].AddVariant(VariantData{ 0.0f, 1.0f, 0.2f });
    Renderer::materials[0].AddVariant(VariantData{ 0.0f, 0.0f, 1.0f });

    Renderer::Init(window, windowSurface, w, h);
    {
        auto [vertices, normals, uvs, triangles] = GenerateSphereMesh();
        Renderer::renderSystem[0].AddMesh(triangles, vertices, normals, uvs);
    }
    std::vector<glm::vec3> vertices, normals;
    std::vector<glm::vec2> uvs;
    std::vector<uint32_t> triangles;

    LoadMesh("resources/Monkey.obj", &vertices, &normals, &triangles);
    uvs.resize(vertices.size());
    Renderer::renderSystem[0].AddMesh(triangles, vertices, normals, uvs);
    vertices.clear();
    normals.clear();
    uvs.clear();
    LoadMesh("resources/Box.obj", &vertices, &normals, &triangles);
    uvs.resize(vertices.size());
    Renderer::renderSystem[0].AddMesh(triangles, vertices, normals, uvs);

    vertices.clear();
    normals.clear();
    uvs.clear();
    LoadMesh("resources/Bottle.obj", &vertices, &normals, &triangles);
    uvs.resize(vertices.size());
    Renderer::renderSystem[0].AddMesh(triangles, vertices, normals, uvs);


    std::vector<Entity> entities(1024 * 2);
    Renderer::renderSystem[0].ReserveRenderObjects(entities.size());
    entities[0] = Entity::AddEntity(Transform({ 0,0,0 }, { 3,3,3 }), MeshArray(0, 0, 1));

    for (int j = 1; j < entities.size(); j++)
    {
        auto&& i = entities[j];
        auto rFloat = [](float min, float max) {return rand() / float(RAND_MAX) * (max - min) + min; };
        glm::vec3 pos(rFloat(-50, 50), rFloat(-50, 50), rFloat(-50, 50));
        glm::vec3 scale(rFloat(1.5, 3), rFloat(1.5, 3), rFloat(1.5, 3));
        glm::vec3 axis(rFloat(-1, 1), rFloat(-1, 1), rFloat(-1, 1));
        float angle = rFloat(0, 360);
        axis = glm::normalize(axis);
        int variant = rand() % 3;
        int mesh = rand() % 3;
        i = Entity::AddEntity(Transform(pos, scale, glm::rotate(glm::quat(1, 0, 0, 0), angle, axis)), MeshArray(0, variant, mesh));
    }

    Transform cameraTransform({ 0,-8,0 });
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

                float speed = 0.4f;
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