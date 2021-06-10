#include "scene_graph.h"
#include "imgui.h"
#include <glm/gtx/polar_coordinates.hpp>

void camera::update(frame_state*) {
    if(!ImGui::IsWindowFocused(ImGuiFocusedFlags_AnyWindow)) {
        if(ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
            auto ws = ImGui::GetMainViewport()->Size;
            auto md = ImGui::GetMouseDragDelta(ImGuiMouseButton_Left);
            ImGui::ResetMouseDragDelta(ImGuiButtonFlags_MouseButtonLeft);
            auto m = (vec2(md.y / ws.y, -md.x/ws.x))*2.f*pi<float>();
            position.x = m.x;
            position.y = m.y;
        }
        //position.z = max(0.f, position.z - ImGui::GetIO().MouseWheel * 0.2f);
    }
}

mat4 camera::view() {
    vec3 pos_eu = glm::euclidean((vec2)position.xy()) * position.z;
    return glm::lookAt(pos_eu+target, target, vec3(0.f, 1.f, 0.f));
}

mat4 camera::proj(float aspect_ratio) {
    return glm::perspective(fov, aspect_ratio, 0.1f, 100.f);
}

void scene::update(frame_state* fs) {
    this->cam.update(fs);
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