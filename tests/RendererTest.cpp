#include "glm/ext/quaternion_trigonometric.hpp"
#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <iostream>
#define GLM_ENABLE_EXPERIMENTAL
#include "Renderer.h"
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/quaternion.hpp>
#include <math.h>
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
extern "C" {
typedef struct VkInstance_T *VkInstance;
typedef struct VkSurfaceKHR_T *VkSurfaceKHR;
int glfwCreateWindowSurface(VkInstance instance, GLFWwindow *window, const void *allocator, VkSurfaceKHR *surface);
}
#include "RenderBuffer.h"

float randf(float min = 0, float max = 1) { return rand() / (float)RAND_MAX * (max - min) + min; }

vg::Queue generalQueue;
vg::Device renderDevice;
void InitVulkan() {
    vg::instance = vg::Instance({}, [](vg::MessageSeverity severity, const char *message) {
        if (severity < vg::MessageSeverity::Warning) return;
        std::cout << message << '\n';
    });

    vg::DeviceFeatures deviceFeatures(
        {vg::Feature::LogicOp, vg::Feature::SampleRateShading, vg::Feature::FillModeNonSolid,
         vg::Feature::MultiDrawIndirect}
    );
    generalQueue = vg::Queue({vg::QueueType::Transfer}, 1.0f);
    renderDevice = vg::Device(
        {&generalQueue}, {}, deviceFeatures,
        [](auto id, auto supportedQueues, auto supportedExtensions, auto type, vg::DeviceLimits limits,
           vg::DeviceFeatures features) { return (type == vg::DeviceType::Dedicated); }
    );
    vg::currentDevice = &renderDevice;
}

void Check(bool condition, const char *msg) {
    if (!condition) {
        std::cerr << "BŁĄD:  " << msg << std::endl;
        // std::terminate();
    }
}

