#include "physics.h"
#include "imgui.h"
#include "reactphysics3d/mathematics/Vector3.h"
using namespace reactphysics3d;

// TODO:
// + offset transform for aliging 3d objects and rigid bodies
// + mass editing
// + proper moment of inertia calculations
// + automatic mesh bbox -> collider
// mesh colliders
// + ser/deser physics objects

void rigid_body_trait::remove_from(scene_object*){
    auto f = (rigid_body_trait_factory*)this->parent;
    f->world->destroyRigidBody(body);
}

void rigid_body_trait::append_transform(scene_object* obj, mat4& T, frame_state* fs) {
    // TODO: interpolate between this and the previous frame's transform based on how much time we have left to simulate
    if (body->getType() != BodyType::STATIC) {
        mat4 local_t;
        this->body->getTransform().getOpenGLMatrix(&local_t[0][0]);
        T = local_t;
        auto offset_t = obj->traits.find(TRAIT_ID_TRANSFORM);
        if (offset_t != obj->traits.end()) {
            offset_t->second->append_transform(obj, T, fs);
        }
    }
}

void rigid_body_trait::postprocess_transform(scene_object*, const mat4& T, frame_state*) {
    if (/*body->getType() == BodyType::STATIC ||*/ should_grab_initial_transform) {
        Transform t = Transform::identity();
        t.setFromOpenGL((decimal*)&T[0][0]);
        body->setTransform(t);
        initial_transform = t;
        should_grab_initial_transform = false;
    }
}

void rigid_body_trait::reset_body() {
	body->setTransform(initial_transform);
	body->setLinearVelocity(Vector3());
	body->setAngularVelocity(Vector3());
}

const char* body_type_names[] = {
    "Static", "Kinematic", "Dynamic"
};

const char* shape_names[] = {
    "Triangle", // apparently internal use only
    "Sphere",
    "Capsule",
    "Box",
    "Convex Mesh",
    "Triangle Mesh",
    "Heightfield"
};

