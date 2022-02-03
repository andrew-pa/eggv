#pragma once
#include "cmmn.h"
#include "mesh.h"
#include <mio/shared_mmap.hpp>
#include "reactphysics3d/engine/PhysicsCommon.h"
#include "reactphysics3d/collision/TriangleMesh.h"
#include "reactphysics3d/collision/TriangleVertexArray.h"

struct physics_pva {
    reactphysics3d::PolygonVertexArray pva;
    std::vector<reactphysics3d::PolygonVertexArray::PolygonFace> faces;

    physics_pva()
        : pva(0, nullptr, 0, nullptr, 0, 0, nullptr,
            reactphysics3d::PolygonVertexArray::VertexDataType::VERTEX_FLOAT_TYPE, 
            reactphysics3d::PolygonVertexArray::IndexDataType::INDEX_SHORT_TYPE), faces()
    {}

    physics_pva(reactphysics3d::PolygonVertexArray pva, 
            std::vector<reactphysics3d::PolygonVertexArray::PolygonFace> faces)
        : pva(pva), faces(faces)
    { }
};
 
class geometry_set {
    std::map<size_t, std::shared_ptr<mesh>> mesh_cache;
    std::map<size_t, physics_pva> convex_hull_cache;
    std::map<size_t, std::pair<
        reactphysics3d::TriangleMesh*,
        std::unique_ptr<reactphysics3d::TriangleVertexArray>>> phys_mesh_cache;
    mio::shared_mmap_source data;
public:
    device* dev;
    std::string path;
    geometry_set(device* dev, const std::string& path, std::filesystem::path data_path);
    std::shared_ptr<mesh> load_mesh(size_t index);
    std::optional<reactphysics3d::PolygonVertexArray*> load_convex_hull(size_t index);
    reactphysics3d::TriangleMesh* load_physics_mesh(
        reactphysics3d::PhysicsCommon* phy, size_t index);
    int32 num_meshes() const;
    const char* mesh_name(size_t index) const;
    const geom_file::mesh_header& header(size_t index) const;
    const char* file_data() { return data.data(); }
};

