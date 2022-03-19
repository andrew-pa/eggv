#include "renderer.h"
#include "imgui.h"
#include "imnodes.h"
#include "ImGuiFileDialog.h"
#include "imgui_impl_vulkan.h"
#include <iomanip>

void renderer::build_gui_menu(frame_state* fs) {
    if(ImGui::BeginMenuBar()) {
        if(ImGui::BeginMenu("File")) {
            if(ImGui::MenuItem("New graph")) {
                render_graph.clear();
                render_graph.push_back(screen_output_node);
                auto test = std::make_shared<render_node>(prototypes[1]);
                render_graph.push_back(test);
                screen_output_node->inputs[0] = {test, 0};
            }
            if(ImGui::MenuItem("Load graph")) {
                ImGuiFileDialog::Instance()->OpenDialog("LoadRenderGraphDlg", "Choose Render Graph", ".json", ".");
            }
            if(ImGui::MenuItem("Save graph")) {
                ImGuiFileDialog::Instance()->OpenDialog("SaveRenderGraphDlg", "Choose Render Graph", ".json", ".");
            }
            ImGui::EndMenu();
        }
        if(ImGui::BeginMenu("Options")) {
            ImGui::MenuItem("Compilation log", nullptr, &log_compile);
            ImGui::MenuItem("Viewport shapes", nullptr, &show_shapes);
            ImGui::EndMenu();
        }
        ImGui::EndMenuBar();
    }
}

void renderer::build_gui_graph_view(frame_state* fs) {
    ImNodes::GetIO().LinkDetachWithModifierClick.Modifier = &ImGui::GetIO().KeyCtrl;

    ImNodes::BeginNodeEditor();

    gui_node_attribs.clear();
    gui_links.clear();
    int next_attrib_id = 1;
    std::vector<std::shared_ptr<render_node>> deleted_nodes;

    for (const auto& node : render_graph) {
        auto i = node->id;
        ImNodes::BeginNode((int)i);
        ImNodes::BeginNodeTitleBar();
        ImGui::Text("%s[%u]", node->prototype->name(), node->subpass_count);
        ImGui::SameLine();
        if(ImGui::SmallButton(" x ")) {
            deleted_nodes.push_back(node);
        }
        ImNodes::EndNodeTitleBar();
        node->prototype->build_gui(this, node.get());

        // TODO: could we have blended inputs be represented by having the input dot and the output dot on the same line
        for (int input_ix = 0; input_ix < node->prototype->inputs.size(); ++input_ix) {
            auto id = next_attrib_id++;
            gui_node_attribs[id] = {node, input_ix, false};
            ImNodes::BeginInputAttribute(id, node->prototype->inputs[input_ix].count > 1 ? ImNodesPinShape_Quad : ImNodesPinShape_Circle);
            ImGui::Text("%s", node->prototype->inputs[input_ix].name.c_str());
            ImNodes::EndInputAttribute();
        }

        for (int output_ix = 0; output_ix < node->prototype->outputs.size(); ++output_ix) {
            auto id = next_attrib_id++;
            gui_node_attribs[id] = {node, output_ix, true};
            ImNodes::BeginOutputAttribute(id ,node->prototype->outputs[output_ix].count > 1 ? ImNodesPinShape_Quad : ImNodesPinShape_Circle);
            ImGui::Text("%s [%lu]", node->prototype->outputs[output_ix].name.c_str(), node->outputs[output_ix]);
            ImNodes::EndOutputAttribute();
        }

        ImNodes::EndNode();
    }

    for (const auto& [attrib_id, scnd] : gui_node_attribs) {
        const auto& [node, input_ix, is_output] = scnd;
        if (is_output) continue;
        auto out_attrib_id = std::find_if(
                gui_node_attribs.begin(), gui_node_attribs.end(),
                [&](const auto& p) { // this closure unsafely captures the reference `node` and `input_ix` by reference
                    return std::get<2>(p.second) && std::get<0>(p.second) == node->input_node(input_ix) 
                        && std::get<1>(p.second) == node->inputs[input_ix].second; 
                });
        if (out_attrib_id == gui_node_attribs.end()) continue;
        ImNodes::Link((int)gui_links.size(), attrib_id, out_attrib_id->first);
        gui_links.emplace_back( attrib_id, out_attrib_id->first );
    }

    if(ImGui::BeginPopupContextWindow()) {
        ImGui::Text("Create new node...");
        ImGui::Separator();
        for(const auto& node_type : prototypes) {
            if(ImGui::MenuItem(node_type->name()))
                render_graph.push_back(std::make_shared<render_node>(node_type));
        }
        ImGui::EndPopup();
    }

    /* ImNodes::MiniMap(0.1f, ImNodesMiniMapLocation_TopRight); */

    ImGui::SetCursorPos(ImVec2(8,8));
    if(ImGui::Button("Recompile"))
        should_recompile = true;

    if(ImGuiFileDialog::Instance()->Display("SaveRenderGraphDlg")) {
        if(ImGuiFileDialog::Instance()->IsOk()) {
            std::ofstream output(ImGuiFileDialog::Instance()->GetFilePathName());
            output << std::setw(4) << serialize_render_graph();
        }
        ImGuiFileDialog::Instance()->Close();
    }

    ImNodes::EndNodeEditor();

    if(ImGuiFileDialog::Instance()->Display("LoadRenderGraphDlg")) {
        if(ImGuiFileDialog::Instance()->IsOk()) {
            std::ifstream input(ImGuiFileDialog::Instance()->GetFilePathName());
            json data;
            input >> data;
            this->deserialize_render_graph(data);
        }
        ImGuiFileDialog::Instance()->Close();
    }

    int start_attrib, end_attrib;
    if(ImNodes::IsLinkCreated(&start_attrib, &end_attrib)) {
        if(std::get<2>(gui_node_attribs[start_attrib])) { // make sure start=input and end=output
            std::swap(start_attrib, end_attrib);
        }
        auto&[input_node, input_ix, is_input] = gui_node_attribs[start_attrib];
        auto&[output_node, output_ix, is_output] = gui_node_attribs[end_attrib];
        input_node->inputs[input_ix] = {output_node, output_ix};
    }

    int link_id;
    if(ImNodes::IsLinkDestroyed(&link_id)) {
        auto&[start_attrib, end_attrib] = gui_links[link_id];
        auto&[input_node, input_ix, is_input] = gui_node_attribs[start_attrib];
        auto&[output_node, output_ix, is_output] = gui_node_attribs[end_attrib];
        input_node->inputs[input_ix] = { {},0 };
        output_node->outputs[output_ix] = 0;
    }

    for(const auto& n : deleted_nodes) {
        auto f = std::find(render_graph.begin(), render_graph.end(), n);
        if(f != render_graph.end())
            render_graph.erase(f);
    }
}