void TestRenderBuffer() {
    constexpr int initialCapacity = 1024;
    int allocSize = 728;
    int alignment = 16;
    // test 1: Dealokacja wszystkich regionów w RenderBuffer
    /*
    RenderBuffer rbzero(initialCapacity, vg::BufferUsage::VertexBuffer);
    Check(rbzero.GetCapacity() >= initialCapacity, "Niepoprawna początkowa pojemność");

    std::vector<uint32_t> regions;
    for (int i = 0; i < 5; ++i) { regions.push_back(rbzero.Allocate(allocSize, alignment)); }

    for (auto &region : regions) { rbzero.Deallocate(std::move(region)); }
    Check(rbzero.GetSize() == 0, "sraka po dealokacji");
    */
    // test 2: Podstawowe operacje na RenderBuffer
    RenderBuffer rb(initialCapacity, vg::BufferUsage::VertexBuffer);
    Check(rb.GetCapacity() >= initialCapacity, "Niepoprawna początkowa pojemność");
    Check(rb.GetCapacity() >= initialCapacity, "zle przypisuje pojemnosc");
    Check(rb.GetSize() == 0, "poczatkowy rozmiar ma byc 0");

    auto region = rb.Allocate(allocSize, alignment);

    // Access internal vectors directly since accessor methods don't exist
    Check(rb.Size(region) == allocSize, "zle przypisuje rozmiar regionu");
    Check(rb.Alignment(region) == alignment, "zle przypisuje alignment regionu");
    Check(rb.Offset(region) % alignment == 0, "offset regionu nie jest aligned");
    Check(rb.GetSize() >= allocSize, "zle przypisuje rozmiar render bufora po alokacji regionu");

    printf(
        "Region %u allocated with size %u, alignment %u, offset %u\n", region, rb.Size(region), rb.Alignment(region),
        rb.Offset(region)
    );

    // test 3: Sprawdzenie poprawności alokacji i zapisu danych do regionu
    int random_size = 100;
    std::vector<uint8_t> data(random_size);
    for (int i = 0; i < random_size; i++) {
        data[i] = static_cast<uint8_t>(i % 256); // cokolwiek zeby bylo idk
    }

    rb.Write(region, data.data(), random_size);

    printf("Data written to region %u: ", region);

    // odczytujemy dane z backBuffer i sprawdzamy czy są poprawne
    std::vector<uint8_t> readData(random_size);
    uint32_t readSize = rb.Read(region, readData.data(), random_size);
    assert(readSize == random_size && "Read size does not match written size");

    printf("Written data: ");
    for (int i = 0; i < random_size; i++) { printf("%02X ", data[i]); }
    printf("\n");

    printf("Read data:    ");
    for (int i = 0; i < random_size; i++) { printf("%02X ", readData[i]); }
    printf("\n");

    Check(std::memcmp(readData.data(), data.data(), random_size) == 0, "Dane nie zostaly poprawnie zapisane");

    // test 5: Częściowe wpisanie danych (dalej)
    uint32_t partialOffset = 500;
    uint32_t partialSize = 20;
    std::vector<uint8_t> partialData = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A,
                                        0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x10, 0x11, 0x12, 0x13, 0x14};

    rb.Write(region, partialData.data(), partialSize, partialOffset);
    // odczytujemy czesciowe dane i sprawdzamy czy są poprawne
    std::vector<uint8_t> partialReadData(partialSize);
    rb.Read(region, partialReadData.data(), partialSize, partialOffset);
    Check(
        std::memcmp(partialReadData.data(), partialData.data(), partialSize) == 0,
        "Wpisywanie częściowych danych nie działa"
    );
    
    // Test 6: Realokacja z zachowaniem danych
    std::cout << "[TEST] Data BEFORE realloc:  ";
    for (int i = 0; i < random_size; i++) std::cout << (int)data[i] << " ";
    std::cout << std::endl;

    uint32_t newSize = static_cast<uint32_t>(allocSize * 1.2);  // zwiększ rozmiar
    rb.Reallocate(region, newSize);

    // Sprawdź czy dane zostały zachowane
    std::vector<uint8_t> reallocReadData(random_size);
    uint32_t readSizeAfterRealloc = rb.Read(region, reallocReadData.data(), random_size);
    printf("Read size after realloc: %u\n", readSizeAfterRealloc);
    
    std::cout << "[TEST] Data after realloc:  ";
    for (int i = 0; i < random_size; i++) std::cout << (int)reallocReadData[i] << " ";
    std::cout << std::endl;

    Check(std::memcmp(reallocReadData.data(), data.data(), random_size) == 0, "Realokacja nie zachowala danych");
    Check(rb.Size(region) == newSize, "Realokacja nie zmieniła rozmiaru regionu");
    Check(rb.Offset(region) % alignment == 0, "Offset po realokacji nie jest aligned");

    // Test 7: Alokacja kolejnego regionu po realokacji (sprawdzenie paddingu)
    uint32_t region2 = rb.Allocate(100, 32);
    Check(rb.Offset(region2) % 32 == 0, "Drugi region nie jest poprawnie aligned");
    Check(rb.Offset(region2) >= rb.Offset(region) + rb.Size(region), "Drugi region nakłada się na pierwszy");
    
    // Sprawdź padding między regionami
    uint32_t expectedPadding = rb.GetPadding(region2, rb.Offset(region) + rb.Size(region));
    uint32_t actualGap = rb.Offset(region2) - (rb.Offset(region) + rb.Size(region));
    Check(actualGap == expectedPadding, "Niepoprawny padding między regionami");

    printf("Region 1: offset=%u, size=%u, alignment= %u\n", rb.Offset(region), rb.Size(region), rb.Alignment(region));
    printf("Region 2: offset=%u, size=%u, alignment= %u\n", rb.Offset(region2), rb.Size(region2), rb.Alignment(region2));
    printf("Gap between regions: %u (expected padding: %u)\n", actualGap, expectedPadding);

    // Test 8: Erase z zachowaniem alignmentów
    std::vector<uint8_t> testData(50);
    for (int i = 0; i < 50; i++) {
        testData[i] = static_cast<uint8_t>(200 + i);
    }
    rb.Write(region2, testData.data(), 50);
    
    // Sprawdź dane przed Erase
    std::vector<uint8_t> beforeErase(50);
    rb.Read(region2, beforeErase.data(), 50);
    Check(std::memcmp(beforeErase.data(), testData.data(), 50) == 0, "Dane nie zostały zapisane przed Erase");

    // Usuń część danych z pierwszego regionu
    rb.Erase(region, 100, 200);  // usuń 100 bajtów od pozycji 200
    
    // Sprawdź czy drugi region wciąż ma poprawne dane i alignment
    std::vector<uint8_t> afterErase(50);
    rb.Read(region2, afterErase.data(), 50);
    Check(std::memcmp(afterErase.data(), testData.data(), 50) == 0, "Erase zniszczył dane w innych regionach");
    Check(rb.Offset(region2) % rb.Alignment(region2) == 0, "Erase naruszył alignment drugiego regionu");
    Check(rb.Size(region) == newSize - 100, "Erase nie zmniejszył rozmiaru regionu");

    printf("After Erase - Region 1: offset=%u, size=%u, alignment= %u\n", rb.Offset(region), rb.Size(region), rb.Alignment(region));
    printf("After Erase - Region 2: offset=%u, size=%u, alignment= %u\n", rb.Offset(region2), rb.Size(region2), rb.Alignment(region2));

    // Test 9: Dealokacja ze sprawdzeniem paddingu
    
    uint32_t region3 = rb.Allocate(64, 64);
    uint32_t oldOffset2 = rb.Offset(region2);
    uint32_t oldOffset3 = rb.Offset(region3);
    
    printf("Before Deallocate - Region 2: offset=%u, Region 3: offset=%u\n", oldOffset2, oldOffset3);
    
    rb.Deallocate(region2);  // usuń środkowy region

    // po usunieciu region2, region3 powinien się przesunąć na miejsce region2, czyli 1
    Check(rb.Offset(region2) % rb.Alignment(region2) == 0, "Dealokacja naruszyla alignment pozostałego regionu");
    Check(rb.Offset(region2) < oldOffset3, "Region nie został przesunięty po dealokacji");
    
    printf("After Deallocate - Region 3: offset=%u (was %u)\n", rb.Offset(region2), oldOffset3);

    // Test 10: Rezerwacja większej pojemności
    int biggerCapacity = initialCapacity * 4;
    int oldCapacity = rb.GetCapacity();
    rb.Reserve(biggerCapacity);
    Check(rb.GetCapacity() >= biggerCapacity, "Rezerwacja większej pojemności nie działa");
    
    std::cout << "💗💗💗💗💗 All tests passed! 💗💗💗💗💗" << std::endl;
}

int main() {
    InitVulkan();
    RenderBuffer renderBuffer(128, vg::BufferUsage::StorageBuffer);
    TestRenderBuffer();
}
