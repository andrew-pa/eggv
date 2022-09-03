#include "geometry_set.h"
#include "imgui.h"
#include <utility>

geometry_set::geometry_set(device* dev, std::filesystem::path path)
    : data(path.c_str()), dev(dev), path(std::move(path)) {}

std::shared_ptr<mesh> geometry_set::load_mesh(size_t index) {
    auto cv = this->mesh_cache.find(index);
    if(cv != this->mesh_cache.end()) return cv->second;
    const auto& h   = this->header(index);
    auto        msh = std::make_shared<mesh>(
        dev,
        (size_t)h.num_vertices,
        sizeof(vertex),
        (size_t)h.num_indices,
        [&](void* stg_buf) {
            memcpy(stg_buf, data.data() + h.vertex_ptr, sizeof(vertex) * h.num_vertices);
            memcpy(
                (char*)stg_buf + sizeof(vertex) * h.num_vertices,
                data.data() + h.index_ptr,
                sizeof(uint16) * h.num_indices
            );
        }
    );
    this->mesh_cache[index] = msh;
    return msh;
}

std::optional<reactphysics3d::PolygonVertexArray*> geometry_set::load_convex_hull(size_t index) {
    auto cv = this->convex_hull_cache.find(index);
    if(cv != this->convex_hull_cache.end()) return &cv->second.pva;
    const auto& h = this->header(index);
    if(h.hull_ptr == 0) return {};
    uint16_t* d                = (uint16_t*)(data.data() + h.hull_ptr);
    uint16_t  num_hull_indices = *d;
    d++;
    this->convex_hull_cache[index] = physics_pva();
    this->convex_hull_cache[index].faces
        = std::vector<reactphysics3d::PolygonVertexArray::PolygonFace>(
            num_hull_indices / 3, reactphysics3d::PolygonVertexArray::PolygonFace()
        );
    for(size_t i = 0, j = 0; i < num_hull_indices; i += 3, j++) {
        this->convex_hull_cache[index].faces[j].nbVertices = 3;
        this->convex_hull_cache[index].faces[j].indexBase  = i;
    }
    this->convex_hull_cache[index].pva = reactphysics3d::PolygonVertexArray(
        h.num_vertices,
        data.data() + h.vertex_ptr,
        sizeof(vertex),
        d,
        2,
        num_hull_indices / 3,
        this->convex_hull_cache[index].faces.data(),
        reactphysics3d::PolygonVertexArray::VertexDataType::VERTEX_FLOAT_TYPE,
        reactphysics3d::PolygonVertexArray::IndexDataType::INDEX_SHORT_TYPE
    );
    return &this->convex_hull_cache[index].pva;
}

#include "reactphysics3d/collision/TriangleVertexArray.h"

reactphysics3d::TriangleMesh* geometry_set::load_physics_mesh(
    reactphysics3d::PhysicsCommon* phy, size_t index
) {
    auto cm = this->phys_mesh_cache.find(index);
    if(cm != this->phys_mesh_cache.end()) return cm->second.first;
    const auto& h   = this->header(index);
    auto        tva = std::make_unique<reactphysics3d::TriangleVertexArray>(
        h.num_vertices,
        data.data() + h.vertex_ptr,
        sizeof(vertex) + offsetof(vertex, position),
        data.data() + h.vertex_ptr + offsetof(vertex, normal),
        sizeof(vertex),
        h.num_indices / 3,
        data.data() + h.index_ptr,
        3 * sizeof(uint16),
        reactphysics3d::TriangleVertexArray::VertexDataType::VERTEX_FLOAT_TYPE,
        reactphysics3d::TriangleVertexArray::NormalDataType::NORMAL_FLOAT_TYPE,
        reactphysics3d::TriangleVertexArray::IndexDataType ::INDEX_SHORT_TYPE
    );
    auto msh = phy->createTriangleMesh();
    msh->addSubpart(tva.get());
    this->phys_mesh_cache[index] = {msh, std::move(tva)};
    return msh;
}

int32 geometry_set::num_meshes() const { return *(int32*)data.data(); }

const char* geometry_set::mesh_name(size_t index) const {
    return data.data() + this->header(index).name_ptr;
}

const geom_file::mesh_header& geometry_set::header(size_t index) const {
    return *((geom_file::mesh_header*)(data.data() + sizeof(int32)) + index);
}
