#include "geometry_set.h"
 
geometry_set::geometry_set(device* dev, const std::string& path) : dev(dev), data(path), path(path) {
}

std::shared_ptr<mesh> geometry_set::load_mesh(size_t index) {
    auto cv = this->mesh_cashe.find(index);
    if (cv != this->mesh_cashe.end()) {
        return cv->second;
    } else {
        auto h = this->header(index);
        auto msh = std::make_shared<mesh>(dev, (size_t)h.num_vertices, sizeof(vertex), (size_t)h.num_indices, [&](void* stg_buf) {
            memcpy(stg_buf, data.data() + h.vertex_ptr, sizeof(vertex)*h.num_vertices);
            memcpy(stg_buf, data.data() + h.index_ptr, sizeof(uint16)*h.num_indices);
        });
        this->mesh_cashe[index] = msh;
        return msh;
    }
}

int32 geometry_set::num_meshes() const {
	return *(int32*)data.data();
}

const char* geometry_set::mesh_name(size_t index) const {
	return data.data() + this->header(index).name_ptr;
}

const geom_file::mesh_header& geometry_set::header(size_t index) const {
	return *((geom_file::mesh_header*)(data.data() + sizeof(int32)) + index);
}

mesh_trait::mesh_trait(trait_factory* f, mesh_create_info* ci)
    : trait(f), geo_src(ci==nullptr ? nullptr : ci->geo_src), mesh_index(ci==nullptr?-1:ci->mesh_index), m(nullptr)
{
    m = geo_src->load_mesh(mesh_index);
}
