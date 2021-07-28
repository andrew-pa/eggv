#include "geometry_set.h"
#include "imgui.h"
 
geometry_set::geometry_set(device* dev, const std::string& path) : dev(dev), data(path), path(path) {
}

std::shared_ptr<mesh> geometry_set::load_mesh(size_t index) {
    auto cv = this->mesh_cashe.find(index);
    if (cv != this->mesh_cashe.end()) {
        return cv->second;
    } else {
        const auto& h = this->header(index);
        auto msh = std::make_shared<mesh>(dev, (size_t)h.num_vertices, sizeof(vertex), (size_t)h.num_indices, [&](void* stg_buf) {
            memcpy(stg_buf, data.data() + h.vertex_ptr, sizeof(vertex)*h.num_vertices);
            memcpy((char*)stg_buf + sizeof(vertex)*h.num_vertices, data.data() + h.index_ptr,
                sizeof(uint16)*h.num_indices);
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

void mesh_trait::build_gui(struct scene_object*, frame_state* fs) {
    ImGui::Text("%zu vertices, %zu indices", m->vertex_count, m->index_count);
    bool reload_mesh = false;
    if (ImGui::BeginCombo("Geometry set", this->geo_src->path.c_str())) {
        for (const auto& gs : fs->current_scene->geometry_sets) {
            if (ImGui::Selectable(gs->path.c_str(), gs == this->geo_src)) {
                this->geo_src = gs;
                reload_mesh = true;
            }
        }
        ImGui::EndCombo();
    }
    if (ImGui::BeginCombo("Mesh name", this->geo_src->mesh_name(this->mesh_index))) {
        for (size_t i = 0; i < this->geo_src->num_meshes(); ++i) {
            if (ImGui::Selectable(this->geo_src->mesh_name(i), i == this->mesh_index)) {
                this->mesh_index = i;
                reload_mesh = true;
            }
        }
        ImGui::EndCombo();
    }
    if(reload_mesh) {
        m = geo_src->load_mesh(this->mesh_index);
    }
}