void rigid_body_trait::build_gui(scene_object* obj, frame_state*) {
    int body_type = (int)body->getType();
    if (ImGui::Combo("Type", &body_type, body_type_names, 3))
        body->setType((BodyType)body_type);

    auto mass = body->getMass();
    if (ImGui::DragFloat("Mass", &mass, 0.01f)) {
        body->setMass(mass);
        body->updateMassPropertiesFromColliders();
    }

    auto pos = initial_transform.getPosition();
    if (ImGui::DragFloat3("Initial Position", &pos.x, 0.01f))
        initial_transform.setPosition(pos);
    auto rot = initial_transform.getOrientation();
    if (ImGui::DragFloat4("Initial Rotation", &rot.x, 0.01f))
        initial_transform.setOrientation(rot);
    if (ImGui::Button("Reset")) {
        this->reset_body();
    }

    if (ImGui::BeginTable("##RigidBodyColliders", 4, ImGuiTableFlags_Resizable)) {
        ImGui::TableSetupColumn("Type");
        ImGui::TableSetupColumn("Params");
        ImGui::TableSetupColumn("Transform");
        ImGui::TableSetupColumn("Properties");
        ImGui::TableHeadersRow();

        for (uint i = 0; i < body->getNbColliders(); ++i) {
            auto col = body->getCollider(i);
            auto shape = col->getCollisionShape();
            ImGui::PushID(i);
            ImGui::TableNextRow();

            ImGui::TableNextColumn();
            ImGui::Text("%s", shape_names[(int)shape->getName()]);
            if (ImGui::Button("-")) {
                ImGui::PopID();
                body->removeCollider(col);
                continue;
            }

            ImGui::TableNextColumn();
            switch (shape->getName()) {
                case CollisionShapeName::BOX: {
                    auto box = dynamic_cast<BoxShape*>(shape);
                    auto ext = box->getHalfExtents();
                    if (ImGui::DragFloat3("Half Extents", &ext.x, 0.01f))
                        box->setHalfExtents(ext);
                } break;
                case CollisionShapeName::CAPSULE: {
                    auto cap = dynamic_cast<CapsuleShape*>(shape);
                    auto height = cap->getHeight();
                    if (ImGui::DragFloat("Height", &height, 0.01f, 0.01f))
                    cap->setHeight(height);
                    auto radius = cap->getRadius();
                    if (ImGui::DragFloat("Radius", &radius, 0.01f, 0.01f))
                    cap->setRadius(radius);
                } break;
                case CollisionShapeName::SPHERE: {
                    auto s = dynamic_cast<SphereShape*>(shape);
                    auto radius = s->getRadius();
                    if (ImGui::DragFloat("Radius", &radius, 0.01f, 0.01f))
                    s->setRadius(radius);
                } break;
            }

            ImGui::TableNextColumn();
            auto tf = col->getLocalToBodyTransform();
            bool tf_changed = false;
            auto pos = tf.getPosition();
            if (ImGui::DragFloat3("Position", &pos.x, 0.01f)) {
                tf.setPosition(pos);
                tf_changed = true;
            }
            auto rot = tf.getOrientation();
            if (ImGui::DragFloat4("Rotation", &rot.x, 0.01f)) {
                tf.setOrientation(rot);
                tf_changed = true;
            }
            if (tf_changed)
                col->setLocalToBodyTransform(tf);

            ImGui::TableNextColumn();
            auto& mat = col->getMaterial();
            auto mat_changed = false;
            auto bouncy = mat.getBounciness(),
                 mass_density = mat.getMassDensity(),
                 friction_coeff = mat.getFrictionCoefficient(),
                 rolling_resist = mat.getRollingResistance();
            if (ImGui::DragFloat("Mass Density", &mass_density, 0.01f, 0.f)) {
                mat.setMassDensity(mass_density);
                mat_changed = true;
            }
            if (ImGui::DragFloat("Bounciness", &bouncy, 0.01f, 0.f, 1.f)) {
                mat.setBounciness(bouncy);
                mat_changed = true;
            }
            if (ImGui::DragFloat("Friction", &friction_coeff, 0.01f, 0.f, 1.f)) {
                mat.setFrictionCoefficient(friction_coeff);
                mat_changed = true;
            }
            if (ImGui::DragFloat("Rolling Resistance", &rolling_resist, 0.01f, 0.f, 1.f)) {
                mat.setRollingResistance(rolling_resist);
                mat_changed = true;
            }
            if(mat_changed)
                body->updateMassPropertiesFromColliders();
            ImGui::PopID();
        }
        ImGui::EndTable();
    }

    static CollisionShapeName new_shape_name = CollisionShapeName::BOX;
    ImGui::Combo("Type ", (int*)&new_shape_name, shape_names, 7);
    ImGui::SameLine();
    if (ImGui::Button("Add Collider")) {
        CollisionShape* shape = nullptr;
        switch (new_shape_name) {
            case CollisionShapeName::BOX: {
                auto mt = obj->traits.find(TRAIT_ID_MESH);
                if(mt != obj->traits.end()) {
                    auto mtt = ((mesh_trait*)mt->second.get())->bounds.extents()/2.f + 0.001f;
                    shape = ((rigid_body_trait_factory*)this->parent)->phy->createBoxShape(Vector3(mtt.x, mtt.y, mtt.z));
                } else {
                    shape = ((rigid_body_trait_factory*)this->parent)->phy->createBoxShape(Vector3(.5f, .5f, .5f));
                }
            } break;
            case CollisionShapeName::CAPSULE:
                shape = ((rigid_body_trait_factory*)this->parent)->phy->createCapsuleShape(1.f, 1.f);
                break;
            case CollisionShapeName::SPHERE:
                shape = ((rigid_body_trait_factory*)this->parent)->phy->createSphereShape(1.f);
                break;
        }
        body->addCollider(shape, Transform::identity());
        body->updateMassPropertiesFromColliders();
    }
}

json serialize(const Vector3& v) {
    return {v.x, v.y, v.z};
}

