#pragma once
#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include "Mesh.h"
#include "Material.h"
#include "RenderObject.h"

class Debug {

  public:
    static glm::mat4 matrix;
    static glm::vec4 color;

  public:
    static void Init();
    static void Destroy();

    static void DrawSphere(glm::vec3 center, float radius, int frameDuration = 1);
    static void DrawWireSphere(glm::vec3 center, float radius, int frameDuration = 1);
    static void DrawCube(glm::vec3 center, glm::vec3 extends, int frameDuration = 1);
    static void DrawWireCube(glm::vec3 center, glm::vec3 extends, int frameDuration = 1);
    static void DrawLine(glm::vec3 begin, glm::vec3 end, int frameDuration = 1);
    static void DrawArrow(glm::vec3 begin, glm::vec3 end, int frameDuration = 1);
    static void DrawTriangle(glm::vec3 a, glm::vec3 b, glm::vec3 c, int frameDuration = 1);
    static void DrawCylinder(glm::vec3 center, float radius, float height, int frameDuration = 1);
    static void DrawCone(glm::vec3 center, float baseRadius, float height, int frameDuration = 1);

    static void Frame();

  private:
    static std::vector<RenderObject> objects;
    static std::vector<int> objectLifeTime;
};
