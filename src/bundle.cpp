#include "bundle.h"
#include "app.h"
#include "geometry_set.h"
#include "imgui.h"
#include "imgui_stdlib.h"
#include "stb_image.h"
#include <glm/gtx/polar_coordinates.hpp>
#include <utility>

static std::mt19937                 default_random_gen(std::random_device{}());
static uuids::uuid_random_generator uuid_gen = uuids::uuid_random_generator(default_random_gen);

// tragic that we can't use constructors the way things are meant to be, but we can't call `shared_from_this` from a constructor and we need some way for materials to refer back to the bundle they came from so sometimes it just be like that
void bundle::load(device* dev, const std::filesystem::path& path) {
    materials_changed = true;
    geometry_sets.clear();
    materials.clear();
    textures.clear();
    render_graphs.clear();
    selected_material = nullptr;

    root_path = path;

    for(const auto& geo_src_path : std::filesystem::directory_iterator{path / "geometry"})
        geometry_sets.push_back(std::make_shared<geometry_set>(dev, geo_src_path));

    auto tx_path = path / "textures";
    for(const auto& tx_entry : std::filesystem::recursive_directory_iterator{tx_path}) {
        if(tx_entry.is_regular_file()) {
            const auto* name = tx_entry.path().lexically_relative(tx_path).c_str();
            textures[name] = texture_data{tx_entry.path()};
        }
    }

    std::ifstream input(path / "materials.json");
    if(input) {
        json raw_materials;
        input >> raw_materials;

        auto self = shared_from_this();

        for(const auto& [id, material_def] : raw_materials.items())
            materials.push_back(
                std::make_shared<material>(self, uuids::uuid::from_string(id).value(), material_def)
            );
    }

    for(const auto& rg_path : std::filesystem::directory_iterator{path / "render-graphs"}) {
        json          rg;
        std::ifstream input{rg_path.path()};
        input >> rg;
        render_graphs.emplace(rg_path.path().filename().c_str(), rg);
    }
}

void bundle::save() {
    json raw_materials;
    for(const auto& mat : materials) {
        raw_materials[uuids::to_string(mat->id)] = mat->serialize();
    }
    std::ofstream mats_out{root_path / "materials.json"};
    mats_out << raw_materials;

    for(const auto&[name, raw_rg] : render_graphs) {
        std::ofstream f{root_path / "render-graphs" / name};
        f << raw_rg;
    }
}

texture_data::texture_data(const std::filesystem::path& path) {
    int channels;
    this->data = stbi_load(path.c_str(), (int*)&this->width, (int*)&this->height, &channels, 4);
    /*switch(channels) {
        case 1: fmt = vk::Format::eR8Unorm; break;
        case 2: fmt = vk::Format::eR8G8Unorm; break;
        case 3: fmt = vk::Format::eR8G8B8Unorm; break;
        case 4: fmt = vk::Format::eR8G8B8A8Unorm; break;
    };*/
    this->fmt = vk::Format::eR8G8B8A8Unorm;
    this->size_bytes = width*4*height;
    std::cout << "image " << path << " -> " << width << "x" << height << "/" << channels << "\n";
}

texture_data::~texture_data() { if(data != nullptr) free(data); }

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
            auto new_mat = std::make_shared<material>(shared_from_this(), "new material");
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

material::material(const std::shared_ptr<class bundle>& parent_bundle,
    std::string name, vec3 base_color, std::optional<std::string> diffuse_tex, uuids::uuid id
)
    : id(id.is_nil() ? uuid_gen() : id), name(std::move(name)), base_color(base_color),
      diffuse_tex(std::move(diffuse_tex)), parent_bundle(parent_bundle) {}

material::material(const std::shared_ptr<class bundle>& parent_bundle,
        uuids::uuid id, json data) : id(id), parent_bundle(parent_bundle) {
    name       = data["name"];
    base_color = ::deserialize_v3(data["base"]);
    if(data.contains("textures")) {
        auto& tx = data["textures"];
        if(tx.contains("diffuse")) diffuse_tex = tx["diffuse"];
    }
}

json material::serialize() const {
    json mat = {
        {"name",     name                   },
        {"base",     ::serialize(base_color)},
        {"textures", json::object()         }
    };

    if(diffuse_tex.has_value()) mat["textures"]["diffuse"] = diffuse_tex.value();

    return mat;
}

bool material::build_gui(frame_state& fs) {
    ImGui::InputText("Name", &this->name);
    ImGui::Text("Id: %s", uuids::to_string(this->id).c_str());

    bool changed = false;
    changed = ImGui::ColorEdit3("Base", &this->base_color[0], ImGuiColorEditFlags_Float) || changed;

    ImGui::Text("Textures");
    ImGui::Indent();
    if(ImGui::BeginCombo("Diffuse", diffuse_tex.value_or("<none>").c_str())) {
        if(ImGui::Selectable("<none>", !diffuse_tex.has_value())) {
            diffuse_tex = std::nullopt;
            changed = true;
        }
        for(const auto& [name, _] : parent_bundle.lock()->textures) {
            if(ImGui::Selectable(name.c_str(), name == diffuse_tex)) {
                diffuse_tex = name;
                changed = true;
            }
        }
        ImGui::EndCombo();
    }

    return changed;
}
