#include "scene_graph.h"
#include "imgui.h"
#include <glm/gtx/polar_coordinates.hpp>
#include "app.h"

camera::camera(vec3 pos, vec3 targ, float fov)
        : fov(fov), position(pos), up(vec3(0.f, 1.f, 0.f)), look(normalize(targ-pos)), speed(5.0), mouse_enabled(false)
{
    right = cross(look, up);
}

void camera::update(frame_state* fs, app* app) {
    if(ImGui::IsWindowFocused(ImGuiFocusedFlags_AnyWindow)) { return; }
    /*     if(ImGui::IsMouseDragging(ImGuiMouseButton_Left)) { */
    /*         auto ws = ImGui::GetMainViewport()->Size; */
    /*         auto md = ImGui::GetMouseDragDelta(ImGuiMouseButton_Left); */
    /*         ImGui::ResetMouseDragDelta(ImGuiButtonFlags_MouseButtonLeft); */
    /*         auto m = (vec2(md.y / ws.y, -md.x/ws.x))*2.f*pi<float>(); */
    /*         position.x = m.x; */
    /*         position.y = m.y; */
    /*     } */
    /*     //position.z = max(0.f, position.z - ImGui::GetIO().MouseWheel * 0.2f); */
    /* } */
    
    if(glfwGetKey(app->wnd, GLFW_KEY_W) != GLFW_RELEASE) {
        position += speed*look*fs->dt;
    } else if(glfwGetKey(app->wnd, GLFW_KEY_S) != GLFW_RELEASE) {
        position -= speed*look*fs->dt;
    }
    if(glfwGetKey(app->wnd, GLFW_KEY_A) != GLFW_RELEASE) {
        position -= speed*right*fs->dt;
    } else if(glfwGetKey(app->wnd, GLFW_KEY_D) != GLFW_RELEASE) {
        position += speed*right*fs->dt;
    }
    if(glfwGetKey(app->wnd, GLFW_KEY_Q) != GLFW_RELEASE) {
        position -= speed*up*fs->dt;
    } else if(glfwGetKey(app->wnd, GLFW_KEY_E) != GLFW_RELEASE) {
        position += speed*up*fs->dt;
    }

    if(mouse_enabled) {
        double xpos, ypos;
        glfwGetCursorPos(app->wnd, &xpos, &ypos);
        vec2 sz = vec2(app->size());
        vec2 np = ((vec2(xpos, ypos) / sz)*2.f - 1.f) * 2.0f;
        glfwSetCursorPos(app->wnd, sz.x/2.f, sz.y/2.f);
        mat4 T = rotate(rotate(mat4(1), -np.y, right), np.x, vec3(0.f, 1.f, 0.f));
        look = vec4(look,0.f)*T;
        right = vec4(right,0.f)*T;
        up = vec4(up,0.f)*T;
    }
}

#include <glm/gtx/io.hpp>

mat4 camera::view() {
    // right handed coordinate system!
    look = normalize(look);
    right = normalize(cross(look, up));
    up = cross(right, look);
    mat4 result(1);
    result[0][0] = right.x;
    result[1][0] = right.y;
    result[2][0] = right.z;
    result[0][1] = up.x;
    result[1][1] = up.y;
    result[2][1] = up.z;
    result[0][2] = -look.x;
    result[1][2] = -look.y;
    result[2][2] = -look.z;
    result[3][0] = -dot(right, position);
    result[3][1] = -dot(up, position);
    result[3][2] = dot(look, position);
    return result;
}

mat4 camera::proj(float aspect_ratio) {
    return glm::perspective(fov, aspect_ratio, 0.1f, 100.f);
}

void scene::update(frame_state* fs, app* app) {
    this->cam.update(fs, app);
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
