#include "scene_graph.h"
#include "imgui.h"
#include <glm/gtx/polar_coordinates.hpp>
#include "app.h"

void scene::update(frame_state* fs, app* app) {
    // TODO: call update on all traits on all objects
}

void scene::build_scene_graph_tree(std::shared_ptr<scene_object> obj) {
    auto node_open = ImGui::TreeNodeEx(obj->name.value_or("<unnamed>").c_str(),
        ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick
        | (selected_object == obj ? ImGuiTreeNodeFlags_Selected : 0)
        | (obj->children.size() == 0 ? ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_Bullet : 0));
    if(ImGui::IsItemClicked())
        selected_object = obj;
    if (node_open) {
        for (const auto& c : obj->children) {
            build_scene_graph_tree(c);
        }
        ImGui::TreePop();
    }
}

void scene::build_gui(frame_state* fs) {
    ImGui::Begin("Scene");
    build_scene_graph_tree(root);
    ImGui::End();

    ImGui::Begin("Selected Object");
    if (selected_object != nullptr) {
        for (auto& [id, t] : selected_object->traits) {
            if (ImGui::CollapsingHeader(t->parent->name().c_str(), ImGuiTreeNodeFlags_DefaultOpen)) {
                t->build_gui(selected_object.get(), fs);
            }
        }
    }
    ImGui::End();
}

void transform_trait::append_transform(struct scene_object*, mat4& T, frame_state*) {
    T = glm::scale(glm::translate(T, translation)*glm::mat4_cast(rotation), scale);
}

void transform_trait::build_gui(struct scene_object*, frame_state*) {
    ImGui::DragFloat3("Translation", (float*)&this->translation, 0.05f);
    ImGui::DragFloat4("Rotation", (float*)&this->rotation, 0.05f);
    this->rotation = glm::normalize(this->rotation);
    ImGui::DragFloat3("Scale", (float*)&this->scale, 0.05f, 0.f, FLT_MAX);
}

void transform_trait_factory::deserialize(struct scene_object* obj, json data) {
}

void light_trait::build_gui(scene_object* obj, frame_state*) {
    ImGui::Combo("Type", (int*)&this->type, "Directional\0Point\0");
    if(type == light_type::directional) {
        ImGui::DragFloat3("Direction", (float*)&this->param, 0.01f);
        this->param = normalize(this->param);
    } else if(type == light_type::point) {
        ImGui::DragFloat("Falloff", &this->param.x, 0.01f, 0.001f, 1000.f);
    }
    ImGui::ColorEdit3("Color", (float*)&this->color);
}

void light_trait::collect_viewport_shapes(scene_object* ob, frame_state*, const mat4& T, bool selected, std::vector<viewport_shape>& shapes) {
    if(this->type == light_type::point) {
        shapes.push_back(viewport_shape(viewport_shape_type::axis, this->color, scale(T, vec3(0.25f))));
    }
}

void light_trait_factory::deserialize(struct scene_object* obj, json data) {
}

void camera_trait::build_gui(scene_object* obj, frame_state* fs) {
    ImGui::DragFloat("FOV", &this->fov, 0.1f, pi<float>()/8.f, pi<float>());
    if(ImGui::Button("Make Active Camera")) {
        fs->current_scene->active_camera = obj->shared_from_this();
    }
}

void camera_trait::collect_viewport_shapes(scene_object* ob, frame_state*, const mat4& T, bool selected, std::vector<viewport_shape>& shapes) {
    shapes.push_back(viewport_shape(viewport_shape_type::axis, vec3(1.f), scale(T, vec3(0.4f, 0.4f, 1.0f))));
}

void camera_trait_factory::deserialize(struct scene_object* obj, json data) {
}
