#include "Components.h"

Transform::Transform(glm::vec3 position, glm::vec3 scale, glm::quat rotation)
    :position(position), scale(scale), rotation(rotation)
{}

glm::mat4 Transform::Matrix() const
{
    return glm::translate(glm::mat4(1), position) * glm::mat4(rotation) * glm::scale(glm::mat4(1), scale);
}

glm::vec3 Transform::Forward() const
{
    auto v = rotation * glm::vec4(0, 1, 0, 0);
    return v;
}

glm::vec3 Transform::Up() const
{
    auto v = rotation * glm::vec4(0, 0, 1, 0);
    return v;
}

glm::vec3 Transform::Right() const
{
    auto v = rotation * glm::vec4(1, 0, 0, 0);
    return v;
}