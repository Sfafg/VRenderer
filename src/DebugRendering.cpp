#include "DebugRendering.h"
#include "AssimpLoader.h"
#include <coroutine>
#include <list>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/quaternion.hpp>

Material material;
Material opaqueMaterial;
std::vector<std::string> meshNames;
std::list<Mesh> meshes;
Mesh *GetMesh(const std::string &name);

void Debug::Init() {
    opaqueMaterial = Material(
        false, "resources/shaders/debugShader.vert.spv", "resources/shaders/debugShader.frag.spv",
        vg::VertexLayout(
            {{0, sizeof(float) * 6}, {1, sizeof(uint), vg::InputRate::Instance}},
            {{0, 0, vg::Format::RGB32SFLOAT},
             {1, 0, vg::Format::RGB32SFLOAT, sizeof(float) * 3},
             {2, 1, vg::Format::R32UINT}}
        ),
        {.cullMode = vg::CullMode::Back}
    );

    material = Material(
        true, "resources/shaders/debugShader.vert.spv", "resources/shaders/debugShader.frag.spv",
        vg::VertexLayout(
            {{0, sizeof(float) * 6}, {1, sizeof(uint), vg::InputRate::Instance}},
            {{0, 0, vg::Format::RGB32SFLOAT},
             {1, 0, vg::Format::RGB32SFLOAT, sizeof(float) * 3},
             {2, 1, vg::Format::R32UINT}}
        ),
        {.cullMode = vg::CullMode::Back, .depthWriteEnable = false, .enableLogicOp = false}
    );
}
Material *GetMaterial(const glm::vec4 &color) { return color.a == 1.0f ? &opaqueMaterial : &material; }

void Debug::Destroy() {
    objects.clear();
    objectLifeTime.clear();
    material.~Material();
    meshes.clear();
    meshNames.clear();
}

// void Debug::DrawPlane(glm::vec3 point, glm::vec3 normal, glm::vec2 size, int frameDuration) {
//     glm::mat4 mat = glm::translate(glm::mat4(1), center) * glm::scale(glm::mat4(1), glm::vec3(radius));

//     objects.emplace_back(RenderObject(GetMesh("Plane"), &material, std::make_tuple(color, matrix * mat), true));
//     objectLifeTime.push_back(frameDuration);
// }

void Debug::DrawSphere(glm::vec3 center, float radius, int frameDuration) {
    glm::mat4 mat = glm::translate(glm::mat4(1), center) * glm::scale(glm::mat4(1), glm::vec3(radius));

    objects.emplace_back(
        RenderObject(GetMesh("Sphere"), GetMaterial(Debug::color), std::make_tuple(color, matrix * mat), true)
    );
    objectLifeTime.push_back(frameDuration);

    // TODO: this is bugging.
    // static bool a = false;
    // if (!a) {
    //     BatchArray::SetLOD(BatchArray::Get(GetMesh("Sphere"), &material), {{GetMesh("Sphere"), &opaqueMaterial}});
    //     a = true;
    // }
}

// void Debug::DrawWireSphere(glm::vec3 center, float radius, int frameDuration);
//
void Debug::DrawCube(glm::vec3 center, glm::vec3 extends, int frameDuration) {
    glm::mat4 mat = glm::translate(glm::mat4(1), center) * glm::scale(glm::mat4(1), extends);

    objects.emplace_back(
        RenderObject(GetMesh("Cube"), GetMaterial(Debug::color), std::make_tuple(color, matrix * mat), true)
    );
    objectLifeTime.push_back(frameDuration);
}
// void Debug::DrawWireCube(glm::vec3 center, glm::vec3 extends, int frameDuration);
void Debug::DrawLine(glm::vec3 begin, glm::vec3 end, int frameDuration) {
    const float thickness = 0.03;
    glm::mat4 mat = matrix;

    glm::vec3 to = end - begin;
    float distance = glm::length(to);
    if (distance == 0) return;
    to /= distance;

    matrix = mat * glm::translate(glm::mat4(1), begin + to * distance * 0.5f) *
             glm::toMat4(glm::rotation(glm::vec3(0, 0, 1), to)) *
             glm::scale(glm::mat4(1), glm::vec3(thickness, thickness, distance * 0.5f));

    objects.emplace_back(
        RenderObject(GetMesh("Cylinder"), GetMaterial(Debug::color), std::make_tuple(color, matrix), true)
    );
    objectLifeTime.push_back(frameDuration);

    matrix = mat;
}
void Debug::DrawArrow(glm::vec3 begin, glm::vec3 end, int frameDuration) {
    const float thickness = 0.03;
    const float arrowThickness = 0.07;
    float arrowLength = 0.15;
    glm::mat4 mat = matrix;

    glm::vec3 to = end - begin;
    float distance = glm::length(to);
    if (distance == 0) return;
    to /= distance;

    arrowLength = std::min(arrowLength, distance);
    matrix = mat * glm::translate(glm::mat4(1), begin + to * (distance - arrowLength * 0.5f)) *
             glm::toMat4(glm::rotation(glm::vec3(0, 0, 1), to)) *
             glm::scale(glm::mat4(1), glm::vec3(arrowThickness, arrowThickness, arrowLength * 0.5));

    objects.emplace_back(
        RenderObject(GetMesh("Cone"), GetMaterial(Debug::color), std::make_tuple(color, matrix * mat), true)
    );
    objectLifeTime.push_back(frameDuration);
    distance -= arrowLength;

    if (distance > 0) {
        matrix = mat * glm::translate(glm::mat4(1), begin + to * distance * 0.5f) *
                 glm::toMat4(glm::rotation(glm::vec3(0, 0, 1), to)) *
                 glm::scale(glm::mat4(1), glm::vec3(thickness, thickness, distance * 0.5f));

        objects.emplace_back(
            RenderObject(GetMesh("Cylinder"), GetMaterial(Debug::color), std::make_tuple(color, matrix), true)
        );
        objectLifeTime.push_back(frameDuration);
    }

    matrix = mat;
}
// void Debug::DrawTriangle(glm::vec3 a, glm::vec3 b, glm::vec3 c, int frameDuration);
void Debug::DrawCylinder(glm::vec3 center, float radius, float height, int frameDuration) {
    glm::mat4 mat = glm::translate(glm::mat4(1), center) * glm::scale(glm::mat4(1), glm::vec3(radius, radius, height));

    objects.emplace_back(
        RenderObject(GetMesh("Cylinder"), GetMaterial(Debug::color), std::make_tuple(color, matrix * mat), true)
    );
    objectLifeTime.push_back(frameDuration);
}
void Debug::DrawCone(glm::vec3 center, float baseRadius, float height, int frameDuration) {
    glm::mat4 mat =
        glm::translate(glm::mat4(1), center) * glm::scale(glm::mat4(1), glm::vec3(baseRadius, baseRadius, height));

    objects.emplace_back(
        RenderObject(GetMesh("Cone"), GetMaterial(Debug::color), std::make_tuple(color, matrix * mat), true)
    );
    objectLifeTime.push_back(frameDuration);
}

