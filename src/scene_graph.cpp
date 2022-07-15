#include "scene_graph.h"
#include "app.h"
#include "geometry_set.h"
#include "imgui.h"
#include <glm/gtx/polar_coordinates.hpp>

static std::mt19937                 default_random_gen(std::random_device{}());
static uuids::uuid_random_generator uuid_gen = uuids::uuid_random_generator(default_random_gen);

scene_object::scene_object(std::optional<std::string> name, uuids::uuid id)
    : name(name), should_delete(false), traits{}, children{} {
    if(id.is_nil())
        this->id = uuid_gen();
    else
        this->id = id;
}

material::material(
    std::string name, vec3 base_color, std::optional<std::string> diffuse_texpath, uuids::uuid id
)
    : name(name), base_color(base_color), diffuse_texpath(diffuse_texpath),
      id(id.is_nil() ? uuid_gen() : id) {}

std::shared_ptr<scene_object> deserialize_object_graph(
    scene* s, const std::vector<std::shared_ptr<trait_factory>>& trait_factories, json data
) {
    auto obj = std::make_shared<scene_object>(
        data.contains("name") ? std::optional((std::string)data.at("name")) : std::nullopt,
        uuids::uuid::from_string((std::string)data.at("id")).value()
    );
    std::deque<std::pair<json, trait_factory*>> ls;
    for(const auto& t : data.at("t").items()) {
        auto t_id = std::atoi(t.key().c_str());
        auto tf   = find_if(trait_factories.begin(), trait_factories.end(), [t_id](auto tf) {
            return tf->id() == t_id;
        });
        if(tf != trait_factories.end())
            ls.emplace_back(t.value(), tf->get());
        else
            throw t_id;
    }
    while(!ls.empty()) {
        auto ttf = ls.front();
        ls.pop_front();
        if(ttf.second->dependencies_loaded(obj.get(), ttf.first))
            ttf.second->deserialize(s, obj.get(), ttf.first);

        else
            ls.emplace_back(ttf);
    }
    for(const auto& c : data.at("c"))
        obj->children.push_back(deserialize_object_graph(s, trait_factories, c));
    return obj;
}

#include <filesystem>

scene::scene(
    device*                                     dev,
    std::vector<std::shared_ptr<trait_factory>> trait_factories,
    std::filesystem::path                       path,
    json                                        data
)
    : trait_factories(trait_factories), selected_object(nullptr), selected_material(nullptr),
      active_camera(nullptr), root(nullptr), materials_changed(true) {
    for(const auto& p : data["geometries"]) {
        auto pp = std::filesystem::path(p.get<std::string>());
        auto gp = path.parent_path() / pp;
        geometry_sets.push_back(std::make_shared<geometry_set>(dev, p, gp));
    }
    for(const auto& [id, m] : data["materials"].items())
        materials.push_back(std::make_shared<material>(uuids::uuid::from_string(id).value(), m));
    root = deserialize_object_graph(this, trait_factories, data["graph"]);
    if(data.contains("active_camera")) {
        active_camera = find_object_by_id(
            uuids::uuid::from_string(data["active_camera"].get<std::string>()).value()
        );
    }
}

std::shared_ptr<scene_object> _find_obj_by_id(
    std::shared_ptr<scene_object> ob, const uuids::uuid& id
) {
    if(!ob) return nullptr;
    if(ob->id == id) return ob;
    for(const auto& ch : ob->children) {
        auto f = _find_obj_by_id(ch, id);
        if(f) return f;
    }
    return nullptr;
}

std::shared_ptr<scene_object> scene::find_object_by_id(const uuids::uuid& id) {
    return _find_obj_by_id(root, id);
}

void _for_each_object(
    std::shared_ptr<scene_object> ob, std::function<void(std::shared_ptr<scene_object>)> f
) {
    f(ob);
    for(const auto& ch : ob->children)
        _for_each_object(ch, f);
}

void scene::for_each_object(std::function<void(std::shared_ptr<scene_object>)> f) {
    _for_each_object(root, f);
}

void _cleanup_objects(const std::shared_ptr<scene_object>& ob) {
    for(auto i = ob->children.begin(); i != ob->children.end(); ++i) {
        if((*i)->should_delete) {
            auto x         = i;
            bool was_first = i == ob->children.begin();
            if(!was_first) i--;
            ob->children.erase(x);
            if(was_first) i = ob->children.begin();
        } else {
            _cleanup_objects(*i);
        }
    }
}

