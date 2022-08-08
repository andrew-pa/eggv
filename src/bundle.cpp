#include "bundle.h"
#include "app.h"
#include "geometry_set.h"
#include "imgui.h"
#include "imgui_stdlib.h"
#include <glm/gtx/polar_coordinates.hpp>
#include <utility>

static std::mt19937                 default_random_gen(std::random_device{}());
static uuids::uuid_random_generator uuid_gen = uuids::uuid_random_generator(default_random_gen);

bundle::bundle(device* dev, const std::filesystem::path& path)
    : selected_material(nullptr), materials_changed(true) {
    for(const auto& geo_src_path : std::filesystem::directory_iterator{path / "geometry"})
        geometry_sets.push_back(std::make_shared<geometry_set>(dev, geo_src_path));

    std::ifstream input(path / "materials.json");
    if(input) {
        json raw_materials;
        input >> raw_materials;

        for(const auto& [id, m] : raw_materials.items())
            materials.push_back(std::make_shared<material>(uuids::uuid::from_string(id).value(), m)
            );
    }

    for(const auto& rg_path : std::filesystem::directory_iterator{path / "render-graphs"}) {
        json          rg;
        std::ifstream input{rg_path};
        input >> rg;
        render_graphs.emplace(rg_path.path().filename().c_str(), rg);
    }
}

#include "ImGuiFileDialog.h"

void bundle::build_gui(frame_state& fs) {
    if(fs.gui_open_windows["Geometry Sets"]) {
        ImGui::Begin("Geometry Sets", &fs.gui_open_windows.at("Geometry Sets"));
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
                       (std::string("##MeshInfo") + gs->path.c_str()).c_str(),
                       5,
                       ImGuiTableFlags_Resizable
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

    if(fs.gui_open_windows["Materials"]) {
        ImGui::Begin("Materials", &fs.gui_open_windows.at("Materials"));
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

material::material(
    std::string name, vec3 base_color, std::optional<std::string> diffuse_texpath, uuids::uuid id
)
    : id(id.is_nil() ? uuid_gen() : id), name(std::move(name)), base_color(base_color),
      diffuse_texpath(std::move(diffuse_texpath)) {}

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

bool material::build_gui(frame_state& fs) {
    ImGui::InputText("Name", &this->name);

    bool changed = false;
    changed = ImGui::ColorEdit3("Base", &this->base_color[0], ImGuiColorEditFlags_Float) || changed;
    ImGui::Text("Id: %s", uuids::to_string(this->id).c_str());

    return changed;
}