Vector3 deserialize(const json& v) {
    return Vector3(v[0], v[1], v[2]);
}


json rigid_body_trait::serialize() const {
    json colliders = json::array();
    for(auto i = 0; i < body->getNbColliders(); ++i) {
        const auto& col = body->getCollider(i);
        const auto& tf = col->getLocalToBodyTransform();
        
        auto shape = col->getCollisionShape();
        json shape_params;
        switch (shape->getName()) {
            case CollisionShapeName::BOX: {
                auto box = dynamic_cast<BoxShape*>(shape);
                shape_params = { {"extent", ::serialize(box->getHalfExtents())} };
            } break;
            case CollisionShapeName::CAPSULE: {
                auto cap = dynamic_cast<CapsuleShape*>(shape);
                shape_params = { {"height", cap->getHeight()}, {"radius", cap->getRadius()} };
            } break;
            case CollisionShapeName::SPHERE: {
                auto s = dynamic_cast<SphereShape*>(shape);
                shape_params = { {"radius", s->getRadius()} };
            } break;
        }

        colliders.push_back({
            {"type", shape->getName()},
            {"position", ::serialize(tf.getPosition())},
            {"rotation", {tf.getOrientation().x, tf.getOrientation().y, tf.getOrientation().z, tf.getOrientation().w}},
            {"mass_density", col->getMaterial().getMassDensity()},
            {"bounciness", col->getMaterial().getBounciness()},
            {"friction", col->getMaterial().getFrictionCoefficient()},
            {"rolling_resist", col->getMaterial().getRollingResistance()},
            {"shape", shape_params}
        });
    }
    return json {
        {"type", body->getType()},
        {
            "initial_transform", {
                {"position", ::serialize(initial_transform.getPosition())},
                {"rotation", {initial_transform.getOrientation().x, initial_transform.getOrientation().y, initial_transform.getOrientation().z, initial_transform.getOrientation().w}},
            }
        },
        {"mass", body->getMass()},
        {"colliders", colliders}
    };
}

void rigid_body_trait_factory::add_to(struct scene_object* obj, void* create_info) {
    // this is wrong, we need to sync the scene graph transform and the physics world transform in some way
    obj->traits[id()] = std::make_unique<rigid_body_trait>(this,
            world->createRigidBody(Transform::identity()), Transform::identity());
}

void rigid_body_trait_factory::deserialize(struct scene* scene, struct scene_object* obj, json data) {
    auto it = data["initial_transform"];
    auto init_rot = Quaternion(it["rotation"][0], it["rotation"][1], it["rotation"][2], it["rotation"][3]);
    init_rot.normalize();
    auto initial_transform = Transform(::deserialize(it["position"]), init_rot);
    auto body = world->createRigidBody(initial_transform);
    body->setType((BodyType)data["type"]);
    for(const auto& col : data["colliders"]) {
        const auto name = (CollisionShapeName)col["type"];
        CollisionShape* shape;
        switch(name) {
            case CollisionShapeName::BOX:
                shape = this->phy->createBoxShape(::deserialize(col["shape"]["extent"]));
                break;
            case CollisionShapeName::CAPSULE:
                shape = this->phy->createCapsuleShape(col["shape"]["radius"], col["shape"]["height"]);
                break;
            case CollisionShapeName::SPHERE:
                shape = this->phy->createSphereShape(col["shape"]["radius"]);
                break;
        }
        auto transform = Transform(::deserialize(col["position"]),
            Quaternion(col["rotation"][0],col["rotation"][1],col["rotation"][2],col["rotation"][3]));
        auto c = body->addCollider(shape, transform);
        auto& mat = c->getMaterial();
        mat.setMassDensity(col["mass_density"]);
        mat.setBounciness(col["bounciness"]);
        mat.setFrictionCoefficient(col["friction"]);
        mat.setRollingResistance(col["rolling_resist"]);
    }
    if(body->getType() != BodyType::STATIC) body->updateMassPropertiesFromColliders();
    obj->traits[id()] = std::make_unique<rigid_body_trait>(this, body, initial_transform, true);
}