void renderer::build_gui_stats(frame_state* fs) {
    ImGui::Text("%zu active meshes, %zu active lights, %zu active shapes, %zu running subpasses",
            active_meshes.size(), active_lights.size(), active_shapes.size(), subpass_order.size());
    ImGui::Text("%zu temp command buffers, %zu temp upload buffers", dev->tmp_cmd_buffers.size(), dev->tmp_upload_buffers.size());

    ImGui::Separator();
    ImGui::Text("Framebuffers:");
    if(ImGui::BeginTable("##RenderFramebufferTable", 5)) {
        ImGui::TableSetupColumn("ID");
        ImGui::TableSetupColumn("In use?");
        ImGui::TableSetupColumn("Format");
        ImGui::TableSetupColumn("Size");
        ImGui::TableSetupColumn("Count");
        ImGui::TableHeadersRow();
        for(const auto& [id, fb] : buffers) {
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::Text("%lu", id);
            ImGui::TableNextColumn();
            ImGui::Text("%s", fb.in_use ? "Y" : "N");
            ImGui::TableNextColumn();
            ImGui::Text("%s", vk::to_string(fb.img->info.format).c_str());
            ImGui::TableNextColumn();
            ImGui::Text("%u x %u", fb.img->info.extent.width, fb.img->info.extent.height);
            ImGui::TableNextColumn();
            ImGui::Text("%lu", fb.num_layers());
        }
        ImGui::EndTable();
    }

}

void renderer::build_gui_textures(frame_state* fs) {
    if (ImGui::BeginTable("#RenderTextureTable", 4, ImGuiTableFlags_Resizable)) {
        ImGui::TableSetupColumn("Name");
        ImGui::TableSetupColumn("Format");
        ImGui::TableSetupColumn("Size");
        ImGui::TableSetupColumn("Preview");
        ImGui::TableHeadersRow();
        for (auto& [name, img, imv, imtex] : texture_cache) {
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::Text("%s", name.c_str());
            ImGui::TableNextColumn();
            ImGui::Text("%s", vk::to_string(img->info.format).c_str());
            ImGui::TableNextColumn();
            ImGui::Text("%u x %u", img->info.extent.width, img->info.extent.height);
            ImGui::TableNextColumn();
            if(imtex == 0) {
                imtex = (uint64_t)ImGui_ImplVulkan_AddTexture((VkSampler)texture_sampler.get(),
                        imv.get(),
                        (VkImageLayout)vk::ImageLayout::eShaderReadOnlyOptimal);
            }
            ImGui::Image((ImTextureID)imtex, ImVec2(128, 128));
        }
        ImGui::EndTable();
    }
}

void renderer::build_gui(frame_state* fs) {
    if(!fs->gui_open_windows->at("Renderer")) return;
    if (!ImGui::Begin("Renderer", &fs->gui_open_windows->at("Renderer"), ImGuiWindowFlags_MenuBar)) { ImGui::End(); return; }

    build_gui_menu(fs);

    if(ImGui::BeginTabBar("##rendertabs")) {
        if(ImGui::BeginTabItem("Pipeline")) {
            build_gui_graph_view(fs);
            ImGui::EndTabItem();
        }

        if(ImGui::BeginTabItem("Statistics")) {
            build_gui_stats(fs);
            ImGui::EndTabItem();
        }
        
        if (ImGui::BeginTabItem("Textures")) {
            build_gui_textures(fs);
            ImGui::EndTabItem();
        }

        ImGui::EndTabBar();
    }
    ImGui::End();
}
