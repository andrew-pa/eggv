#include "ecs.h"
#include "imgui.h"

world::world()
    : next_id(root_id + 1), root_entity(std::make_shared<world::node>(root_id, "Root"))
{
    nodes.emplace(root_id, root_entity);
}

void world::update(const frame_state& fs) {
    for(const auto& [_, sys] : systems) {
        sys->update(fs, this);
    }
}

void world::build_scene_tree_gui(const world::entity_handle& e) {
    ImGui::PushID(e.id());
    auto node_open = ImGui::TreeNodeEx(
            (void*)e.id(),
            ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick
                | (this->selected_entity == e ? ImGuiTreeNodeFlags_Selected : 0)
                | (e.has_children() ? 0 : ImGuiTreeNodeFlags_Leaf),
                "%s", e.name().data());

    if(ImGui::IsItemClicked()) this->selected_entity = e;

    if(node_open) {
        e.for_each_child([&](auto c) {
            this->build_scene_tree_gui(c);
        });
        ImGui::TreePop();
    }

    ImGui::PopID();
}

void world::build_gui(const frame_state& fs) {
    if(fs.gui_open_windows->at("World")) {
        ImGui::Begin("World", &fs.gui_open_windows->at("World"));
        this->build_scene_tree_gui(this->root());
        ImGui::End();
    }

    if(fs.gui_open_windows->at("Selected Entity")) {
        ImGui::Begin("Selected Entity", &fs.gui_open_windows->at("Selected Entity"));
        auto sel = entity(selected_entity);
        ImGui::Text("%s", sel.name().data());
        for(const auto& [_, sys] : systems) {
            sys->build_gui(fs, selected_entity);
        }
        ImGui::End();
    }
}
