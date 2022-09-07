#include "scene_components.h"
#include <imgui.h>

// TODO: break up headers and move modules into directories ie this should go with the other ECS
// stuff

// TODO: emlisp improvments
inline vec3 vec3_from_value(emlisp::runtime* rt, emlisp::value v) {
    auto [len, data] = rt->to_fvec(v);
    assert(len == 3);
    return *(vec3*)data;
}

inline vec4 vec4_from_value(emlisp::runtime* rt, emlisp::value v) {
    auto [len, data] = rt->to_fvec(v);
    assert(len == 4);
    return *(vec4*)data;
}

void transform_system::update_world_transforms(world::entity_handle e, const mat4& T) {
    auto d = this->entity_data.find(e);
    if(d != this->entity_data.end()) {
        auto& comp = d->second;
        comp.world = glm::scale(
            glm::translate(T, comp.translation) * glm::mat4_cast(comp.rotation), comp.scale
        );
    }
    const auto& p = d != this->entity_data.end() ? d->second.world : T;
    e.for_each_child([&](const auto& c) { this->update_world_transforms(c, p); });
}

void transform_system::update(const frame_state& fs) {
    this->update_world_transforms(cur_world.lock()->root(), mat4(1));
}

void transform_system::build_gui_for_entity(const frame_state& fs, entity_id selected_entity) {
    auto d = this->entity_data.find(selected_entity);
    if(d != this->entity_data.end()) {
        auto& comp = d->second;
        ImGui::DragFloat3("Translation", (float*)&comp.translation, 0.05f);
        ImGui::DragFloat4("Rotation", (float*)&comp.rotation, 0.05f);
        comp.rotation = glm::normalize(comp.rotation);
        ImGui::DragFloat3("Scale", (float*)&comp.scale, 0.05f, 0.f, FLT_MAX);
    }
}

void transform_system::init_scripting(emlisp::runtime* rt) {
    using namespace emlisp;
    rt->define_fn("c/+transform", [](runtime* rt, value args, void* cx) {
        auto* ts = (transform_system*)cx;
        auto tf = transform();
        if(nth(args, 1) != NIL)
            tf.translation = vec3_from_value(rt, nth(args, 1));
        if(nth(args, 2) != NIL)
            tf.rotation = quat(vec4_from_value(rt, nth(args, 1)));
        if(nth(args, 3) != NIL)
            tf.scale = vec3_from_value(rt, nth(args, 1));
        ts->add_entity(to_int(first(args)), tf);
        return NIL;
    }, this);
}

void light_system::build_gui_for_entity(const frame_state& fs, entity_id selected_entity) {
    auto d = this->entity_data.find(selected_entity);
    if(d != this->entity_data.end()) {
        auto& comp = d->second;
        ImGui::Combo("Type", (int*)&comp.type, "Directional\0Point\0");
        if(comp.type == light_type::directional) {
            ImGui::DragFloat3("Direction", (float*)&comp.param, 0.01f);
            comp.param = normalize(comp.param);
        } else if(comp.type == light_type::point) {
            ImGui::DragFloat("Falloff", &comp.param.x, 0.000f, 0.001f, 1000.f, "%.6f");
        }
        ImGui::ColorEdit3(
            "Color", (float*)&comp.color, ImGuiColorEditFlags_HDR | ImGuiColorEditFlags_Float
        );
        ImGui::Text("Light Render Index: %lu", comp._render_index);
    }
}

void light_system::generate_viewport_shapes(
    const std::function<void(viewport_shape)>& add_shape, const frame_state& fs
) {
    auto transforms = cur_world.lock()->system<transform_system>();
    for(const auto& [id, li] : this->entity_data) {
        if(li.type == light_type::point) {
            auto trf = transforms->get_data_for_entity(id);
            add_shape(viewport_shape{
                viewport_shape_type::axis, li.color, scale(trf.world, vec3(0.25f))});
        }
    }
}

void light_system::init_scripting(emlisp::runtime* rt) {
    using namespace emlisp;
    rt->define_fn("c/+light", [](runtime* rt, value args, void* cx) {
        auto* ls = (light_system*)cx;
        auto lc = light();
        if(nth(args, 1) != NIL) {
            auto t = nth(args, 1);
            if(t == rt->symbol("directional")) lc.type = light_type::directional;
            else if(t == rt->symbol("point")) lc.type = light_type::point;
        }
        if(nth(args, 2) != NIL)
            lc.param = vec3_from_value(rt, nth(args, 1));
        if(nth(args, 3) != NIL)
            lc.color = vec3_from_value(rt, nth(args, 1));
        ls->add_entity(to_int(first(args)), lc);
        return NIL;
    }, this);
}

void camera_system::build_gui_for_entity(const frame_state& fs, entity_id selected_entity) {
    auto d = this->entity_data.find(fs.selected_entity);
    if(d != this->entity_data.end()) {
        auto& comp = d->second;
        ImGui::DragFloat("Field of View", &comp.fov, 0.1f, pi<float>() / 8.f, pi<float>());
        if(!this->active_camera_id.has_value()
           || selected_entity != this->active_camera_id.value()) {
            if(ImGui::Button("Make Active Camera")) this->active_camera_id = selected_entity;
        }
    }
}

void camera_system::generate_viewport_shapes(
    const std::function<void(viewport_shape)>& add_shape, const frame_state& fs
) {
    auto transforms = cur_world.lock()->system<transform_system>();
    for(const auto& [id, _] : this->entity_data) {
        auto trf = transforms->get_data_for_entity(id);
        add_shape(viewport_shape{
            viewport_shape_type::axis, vec3(1.f), scale(trf.world, vec3(0.4f, 0.4f, 1.f))});
    }
}


void camera_system::init_scripting(emlisp::runtime* rt) {
    using namespace emlisp;
    rt->define_fn("c/+camera", [](runtime* rt, value args, void* cx) {
        auto* cs = (camera_system*)cx;
        auto cam = camera();
        if(nth(args, 1) != NIL)
            cam.fov = to_float(nth(args, 1));
        cs->add_entity(to_int(first(args)), cam);
        return NIL;
    }, this);
}


