#include "Batch.h"
#include "Mesh.h"
#include "Material.h"
#include <algorithm>

Batch::Batch(class Material *material, class Mesh *mesh, uint32_t batchDataElementSize)
    : material(material), mesh(mesh) {
    auto id = batchMetaData.Allocate(sizeof(BatchMetaData), sizeof(BatchMetaData));
    BatchMetaData batch;
    batch.firstInstace = RenderObject::objectDataBuffer.Offset(id) / RenderObject::objectDataBuffer.Alignment(id);
    batch.batchDataElementSize = batchDataElementSize;
    batch.materialIndex =
        Material::materialBuffer.Offset(material->index) / Material::materialBuffer.Alignment(material->index) +
        material->variant;
    batch.meshIndex = mesh->index;

    batchMetaData.Write(id, batch);
    if (drawCallBuffer.GetSize() == 0) {
        drawCallBuffer.Allocate(sizeof(vg::cmd::DrawIndexed), sizeof(vg::cmd::DrawIndexed));
        drawCallBuffer.Allocate(sizeof(vg::cmd::DrawIndexed), sizeof(vg::cmd::DrawIndexed));
    }
}

Batch::Batch() : material(nullptr), mesh(nullptr) {}

Batch::Batch(Batch &&o) : Batch() {
    std::swap(material, o.material);
    std::swap(mesh, o.mesh);
    std::swap(renderObjects, o.renderObjects);

    if (material) {
        auto it = std::find(material->batches.begin(), material->batches.end(), &o);
        if (it != material->batches.end()) *it = this;
    }
    if (mesh) {
        auto it = std::find(mesh->batches.begin(), mesh->batches.end(), &o);
        if (it != mesh->batches.end()) *it = this;
    }
}

Batch &Batch::operator=(Batch &&o) {
    if (this == &o) return *this;

    std::swap(material, o.material);
    std::swap(mesh, o.mesh);
    std::swap(renderObjects, o.renderObjects);

    if (material) {
        auto it = std::find(material->batches.begin(), material->batches.end(), &o);
        if (it != material->batches.end()) *it = this;
    }
    if (o.material) {
        auto it = std::find(o.material->batches.begin(), o.material->batches.end(), this);
        if (it != o.material->batches.end()) *it = &o;
    }
    if (mesh) {
        auto it = std::find(mesh->batches.begin(), mesh->batches.end(), &o);
        if (it != mesh->batches.end()) *it = this;
    }
    if (o.mesh) {
        auto it = std::find(o.mesh->batches.begin(), o.mesh->batches.end(), this);
        if (it != o.mesh->batches.end()) *it = &o;
    }

    return *this;
}

Batch::~Batch() {
    if (material) {
        auto it = std::find(material->batches.begin(), material->batches.end(), this);
        if (it != material->batches.end()) material->batches.erase(it);
    }
    if (mesh) {
        auto it = std::find(mesh->batches.begin(), mesh->batches.end(), this);
        if (it != mesh->batches.end()) mesh->batches.erase(it);
    }
}

bool Batch::operator==(const std::tuple<Material *, Mesh *> &o) const {
    return material->index == std::get<Material *>(o)->index && material->variant == std::get<Material *>(o)->variant &&
           mesh->index == std::get<Mesh *>(o)->index;
}

bool Batch::operator<(const std::tuple<Material *, Mesh *> &o) const {
    if (material->index < std::get<Material *>(o)->index) return true;
    if (material->index > std::get<Material *>(o)->index) return false;

    if (material->variant < std::get<Material *>(o)->variant) return true;
    if (material->variant > std::get<Material *>(o)->variant) return false;
    return mesh->index < std::get<Mesh *>(o)->index;
}

RenderBuffer Batch::drawCallBuffer({vg::BufferUsage::IndirectBuffer, vg::BufferUsage::StorageBuffer});
RenderBuffer Batch::batchMetaData(vg::BufferUsage::StorageBuffer);
