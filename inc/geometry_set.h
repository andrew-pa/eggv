#pragma once
#include "cmmn.h"
#include "mesh.h"
#include <mio/shared_mmap.hpp>

class geometry_set {
    std::map<size_t, std::shared_ptr<mesh>> mesh_cashe;
    mio::shared_mmap_source data;
public:
    device* dev;
    std::string path;
    geometry_set(device* dev, const std::string& path);
    std::shared_ptr<mesh> load_mesh(size_t index);
    int32 num_meshes() const;
    const char* mesh_name(size_t index) const;
    const geom_file::mesh_header& header(size_t index) const;
};