void scene::update(frame_state* fs, app* app) {
    _cleanup_objects(root);
    for_each_object([&](std::shared_ptr<scene_object> ob) {
        for(const auto& [_, t] : ob->traits)
            t->update(ob.get(), fs);
    });
}

void scene::build_scene_graph_tree(std::shared_ptr<scene_object> obj) {
    ImGui::PushID(obj.get());
    auto node_open = ImGui::TreeNodeEx(
        uuids::to_string(obj->id).c_str(),
        ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick
            | (selected_object == obj ? ImGuiTreeNodeFlags_Selected : 0)
            | (obj->children.size() == 0 ? ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_Bullet : 0),
        "%s",
        obj->name.value_or("<unnamed>").c_str()
    );
    if(ImGui::IsItemClicked()) selected_object = obj;
    if(ImGui::BeginPopupContextItem()) {
        if(ImGui::MenuItem("New Object")) {
            auto ch = std::make_shared<scene_object>();
            obj->children.push_back(ch);
        }
        if(ImGui::MenuItem("Delete")) obj->should_delete = true;
        ImGui::EndPopup();
    }
    if(node_open) {
        for(const auto& c : obj->children)
            build_scene_graph_tree(c);
        ImGui::TreePop();
    }
    ImGui::PopID();
}

#include "ImGuiFileDialog.h"

void InputTextResizable(const char* label, std::string* str) {
    ImGui::InputText(
        label,
        (char*)str->c_str(),
        str->size() + 1,
        ImGuiInputTextFlags_CallbackResize,
        (ImGuiInputTextCallback)([](ImGuiInputTextCallbackData* data) {
            if(data->EventFlag == ImGuiInputTextFlags_CallbackResize) {
                std::string* str = (std::string*)data->UserData;
                str->resize(data->BufTextLen);
                data->Buf = (char*)str->c_str();
            }
            return 0;
        }),
        (void*)str
    );
}

void InputTextResizable(const char* label, std::optional<std::string>* str) {
    ImGui::InputText(
        label,
        (char*)str->value_or("<unnamed>").c_str(),
        str->value_or("<unnamed>").size() + 1,
        ImGuiInputTextFlags_CallbackResize,
        (ImGuiInputTextCallback)([](ImGuiInputTextCallbackData* data) {
            if(data->EventFlag == ImGuiInputTextFlags_CallbackResize) {
                std::optional<std::string>* str = (std::optional<std::string>*)data->UserData;
                if(str->has_value()) {
                    str->value().resize(data->BufTextLen);
                    data->Buf = (char*)str->value().c_str();
                } else {
                    *str = std::string(data->Buf, data->BufTextLen);
                }
            }
            return 0;
        }),
        (void*)str
    );
}

