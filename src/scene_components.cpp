#include "scene_components.h"
#include <imgui.h>
//TODO: break up headers and move modules into directories ie this should go with the other ECS stuff

void transform_system::update_world_transforms(world::entity_handle e, const mat4& T) {
    auto d = this->entity_data.find(e);
    if(d != this->entity_data.end()) {
        auto& comp = d->second;
        comp.world =
            glm::scale(
                glm::translate(T, comp.translation)
                    * glm::mat4_cast(comp.rotation),
                comp.scale);
    }
    const auto& p = d->second.world;
    e.for_each_child([&](const auto& c) {
        this->update_world_transforms(c, p);
    });
}

void transform_system::update(const frame_state& fs, world* w) {
    this->update_world_transforms(w->root(), mat4(1));
}

void transform_system::build_gui_for_entity(const frame_state& fs, entity_id selected_entity) {
    auto d = this->entity_data.find(selected_entity);
    if(d != this->entity_data.end()) {
        auto& comp = d->second;
        ImGui::DragFloat3("Translation", (float*)&comp.translation, 0.05f);
        ImGui::DragFloat4("Rotation", (float*)&comp.rotation, 0.05f);
        comp.rotation = glm::normalize(comp.rotation);
        ImGui::DragFloat3("Scale", (float*)&comp.scale, 0.05f, 0.f, FLT_MAX);
    }
}

void light_system::build_gui_for_entity(const frame_state& fs, entity_id selected_entity) {
    auto d = this->entity_data.find(selected_entity);
    if(d != this->entity_data.end()) {
        auto& comp = d->second;
        ImGui::Combo("Type", (int*)&comp.type, "Directional\0Point\0");
        if(comp.type == light_type::directional) {
            ImGui::DragFloat3("Direction", (float*)&comp.param, 0.01f);
            comp.param = normalize(comp.param);
        } else if(comp.type == light_type::point) {
            ImGui::DragFloat("Falloff", &comp.param.x, 0.000f, 0.001f, 1000.f, "%.6f");
        }
        ImGui::ColorEdit3("Color", (float*)&comp.color, ImGuiColorEditFlags_HDR | ImGuiColorEditFlags_Float);
        ImGui::Text("Light Render Index: %lu", comp._render_index);
    }
}

void camera_system::build_gui_for_entity(const frame_state& fs, entity_id selected_entity) {
    auto d = this->entity_data.find(selected_entity);
    if(d != this->entity_data.end()) {
        auto& comp = d->second;
        ImGui::DragFloat("Field of View", &comp.fov, 0.1f, pi<float>()/8.f, pi<float>());
        if (!this->active_camera.has_value() || selected_entity != this->active_camera.value()) {
            if (ImGui::Button("Make Active Camera")) {
                this->active_camera = selected_entity;
            }
        }
    }
}
