#include "scene_graph.h"
#include "app.h"
#include "geometry_set.h"
#include "imgui.h"
#include <glm/gtx/polar_coordinates.hpp>

static std::mt19937                 default_random_gen(std::random_device{}());
static uuids::uuid_random_generator uuid_gen = uuids::uuid_random_generator(default_random_gen);

material::material(
    std::string name, vec3 base_color, std::optional<std::string> diffuse_texpath, uuids::uuid id
)
    : id(id.is_nil() ? uuid_gen() : id), name(name), base_color(base_color),
      diffuse_texpath(diffuse_texpath) {}

#include <filesystem>

scene::scene(device* dev, std::filesystem::path path, json data)
    : selected_material(nullptr), materials_changed(true) {
    for(const auto& p : data["geometries"]) {
        auto pp = std::filesystem::path(p.get<std::string>());
        auto gp = path.parent_path() / pp;
        geometry_sets.push_back(std::make_shared<geometry_set>(dev, p, gp));
    }
    for(const auto& [id, m] : data["materials"].items())
        materials.push_back(std::make_shared<material>(uuids::uuid::from_string(id).value(), m));
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
