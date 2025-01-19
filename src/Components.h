#pragma once
#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include "VG/VG.h"
#include "ECS.h"

struct Transform : public ECS::Component<Transform>
{
    glm::vec3 position;
    glm::vec3 scale;
    glm::quat rotation;

    Transform(glm::vec3 position = glm::vec3(0, 0, 0), glm::vec3 scale = glm::vec3(1, 1, 1), glm::quat rotation = glm::quat(1, 0, 0, 0));

    glm::mat4 Matrix() const;
    glm::vec3 Forward() const;
    glm::vec3 Up() const;
    glm::vec3 Right() const;
};

struct MeshArray : public ECS::Component<MeshArray>
{
    uint8_t materialID;
    uint8_t materialVariantID;
    uint16_t meshID;

    MeshArray() {}
    MeshArray(uint8_t materialID, uint8_t materialVariantID, uint16_t meshID)
        :materialID(materialID), materialVariantID(materialVariantID), meshID(meshID)
    {}
};

ENTITY(Transform, MeshArray);