void build_physics_world_gui(frame_state* fs, bool* window_open, reactphysics3d::PhysicsWorld* world) {
    if (*window_open) {
        ImGui::Begin("Physics World", window_open);
        bool grav_enb = world->isGravityEnabled();
        if (ImGui::Checkbox("Enable Gravity", &grav_enb))
            world->setIsGravityEnabled(grav_enb);
        if (grav_enb) {
            auto grav = world->getGravity();
            if (ImGui::DragFloat3("Gravity", &grav.x, 0.01f))
                world->setGravity(grav);
        }
        if (ImGui::Button("Reset All")) {
            fs->current_scene->for_each_object([](std::shared_ptr<scene_object> ob) {
                auto rbt = ob->traits.find(TRAIT_ID_RIGID_BODY);
                if (rbt != ob->traits.end()) {
                    auto rb = (rigid_body_trait*)rbt->second.get();
                    rb->reset_body();
                }
            });
        }
        ImGui::End();
    }
}

struct physics_debug_shape_render_node_data : render_node_data {
    vk::UniquePipeline triangle_pipeline;
    physics_debug_shape_render_node_data(vk::UniquePipeline&& pipe) : triangle_pipeline(std::move(pipe)) {}
    json serialize() const override { return json{}; }
};

physics_debug_shape_render_node_prototype::physics_debug_shape_render_node_prototype(device* dev, PhysicsWorld* world)
    : world(world)
{
    inputs = {
        framebuffer_desc{"color", vk::Format::eUndefined, framebuffer_type::color, framebuffer_mode::blend_input},
        // framebuffer_desc{"depth", vk::Format::eUndefined, framebuffer_type::depth}, // how do we indicate we want this bound as the depth buffer??????????
    };
    outputs = {
        framebuffer_desc{"color", vk::Format::eUndefined, framebuffer_type::color},
    };

    desc_layout = dev->create_desc_set_layout({
            vk::DescriptorSetLayoutBinding(0, vk::DescriptorType::eUniformBuffer, 1, vk::ShaderStageFlagBits::eVertex)
            });

    vk::PushConstantRange push_consts[] = {
        vk::PushConstantRange { vk::ShaderStageFlagBits::eVertex, 0, sizeof(mat4) },
        vk::PushConstantRange { vk::ShaderStageFlagBits::eFragment, sizeof(mat4), sizeof(vec4) }
    };

    pipeline_layout = dev->dev->createPipelineLayoutUnique(vk::PipelineLayoutCreateInfo {
            {}, 1, &desc_layout.get(), 2, push_consts
            });

    geo_buffer = std::make_unique<buffer>(dev, 1024*sizeof(DebugRenderer::DebugLine) + 1024*sizeof(DebugRenderer::DebugTriangle),
            vk::BufferUsageFlagBits::eVertexBuffer, vk::MemoryPropertyFlagBits::eHostCoherent,
            (void**)&geo_bufmap);

    world->setIsDebugRenderingEnabled(true);
    auto& dr = world->getDebugRenderer();
    dr.setIsDebugItemDisplayed(DebugRenderer::DebugItem::COLLIDER_AABB, true);
    dr.setIsDebugItemDisplayed(DebugRenderer::DebugItem::COLLIDER_BROADPHASE_AABB, true);
    dr.setIsDebugItemDisplayed(DebugRenderer::DebugItem::COLLISION_SHAPE, true);
}