void Debug::DrawFrustum(const glm::mat4 &view, const glm::mat4 &projection, int frameDuration) {
    glm::mat4 inverseViewProjection = glm::inverse(projection * view);

    glm::vec4 corners[8] = {glm::vec4(-1, -1, -1, 1), glm::vec4(1, -1, -1, 1), glm::vec4(-1, 1, -1, 1),
                            glm::vec4(1, 1, -1, 1),   glm::vec4(-1, -1, 1, 1), glm::vec4(1, -1, 1, 1),
                            glm::vec4(-1, 1, 1, 1),   glm::vec4(1, 1, 1, 1)};
    for (int i = 0; i < 8; i++) {
        corners[i] = inverseViewProjection * corners[i];
        corners[i] /= corners[i].w;
    }

    DrawLine(corners[0], corners[1], frameDuration);
    DrawLine(corners[1], corners[3], frameDuration);
    DrawLine(corners[3], corners[2], frameDuration);
    DrawLine(corners[2], corners[0], frameDuration);
    DrawLine(corners[4], corners[5], frameDuration);
    DrawLine(corners[5], corners[7], frameDuration);
    DrawLine(corners[7], corners[6], frameDuration);
    DrawLine(corners[6], corners[4], frameDuration);
    DrawLine(corners[0], corners[4], frameDuration);
    DrawLine(corners[1], corners[5], frameDuration);
    DrawLine(corners[2], corners[6], frameDuration);
    DrawLine(corners[3], corners[7], frameDuration);
    objects.emplace_back(
        RenderObject(GetMesh("Cube"), GetMaterial(Debug::color), std::make_tuple(color, inverseViewProjection), true)
    );
    objectLifeTime.push_back(frameDuration);
}

void Debug::Frame() {
    for (int i = objects.size() - 1; i >= 0; i--) {
        if (objectLifeTime[i] <= 0) {
            objects.erase(objects.begin() + i);
            objectLifeTime.erase(objectLifeTime.begin() + i);
        } else objectLifeTime[i]--;
    }
}

void Debug::Reserve(BatchArray *batchArray, std::string meshName, bool transparent, int count) {
    batchArray->ReserveObjects(batchArray->Get(GetMesh(meshName), transparent ? &material : &opaqueMaterial), count);
}

int Debug::ObjectCount(BatchArray *batchArray, std::string meshName, bool transparent) {
    return batchArray->GetObjectCount(batchArray->Get(GetMesh(meshName), transparent ? &material : &opaqueMaterial));
}

glm::mat4 Debug::matrix(1);
glm::vec4 Debug::color(1, 1, 1, 1);
std::vector<RenderObject> Debug::objects;
std::vector<int> Debug::objectLifeTime;

Mesh *GetMesh(const std::string &name) {
    int j = 0;
    for (auto i = meshes.begin(); i != meshes.end(); i++) {
        if (meshNames[j] == name) return &*i;
        j++;
    }
    meshes.emplace_back(std::move(Load::Meshes(std::string("resources/") + name + std::string(".fbx"))[0]));
    meshNames.emplace_back(name);
    return &meshes.back();
}