void scene::build_gui(frame_state* fs) {
    if(fs->gui_open_windows->at("Scene")) {
        ImGui::Begin("Scene", &fs->gui_open_windows->at("Scene"), ImGuiWindowFlags_MenuBar);
        if(ImGui::BeginMenuBar()) {
            if(ImGui::BeginMenu("File")) {
                if(ImGui::MenuItem("Open scene...")) {
                    ImGuiFileDialog::Instance()->OpenDialog(
                        "LoadSceneGraphDlg", "Open scene", ".json", "."
                    );
                }
                if(ImGui::MenuItem("Save scene...")) {
                    ImGuiFileDialog::Instance()->OpenDialog(
                        "SaveSceneGraphDlg", "Save scene", ".json", "."
                    );
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
        if(selected_object != nullptr) {
            InputTextResizable("Name", &selected_object->name);
            std::optional<trait_id> removed_trait;
            for(auto& [id, t] : selected_object->traits) {
                if(ImGui::CollapsingHeader(
                       t->parent->name().c_str(), ImGuiTreeNodeFlags_DefaultOpen
                   )) {
                    t->build_gui(selected_object.get(), fs);
                    if(ImGui::Button((std::string("Remove ") + t->parent->name()).c_str()))
                        removed_trait = id;
                }
            }
            if(removed_trait.has_value()) {
                selected_object->traits[removed_trait.value()]->remove_from(selected_object.get());
                selected_object->traits.erase(removed_trait.value());
            }
            if(ImGui::BeginPopupContextWindow()) {
                if(ImGui::BeginMenu("Add trait")) {
                    for(const auto& tf : this->trait_factories)
                        if(ImGui::MenuItem(tf->name().c_str()))
                            tf->add_to(selected_object.get(), nullptr);
                    ImGui::EndMenu();
                }
                ImGui::EndPopup();
            }
        }
        ImGui::End();
    }

    if(fs->gui_open_windows->at("Geometry Sets")) {
        ImGui::Begin("Geometry Sets", &fs->gui_open_windows->at("Geometry Sets"));
        if(ImGui::BeginTable("##GeomSets", 2, ImGuiTableFlags_Resizable)) {
            ImGui::TableSetupColumn("Path");
            ImGui::TableSetupColumn("Meshes");
            ImGui::TableHeadersRow();
            for(const auto& gs : this->geometry_sets) {
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::Text("%s", gs->path.c_str());
                ImGui::TableNextColumn();
                if(ImGui::BeginTable(
                       (std::string("##MeshInfo") + gs->path).c_str(), 5, ImGuiTableFlags_Resizable
                   )) {
                    ImGui::TableSetupColumn("Name");
                    ImGui::TableSetupColumn("# Vertices");
                    ImGui::TableSetupColumn("# Indices");
                    ImGui::TableSetupColumn("Material Ix");
                    ImGui::TableSetupColumn("Volume");
                    ImGui::TableHeadersRow();
                    for(size_t i = 0; i < gs->num_meshes(); ++i) {
                        ImGui::TableNextRow();
                        ImGui::TableNextColumn();
                        auto h = gs->header(i);
                        ImGui::Text("%s", gs->mesh_name(i));
                        ImGui::TableNextColumn();
                        ImGui::Text("%i", h.num_vertices);
                        ImGui::TableNextColumn();
                        ImGui::Text("%i", h.num_indices);
                        ImGui::TableNextColumn();
                        ImGui::Text("%i", h.material_index);
                        ImGui::TableNextColumn();
                        vec3 ext = h.aabb_max - h.aabb_min;
                        ImGui::Text("%f", ext.x * ext.y * ext.z);
                    }
                    ImGui::EndTable();
                }
            }
            ImGui::EndTable();
        }
        ImGui::End();
    }

    if(fs->gui_open_windows->at("Materials")) {
        ImGui::Begin("Materials", &fs->gui_open_windows->at("Materials"));
        if(ImGui::BeginCombo(
               "##SelMat",
               selected_material == nullptr ? "<no material selected>"
                                            : selected_material->name.c_str()
           )) {
            for(const auto& m : materials)
                if(ImGui::Selectable(m->name.c_str(), m == selected_material))
                    selected_material = m;
            ImGui::EndCombo();
        }
        ImGui::SameLine();
        if(ImGui::Button("+")) {
            auto new_mat = std::make_shared<material>("new material");
            materials.push_back(new_mat);
            selected_material = new_mat;
            materials_changed = true;
        }
        ImGui::Separator();
        if(selected_material != nullptr)
            materials_changed = selected_material->build_gui(fs) || materials_changed;
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
            json          data;
            input >> data;
            throw "TODO: this needs to probably be more careful about yeeting away in-use "
                  "resources";
        }
        ImGuiFileDialog::Instance()->Close();
    }
}

json scene::serialize_graph(std::shared_ptr<scene_object> obj) const {
    auto id_str        = uuids::to_string(obj->id);
    json children_json = json::array();
    for(const auto& c : obj->children)
        if(!c->should_delete) children_json.push_back(this->serialize_graph(c));
    json obj_json = {
        {"id", id_str        },
        {"c",  children_json },
        {"t",  json::object()}
    };
    if(obj->name.has_value()) obj_json["name"] = obj->name.value();
    for(const auto& [tid, t] : obj->traits)
        obj_json["t"][std::to_string(tid)] = t->serialize();
    return obj_json;
}

json scene::serialize() const {
    json geosets = json::array();
    for(const auto& gs : geometry_sets)
        geosets.push_back(gs->path);
    json mats = json::object();
    for(const auto& m : materials)
        mats[uuids::to_string(m->id)] = m->serialize();
    json sc = {
        {"geometries", geosets              },
        {"materials",  mats                 },
        {"graph",      serialize_graph(root)}
    };

    if(active_camera) sc["active_camera"] = uuids::to_string(active_camera->id);

    return sc;
}

json transform_trait::serialize() const {
    return {
        {"t", ::serialize(this->translation)                  },
        {"s", ::serialize(this->scale)                        },
        {"r", {rotation.x, rotation.y, rotation.z, rotation.w}}
    };
}

void transform_trait::append_transform(struct scene_object*, mat4& T, frame_state*) {
    T = glm::scale(glm::translate(T, translation) * glm::mat4_cast(rotation), scale);
}

void transform_trait::build_gui(struct scene_object*, frame_state*) {
    ImGui::DragFloat3("Translation", (float*)&this->translation, 0.05f);
    ImGui::DragFloat4("Rotation", (float*)&this->rotation, 0.05f);
    this->rotation = glm::normalize(this->rotation);
    ImGui::DragFloat3("Scale", (float*)&this->scale, 0.05f, 0.f, FLT_MAX);
}

void transform_trait_factory::deserialize(class scene* scene, struct scene_object* obj, json data) {
    auto r   = data.at("r");
    auto cfo = create_info(
        ::deserialize_v3(data.at("t")), quat(r[3], r[0], r[1], r[2]), ::deserialize_v3(data.at("s"))
    );
    this->add_to(obj, &cfo);
}

json light_trait::serialize() const {
    return {
        {"t", this->type              },
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
        ImGui::DragFloat("Falloff", &this->param.x, 0.000f, 0.001f, 1000.f, "%.6f");
    }
    ImGui::ColorEdit3(
        "Color", (float*)&this->color, ImGuiColorEditFlags_HDR | ImGuiColorEditFlags_Float
    );
    ImGui::Text("Light Render Index: %lu", this->_render_index);
}

void light_trait::collect_viewport_shapes(
    scene_object* ob,
    frame_state*,
    const mat4&                  T,
    bool                         selected,
    std::vector<viewport_shape>& shapes
) {
    if(this->type == light_type::point) {
        shapes.push_back(
            viewport_shape(viewport_shape_type::axis, this->color, scale(T, vec3(0.25f)))
        );
    }
}

void light_trait_factory::deserialize(class scene* scene, struct scene_object* obj, json data) {
    auto cfo = create_info(
        (light_type)data.at("t"), ::deserialize_v3(data.at("p")), ::deserialize_v3(data.at("c"))
    );
    this->add_to(obj, &cfo);
}

json camera_trait::serialize() const {
    return {
        {"fov", this->fov}
    };
}

void camera_trait::build_gui(scene_object* obj, frame_state* fs) {
    ImGui::DragFloat("FOV", &this->fov, 0.1f, pi<float>() / 8.f, pi<float>());
    if(obj != fs->current_scene->active_camera.get()) {
        if(ImGui::Button("Make Active Camera"))
            fs->current_scene->active_camera = obj->shared_from_this();
    }
}

void camera_trait::collect_viewport_shapes(
    scene_object* ob,
    frame_state*,
    const mat4&                  T,
    bool                         selected,
    std::vector<viewport_shape>& shapes
) {
    shapes.push_back(
        viewport_shape(viewport_shape_type::axis, vec3(1.f), scale(T, vec3(0.4f, 0.4f, 1.0f)))
    );
}

void camera_trait_factory::deserialize(class scene* scene, struct scene_object* obj, json data) {
    auto cfo = create_info(data.at("fov"));
    this->add_to(obj, &cfo);
}

material::material(uuids::uuid id, json data) : id(id) {
    name       = data["name"];
    base_color = ::deserialize_v3(data["base"]);
    if(data.contains("textures")) {
        auto& tx = data["textures"];
        if(tx.contains("diffuse")) diffuse_texpath = tx["diffuse"];
    }
}

json material::serialize() const {
    json mat = {
        {"name",     name                   },
        {"base",     ::serialize(base_color)},
        {"textures", json::object()         }
    };

    if(diffuse_texpath.has_value()) mat["textures"]["diffuse"] = diffuse_texpath.value();

    return mat;
}

bool material::build_gui(frame_state*) {
    InputTextResizable("Name", &this->name);

    bool changed = false;
    changed = ImGui::ColorEdit3("Base", &this->base_color[0], ImGuiColorEditFlags_Float) || changed;
    ImGui::Text("Id: %s", uuids::to_string(this->id).c_str());

    return changed;
}
