#include "ecs.h"
#include "imgui.h"
#include "imgui_stdlib.h"

world::world()
    : next_id(root_id + 1), root_entity(std::make_shared<world::node>(nullptr, root_id, "Root")) {
    nodes.emplace(root_id, root_entity);
}

void world::update(const frame_state& fs) {
    // remove any dead entities
    while(!dead_entities.empty()) {
        auto ent  = dead_entities.extract(dead_entities.begin()).value();
        auto node = nodes.extract(ent).mapped();
        for(const auto& [_, sys] : systems)
            sys->remove_entity(ent);
        if(node->parent.lock() != nullptr) {
            auto& ch   = node->parent.lock()->children;
            auto  self = std::find_if(ch.begin(), ch.end(), [&](const auto& c) {
                return c->entity == node->entity;
            });
            ch.erase(self);
        }
        for(const auto& c : node->children)
            dead_entities.insert(c->entity);
    }

    // update all systems
    for(const auto& [_, sys] : systems)
        sys->update(fs, this);
}

void world::build_scene_tree_gui(frame_state& fs, world::entity_handle& e) {
    ImGui::PushID(e.id());
    auto node_open = ImGui::TreeNodeEx(
        (void*)e.id(),
        ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick
            | (fs.selected_entity == e ? ImGuiTreeNodeFlags_Selected : 0)
            | (e.has_children() ? 0 : ImGuiTreeNodeFlags_Leaf),
        "%s",
        e.name().length() > 0 ? e.name().data() : "<unnamed>"
    );

    if(ImGui::IsItemClicked()) fs.selected_entity = e;

    if(ImGui::BeginPopupContextItem("#entity-menu")) {
        if(ImGui::MenuItem("New child")) fs.selected_entity = e.add_child();

        if(ImGui::MenuItem("Remove")) {
            if(fs.selected_entity == e) fs.selected_entity = e.parent();
            e.remove();
        }
        ImGui::EndPopup();
    }

    if(node_open) {
        e.for_each_child([&](auto c) { this->build_scene_tree_gui(fs, c); });
        ImGui::TreePop();
    }

    ImGui::PopID();
}

void world::build_gui(frame_state& fs) {
    if(fs.gui_open_windows["World"]) {
        ImGui::Begin("World", &fs.gui_open_windows.at("World"));
        auto root = this->root();
        this->build_scene_tree_gui(fs, root);
        ImGui::End();
    }

    if(fs.gui_open_windows["Selected Entity"]) {
        ImGui::Begin("Selected Entity", &fs.gui_open_windows.at("Selected Entity"));
        if(fs.selected_entity != 0) {
            auto sel = entity(fs.selected_entity);
            ImGui::InputTextWithHint("##name", "<entity name>", &sel._node->name);
            ImGui::SameLine();
            ImGui::Text("#%lu", fs.selected_entity);
            for(const auto& [sys_id, sys] : systems)
                if(sys->has_data_for_entity(fs.selected_entity)
                   && ImGui::CollapsingHeader(sys->name().data(), ImGuiTreeNodeFlags_DefaultOpen))
                    sys->build_gui_for_entity(fs, fs.selected_entity);
            if(ImGui::BeginPopupContextWindow("##newcomp")) {
                for(const auto& [id, sys] : systems)
                    if(ImGui::MenuItem(sys->name().data()))
                        sys->add_entity_with_defaults(fs.selected_entity);
                ImGui::EndPopup();
            }
        }
        ImGui::End();
    }

    for(const auto& sys : this->systems)
        sys.second->build_gui(fs);
}
