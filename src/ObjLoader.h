#pragma once
#include <vector>
#include <glm/glm.hpp>

bool LoadMesh(const char* meshPath, std::vector<glm::vec3>* vertices, std::vector<glm::vec3>* normals, std::vector<unsigned int>* triangles);