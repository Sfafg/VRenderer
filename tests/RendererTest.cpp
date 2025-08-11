#include "glm/ext/quaternion_trigonometric.hpp"
#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <iostream>
#define GLM_ENABLE_EXPERIMENTAL
#include "Renderer.h"
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/quaternion.hpp>
#include <math.h>

#include "RenderBuffer.h"

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
    vg::instance =
        vg::Instance({"VK_KHR_surface", "VK_KHR_win32_surface"}, [](vg::MessageSeverity severity, const char *message) {
            if (severity < vg::MessageSeverity::Warning) return;
            std::cout << message << '\n';
            // std::string m(message);
            // m = m.substr(m.find('[') + 2, m.size());
            // std::string brackets = m.substr(0, m.find(']') - 1);
            // m = m.substr(brackets.size(), m.size());
            // m = m.substr(m.find(',') + 1, m.size());
            // std::string type = m.substr(0, m.find(';'));
            // m = m.substr(type.size() + 3, m.size());
            // m = m.substr(m.find('|') + 1, m.size());
            // m = m.substr(0, m.rfind('('));
            // std::string me = m.substr(0, m.rfind('.'));
            // m = m.substr(m.rfind(':') + 1, m.size());

            // std::cout << type << me << '.' << m << '\n';
        });

    vg::SurfaceHandle windowSurface = vg::Window::CreateWindowSurface(vg::instance, window);
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

void Check(bool condition, const char* msg) {
    if (!condition) {
        std::cerr << "srakapierdaka:  " << msg << std::endl;
        std::terminate();
    }
}

void TestRenderBuffer() {
    constexpr int initialCapacity = 1024;
    // sprawdzamy czy po usunieciu wszystkiego nie wywali
    RenderBuffer rbzero(initialCapacity, vg::BufferUsage::VertexBuffer);
    int allocSize = 256;
    int alignment = 16;

    std::vector<uint32_t> regions;
    for (int i = 0; i < 5; ++i) {
        regions.push_back(rbzero.Allocate(allocSize, alignment));
    }

    for (auto& region : regions) {
        rbzero.Deallocate(std::move(region));
    }
    Check(rbzero.GetSize() == 0, "sraka po dealokacji");

    // drugi renderer
    RenderBuffer rb(initialCapacity, vg::BufferUsage::VertexBuffer);
    Check(rb.GetCapacity() >= initialCapacity, "zle przypisuje pojemnosc");
    Check(rb.GetSize() == 0, "poczatkowy rozmiar ma byc 0 nigga");

    int allocSize = 256;
    int alignment = 16;
    
    auto region = rb.Allocate(allocSize, alignment);

    // Access internal vectors directly since accessor methods don't exist
    Check(rb.Size(region) == allocSize, "zle przypisuje rozmiar regionu");
    Check(rb.Alignment(region) == alignment, "zle przypisuje alignment regionu");
    Check(rb.Offset(region) % alignment == 0, "offset regionu nie jest aligned");
    Check(rb.GetSize() >= allocSize, "zle przypisuje rozmiar render bufora po alokacji regionu");

    std::vector<uint8_t> data(allocSize);
    for (int i = 0; i < allocSize; i++) {
        data[i] = static_cast<uint8_t>(i % 256); // cokolwiek zeby bylo idk
    }

    rb.Write(region, data.data(), allocSize);
    // tu blad  
    /*
    auto mem = rb.backBuffer.MapMemory() + rb.Offset(region);
    Check(mem != nullptr);
    Check(std::memcmp(mem, data.data(), allocSize) == 0, "to nie wiem co testuje ;)");
    */
    // wpisujemy dalej
    uint32_t partialOffset = 256;
    uint32_t partialSize = 20;
    std::vector<uint8_t> partialData = {
        0x01, 0x02, 0x03, 0x04, 0x05,
        0x06, 0x07, 0x08, 0x09, 0x0A,
        0x0B, 0x0C, 0x0D, 0x0E, 0x0F,
        0x10, 0x11, 0x12, 0x13, 0x14
    };
    
    rb.Write(region, partialData.data(), partialSize, partialOffset);
    Check(std::memcmp(mem + partialOffset, partialData.data(), partialSize) == 0, "Wpisywanie danych nei dziala ");

    // tu blad 
    /*
    uint32_t newSize = allocSize * 2;
    rb.Reallocate(region, newSize);
    auto newMem = rb.backBuffer.MapMemory() + rb.Offset(region);
    Check(std::memcmp(newMem, data.data(), allocSize) == 0, "Realokacja nie zachowala danych");
    */
    // Deallocate region and verify size decreases
    int sizeBeforeDealloc = rb.GetSize();
    rb.Deallocate(std::move(region));
    Check(rb.GetSize() <= sizeBeforeDealloc, "Rozmiar render bufora po dealokacji nie zmniejszyl sie");

    // Reserve bigger capacity and verify capacity increases
    int biggerCapacity = initialCapacity * 4;
    rb.Reserve(biggerCapacity);
    Check(rb.GetCapacity() >= biggerCapacity, "Rezerwacja wiekszej pojemnosci nie dziala");

    // Allocate multiple regions, check padding calculation correctness
    auto r1 = rb.Allocate(30, 8);
    auto r2 = rb.Allocate(50, 16);
    uint32_t paddingR2 = rb.GetPadding(r2, rb.Offset(r1) + rb.Size(r1));
    Check(paddingR2 == (rb.Alignment(r2) - (rb.Offset(r1) + rb.Size(r1)) % rb.Alignment(r2)) % rb.Alignment(r2), 
          "Padding calculation for second region is incorrect");
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

    TestRenderBuffer();

}