void physics_debug_shape_render_node_prototype::build_gui(class renderer*, struct render_node* node) {
    auto enabled = world->getIsDebugRenderingEnabled();
    if (ImGui::Checkbox("Enable", &enabled))
        world->setIsDebugRenderingEnabled(enabled);
    auto& dr = world->getDebugRenderer();
    const std::tuple<DebugRenderer::DebugItem, const char*> items[] = {
        {DebugRenderer::DebugItem::COLLIDER_AABB, "Collider AABBs"},
        {DebugRenderer::DebugItem::COLLIDER_BROADPHASE_AABB, "Collider Broadphase AABBs"},
        {DebugRenderer::DebugItem::COLLISION_SHAPE, "Collision Shapes"},
        {DebugRenderer::DebugItem::CONTACT_NORMAL, "Contact Normals"},
        {DebugRenderer::DebugItem::CONTACT_POINT, "Contact Points"}
    };
    ImGui::BeginGroup();
    for (int i = 0; i < IM_ARRAYSIZE(items); ++i) {
        bool enb = dr.getIsDebugItemDisplayed(std::get<0>(items[i]));
        if (ImGui::Checkbox(std::get<1>(items[i]), &enb))
            dr.setIsDebugItemDisplayed(std::get<0>(items[i]), enb);
    }
    ImGui::EndGroup();
}

void physics_debug_shape_render_node_prototype::collect_descriptor_layouts(render_node* node, std::vector<vk::DescriptorPoolSize>& pool_sizes, 
        std::vector<vk::DescriptorSetLayout>& layouts, std::vector<vk::UniqueDescriptorSet*>& outputs)
{
    pool_sizes.push_back(vk::DescriptorPoolSize(vk::DescriptorType::eUniformBuffer, 1));
    layouts.push_back(desc_layout.get());
    outputs.push_back(&node->desc_set);
}

void physics_debug_shape_render_node_prototype::update_descriptor_sets(renderer* r, render_node* node,
        std::vector<vk::WriteDescriptorSet>& writes, arena<vk::DescriptorBufferInfo>& buf_infos,
        arena<vk::DescriptorImageInfo>& img_infos)
{
    auto b = buf_infos.alloc(vk::DescriptorBufferInfo(r->frame_uniforms_buf->buf, 0, sizeof(frame_uniforms)));
    writes.push_back(vk::WriteDescriptorSet(node->desc_set.get(), 0, 0, 1, vk::DescriptorType::eUniformBuffer, nullptr, b));
}

vk::UniquePipeline physics_debug_shape_render_node_prototype::generate_pipeline(renderer* r, render_node* node,
        vk::RenderPass render_pass, uint32_t subpass)
{
    vk::PipelineShaderStageCreateInfo shader_stages[] = {
        vk::PipelineShaderStageCreateInfo {
            {}, vk::ShaderStageFlagBits::eVertex,
            r->dev->load_shader("simple.vert.spv"), "main"
        },
        vk::PipelineShaderStageCreateInfo {
            {}, vk::ShaderStageFlagBits::eFragment,
            r->dev->load_shader("solid-color.frag.spv"), "main"
        }
    };

    auto vertex_binding = vk::VertexInputBindingDescription { 0, sizeof(Vector3) + sizeof(uint32), vk::VertexInputRate::eVertex };
    constexpr static vk::VertexInputAttributeDescription ds_vertex_attribute_description[] = {
        vk::VertexInputAttributeDescription{0, 0, vk::Format::eR32G32B32Sfloat, 0},
        vk::VertexInputAttributeDescription{1, 0, vk::Format::eR8G8B8A8Uint, sizeof(Vector3)},
    };

    auto vertex_input_info = vk::PipelineVertexInputStateCreateInfo {
        {}, 1, &vertex_binding,
            2, ds_vertex_attribute_description
    };

    auto input_assembly = vk::PipelineInputAssemblyStateCreateInfo {
        {}, vk::PrimitiveTopology::eTriangleList
    };

    auto viewport_state = vk::PipelineViewportStateCreateInfo {
        {}, 1, &r->full_viewport, 1, &r->full_scissor
    };

    auto rasterizer_state = vk::PipelineRasterizationStateCreateInfo{
        {}, false, false, vk::PolygonMode::eLine, vk::CullModeFlagBits::eNone,
            vk::FrontFace::eCounterClockwise, false, 0.f, 0.f, 0.f, 1.f
    };

    auto multisample_state = vk::PipelineMultisampleStateCreateInfo{};

    auto depth_stencil_state = vk::PipelineDepthStencilStateCreateInfo{
        {}, false, false, vk::CompareOp::eLess, false, false
    };

    vk::PipelineColorBlendAttachmentState color_blend_att[] = {
        vk::PipelineColorBlendAttachmentState(false,
                vk::BlendFactor::eOne, vk::BlendFactor::eOne, vk::BlendOp::eAdd,
                vk::BlendFactor::eOne, vk::BlendFactor::eOne, vk::BlendOp::eAdd,
                vk::ColorComponentFlagBits::eR|vk::ColorComponentFlagBits::eG
                |vk::ColorComponentFlagBits::eB |vk::ColorComponentFlagBits::eA) 
    };
    auto color_blending_state = vk::PipelineColorBlendStateCreateInfo {
        {}, false, vk::LogicOp::eCopy, 1, color_blend_att
    };

    auto cfo = vk::GraphicsPipelineCreateInfo(
            {},
            2, shader_stages,
            &vertex_input_info,
            &input_assembly,
            nullptr,
            &viewport_state,
            &rasterizer_state,
            &multisample_state,
            &depth_stencil_state,
            &color_blending_state,
            nullptr,
            this->pipeline_layout.get(),
            render_pass, subpass
            );

    node->data = std::make_unique<physics_debug_shape_render_node_data>(r->dev->dev->createGraphicsPipelineUnique(nullptr, cfo));

    input_assembly.topology = vk::PrimitiveTopology::eLineList;

    return r->dev->dev->createGraphicsPipelineUnique(nullptr, cfo);// .value;
}

