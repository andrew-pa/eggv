#include "scene_graph.h"
#include "imgui.h"
#include <glm/gtx/polar_coordinates.hpp>
#include "app.h"

static std::mt19937 default_random_gen;
static uuids::uuid_random_generator uuid_gen = uuids::uuid_random_generator(default_random_gen);

scene_object::scene_object(std::optional<std::string> name, uuids::uuid id) : name(name), traits{}, children{} {
    if(id.is_nil()) this->id = uuid_gen();
    else this->id = id;
}

std::shared_ptr<scene_object> deserialize_object_graph(const std::vector<std::shared_ptr<trait_factory>>& trait_factories, json data) {
    auto obj = std::make_shared<scene_object>(data.contains("name") ? std::optional((std::string)data.at("name")) : std::nullopt,
            uuids::uuid::from_string((std::string)data.at("id")).value());
    for(const auto& t : data.at("t").items()) {
        auto t_id = std::atoi(t.key().c_str());
        auto tf = find_if(trait_factories.begin(), trait_factories.end(), [t_id](auto tf) { return tf->id() == t_id; });
        if(tf != trait_factories.end()) {
            (*tf)->deserialize(obj.get(), t.value());
        } else {
            throw t_id;
        }
    }
    for(const auto& c : data.at("c")) {
        obj->children.push_back(deserialize_object_graph(trait_factories, c));
    }
    return obj;
}

scene::scene(std::vector<std::shared_ptr<trait_factory>> trait_factories, json data) 
    : trait_factories(trait_factories), selected_object(nullptr), active_camera(nullptr),
      root(deserialize_object_graph(trait_factories, data))
{
}

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

#include "ImGuiFileDialog.h"

void scene::build_gui(frame_state* fs) {
    if(fs->gui_open_windows->at("Scene")) {
        ImGui::Begin("Scene", &fs->gui_open_windows->at("Scene"), ImGuiWindowFlags_MenuBar);
        if(ImGui::BeginMenuBar()) {
            if(ImGui::BeginMenu("File")) {
                if(ImGui::MenuItem("Open scene...")) {
                    ImGuiFileDialog::Instance()->OpenDialog("LoadSceneGraphDlg", "Open scene", ".json", ".");
                }
                if(ImGui::MenuItem("Save scene...")) {
                    ImGuiFileDialog::Instance()->OpenDialog("SaveSceneGraphDlg", "Save scene", ".json", ".");
                }
                ImGui::EndMenu();
            }
            ImGui::EndMenuBar();
        }
        build_scene_graph_tree(root);
        ImGui::End();
    }

    if(fs->gui_open_windows->at("Selected Object")) {
        ImGui::Begin("Selected Object", &fs->gui_open_windows->at("Selected Object"));
        if (selected_object != nullptr) {
            for (auto& [id, t] : selected_object->traits) {
                if (ImGui::CollapsingHeader(t->parent->name().c_str(), ImGuiTreeNodeFlags_DefaultOpen)) {
                    t->build_gui(selected_object.get(), fs);
                }
            }
        }
        ImGui::End();
    }

    if(ImGuiFileDialog::Instance()->Display("SaveSceneGraphDlg")) {
        if(ImGuiFileDialog::Instance()->IsOk()) {
            std::ofstream output(ImGuiFileDialog::Instance()->GetFilePathName());
            output << std::setw(4) << this->serialize();
        }
        ImGuiFileDialog::Instance()->Close();
    }

    if(ImGuiFileDialog::Instance()->Display("LoadSceneGraphDlg")) {
        if(ImGuiFileDialog::Instance()->IsOk()) {
            std::ifstream input(ImGuiFileDialog::Instance()->GetFilePathName());
            json data;
            input >> data;
            *this = scene(trait_factories, data);
        }
        ImGuiFileDialog::Instance()->Close();
    }

}

json scene::serialize_graph(std::shared_ptr<scene_object> obj) const {
    auto id_str = uuids::to_string(obj->id);
    json children_json = json::array();
    for(const auto& c : obj->children) {
        children_json.push_back(this->serialize_graph(c));
    }
    json obj_json = {
        {"id", id_str},
        {"c", children_json},
        {"t", json::object()}
    };
    if(obj->name.has_value()) obj_json["name"] = obj->name.value();
    for(const auto&[tid, t] : obj->traits) {
        obj_json["t"][std::to_string(tid)] = t->serialize();
    }
    return obj_json;
}

json scene::serialize() const {
    return serialize_graph(root);
}

json transform_trait::serialize() const {
    return {
        {"t", ::serialize(this->translation)},
        {"s", ::serialize(this->scale)},
        {"r", {rotation.x, rotation.y, rotation.z, rotation.w}}
    };
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
    auto r = data.at("r");
    auto cfo = create_info(::deserialize_v3(data.at("t")),
            quat(r[3], r[0], r[1], r[2]),
            ::deserialize_v3(data.at("s")));
    this->add_to(obj, &cfo);
}

json light_trait::serialize() const {
    return {
        {"t", this->type},
        {"p", ::serialize(this->param)},
        {"c", ::serialize(this->color)}
    };
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
    auto cfo = create_info((light_type)data.at("t"), 
            ::deserialize_v3(data.at("p")),
            ::deserialize_v3(data.at("c")));
    this->add_to(obj, &cfo);
}

json camera_trait::serialize() const {
    return {
        {"fov", this->fov}
    };
}

void camera_trait::build_gui(scene_object* obj, frame_state* fs) {
    ImGui::DragFloat("FOV", &this->fov, 0.1f, pi<float>()/8.f, pi<float>());
    if(ImGui::Button("Make Active Camera")) {
        fs->current_scene->active_camera = obj->shared_from_this();
    }
}

void camera_trait::collect_viewport_shapes(scene_object* ob, frame_state*, const mat4& T,
        bool selected, std::vector<viewport_shape>& shapes)
{
    shapes.push_back(viewport_shape(viewport_shape_type::axis, vec3(1.f), scale(T, vec3(0.4f, 0.4f, 1.0f))));
}

void camera_trait_factory::deserialize(struct scene_object* obj, json data) {
    auto cfo = create_info(data.at("fov"));
    this->add_to(obj, &cfo);
}
