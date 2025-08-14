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

vg::Queue generalQueue;
vg::Device renderDevice;
void InitVulkan() {
    vg::instance = vg::Instance({}, [](vg::MessageSeverity severity, const char *message) {
        if (severity < vg::MessageSeverity::Warning) return;
        std::cout << message << '\n';
    });

    generalQueue = vg::Queue({vg::QueueType::Transfer}, 1.0f);
    renderDevice = vg::Device(
        {&generalQueue}, {}, vg::DeviceFeatures(),
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

    uint32_t sraka = rb.Read(region, readData.data(), random_size); // 124uytve2q3rvbf32weptif
    assert(sraka == random_size && "Read size does not match written size");

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

    // Test 6: Realokacja
    uint32_t newSize = allocSize * 2;
    uint32_t newRegion = rb.Reallocate(region, newSize);

    std::vector<uint8_t> reallocReadData(allocSize);
    rb.Read(newRegion, reallocReadData.data(), allocSize);
    Check(std::memcmp(reallocReadData.data(), data.data(), allocSize) == 0, "Realokacja nie zachowala danych");

    // Sprawdzenie, czy nowy region ma poprawny rozmiar i offset
    int sizeBeforeDealloc = rb.GetSize();
    rb.Deallocate(std::move(region));
    Check(rb.GetSize() <= sizeBeforeDealloc, "Rozmiar render bufora po dealokacji nie zmniejszyl sie");

    // Test 7: Rezerwacja większej pojemności
    int biggerCapacity = initialCapacity * 4;
    rb.Reserve(biggerCapacity);
    Check(rb.GetCapacity() >= biggerCapacity, "Rezerwacja wiekszej pojemnosci nie dziala");

    // Test 8: Sprawdzenie poprawności alokacji i zapisu danych do regionu po rezerwacji
    auto r1 = rb.Allocate(30, 8);
    auto r2 = rb.Allocate(50, 16);
    uint32_t paddingR2 = rb.GetPadding(r2, rb.Offset(r1) + rb.Size(r1));
    Check(
        paddingR2 == (rb.Alignment(r2) - (rb.Offset(r1) + rb.Size(r1)) % rb.Alignment(r2)) % rb.Alignment(r2),
        "Padding calculation for second region is incorrect"
    );
}

int main() {
    InitVulkan();
    RenderBuffer renderBuffer(128, vg::BufferUsage::StorageBuffer);
    TestRenderBuffer();
}