void physics_debug_shape_render_node_prototype::generate_command_buffer_inline(renderer* r, render_node* node, vk::CommandBuffer& cb) {
    if (!world->getIsDebugRenderingEnabled()) return;
    auto& dr = world->getDebugRenderer();
    if (dr.getNbLines() > 0) {
        memcpy(geo_bufmap, dr.getLinesArray(),
                sizeof(DebugRenderer::DebugLine) * max(1024, (int)dr.getNbLines()));
    }
    if (dr.getNbTriangles() > 0) {
        memcpy((char*)geo_bufmap + 1024*sizeof(DebugRenderer::DebugLine), dr.getTrianglesArray(),
                sizeof(DebugRenderer::DebugTriangle) * max(1024, (int)dr.getNbTriangles()));
    }

    if (dr.getNbLines() > 0) {
        cb.bindPipeline(vk::PipelineBindPoint::eGraphics, node->pipeline.get());
        cb.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, this->pipeline_layout.get(),
                0, { node->desc_set.get() }, {});
        cb.bindVertexBuffers(0, { geo_buffer->buf }, { 0 });
        cb.pushConstants<mat4>(this->pipeline_layout.get(), vk::ShaderStageFlagBits::eVertex, 0, { mat4(1) });
        cb.pushConstants<vec4>(this->pipeline_layout.get(), vk::ShaderStageFlagBits::eFragment, sizeof(mat4), { vec4(1.f, 0.f, 1.f, 1.f) });
        cb.draw(2 * dr.getNbLines(), 1, 0, 0);
    }

    if (dr.getNbTriangles() > 0) {
        auto data = (physics_debug_shape_render_node_data*)(node->data.get());
        cb.bindPipeline(vk::PipelineBindPoint::eGraphics, data->triangle_pipeline.get());
        cb.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, this->pipeline_layout.get(),
                0, { node->desc_set.get() }, {});
        cb.bindVertexBuffers(0, { geo_buffer->buf }, { sizeof(DebugRenderer::DebugLine)*1024 });
        cb.pushConstants<mat4>(this->pipeline_layout.get(), vk::ShaderStageFlagBits::eVertex, 0, { mat4(1) });
        cb.pushConstants<vec4>(this->pipeline_layout.get(), vk::ShaderStageFlagBits::eFragment, sizeof(mat4), { vec4(0.f, 1.f, 1.f, 1.f) });
        cb.draw(3 * dr.getNbTriangles(), 1, 0, 0);
    }
}
