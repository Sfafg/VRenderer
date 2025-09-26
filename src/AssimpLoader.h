#pragma once
#include "assimp/Importer.hpp"
#include "assimp/scene.h"
#include "assimp/postprocess.h"
#include "Mesh.h"

namespace Load {
enum class Attribute { Position, Normal, Color };

inline std::vector<Mesh> Meshes(const std::string &path) {
    Assimp::Importer importer;
    const aiScene *scene = importer.ReadFile(path, aiProcess_Triangulate | aiProcess_ImproveCacheLocality);
    if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode) {
        std::cout << "ERROR::ASSIMP::" << importer.GetErrorString() << std::endl;
        return {};
    }

    std::vector<Mesh> meshes;
    for (int i = 0; i < scene->mNumMeshes; i++) {
        aiMesh &mesh = *scene->mMeshes[i];

        std::vector<std::tuple<aiVector3D, aiVector3D>> vertices;
        vertices.reserve(mesh.mNumVertices);
        for (int j = 0; j < mesh.mNumVertices; j++)
            vertices.push_back(std::make_tuple(mesh.mNormals[j], mesh.mVertices[j]));

        std::vector<int> indices;
        indices.reserve(mesh.mNumFaces * 3);
        for (int j = 0; j < mesh.mNumFaces; j++) {
            indices.push_back(mesh.mFaces[j].mIndices[0]);
            indices.push_back(mesh.mFaces[j].mIndices[1]);
            indices.push_back(mesh.mFaces[j].mIndices[2]);
        }
        meshes.emplace_back(Mesh(vertices.size(), vertices.data(), indices.size(), indices.data()));
    }
    return meshes;
}

inline void Model(
    const std::string &path, Material *parentMaterial, std::vector<RenderObject> *renderObjects,
    std::vector<Material> *materials, std::vector<Mesh> *meshes
) {
    Assimp::Importer importer;
    const aiScene *scene = importer.ReadFile(path, aiProcess_Triangulate | aiProcess_ImproveCacheLocality);
    if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode) {
        std::cout << "ERROR::ASSIMP::" << importer.GetErrorString() << std::endl;
        return;
    }

    for (int i = 0; i < scene->mNumMaterials; i++) {
        aiColor4D color;
        float roughness = 1.0f;

        if (AI_SUCCESS == scene->mMaterials[i]->Get(AI_MATKEY_ROUGHNESS_FACTOR, roughness) &&
            AI_SUCCESS == aiGetMaterialColor(scene->mMaterials[i], AI_MATKEY_COLOR_DIFFUSE, &color))
            materials->emplace_back(Material(parentMaterial, std::make_tuple(glm::vec3(0), roughness, color)));
    }

    for (int i = 0; i < scene->mNumMeshes; i++) {
        aiMesh &mesh = *scene->mMeshes[i];

        std::vector<std::tuple<aiVector3D, aiVector3D>> vertices;
        vertices.reserve(mesh.mNumVertices);
        for (int j = 0; j < mesh.mNumVertices; j++)
            vertices.push_back(std::make_tuple(mesh.mNormals[j], mesh.mVertices[j]));

        std::vector<int> indices;
        indices.reserve(mesh.mNumFaces * 3);
        for (int j = 0; j < mesh.mNumFaces; j++) {
            indices.push_back(mesh.mFaces[j].mIndices[0]);
            indices.push_back(mesh.mFaces[j].mIndices[1]);
            indices.push_back(mesh.mFaces[j].mIndices[2]);
        }
        meshes->emplace_back(Mesh(vertices.size(), vertices.data(), indices.size(), indices.data()));
    }

    auto processNode = [&renderObjects, &meshes, &materials,
                        &scene](aiNode *node, const aiMatrix4x4 &parentTransform, auto self) {
        if (!node) return;

        for (int i = 0; i < node->mNumMeshes; i++) {
            int materialIndex = scene->mMeshes[node->mMeshes[i]]->mMaterialIndex;
            renderObjects->emplace_back(RenderObject(
                &meshes[0][node->mMeshes[i]], &materials[0][materialIndex],
                (parentTransform * node->mTransformation).Transpose()
            ));
        }

        for (int i = 0; i < node->mNumChildren; i++)
            self(node->mChildren[i], parentTransform * node->mTransformation, self);
    };

    processNode(scene->mRootNode, scene->mRootNode->mTransformation, processNode);
}
}; // namespace Load
