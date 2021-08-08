#include "physics.h"
#include "imgui.h"
using namespace reactphysics3d;

void rigid_body_trait::append_transform(scene_object*, mat4& T, frame_state*) {
	// TODO: interpolate between this and the previous frame's transform based on how much time we have left to simulate
	if (body->getType() != BodyType::STATIC) {
		mat4 local_t;
		this->body->getTransform().getOpenGLMatrix(&local_t[0][0]);
		T = local_t;
	}
}

void rigid_body_trait::postprocess_transform(scene_object*, const mat4& T, frame_state*) {
	if (body->getType() == BodyType::STATIC || should_grab_initial_transform) {
		Transform t = Transform::identity();
		t.setFromOpenGL((decimal*)&T[0][0]);
		body->setTransform(t);
		initial_transform = t;
		should_grab_initial_transform = false;
	}
}

const char* body_type_names[] = {
	"Static", "Kinematic", "Dynamic"
};

const char* shape_names[] = {
	"Triangle",
	"Sphere",
	"Capsule",
	"Box",
	"Convex Mesh",
	"Triangle Mesh",
	"Heightfield"
};

void rigid_body_trait::build_gui(scene_object*, frame_state*) {
	int body_type = (int)body->getType();
	if (ImGui::Combo("Type", &body_type, body_type_names, 3))
		body->setType((BodyType)body_type);
 
	auto mass = body->getMass();
	if (ImGui::DragFloat("Mass", &mass, 0.01f))
		body->setMass(mass);
	
	auto pos = initial_transform.getPosition();
	if (ImGui::DragFloat3("Initial Position", &pos.x, 0.01f))
		initial_transform.setPosition(pos);
	auto rot = initial_transform.getOrientation();
	if (ImGui::DragFloat4("Initial Rotation", &rot.x, 0.01f))
		initial_transform.setOrientation(rot);
	if (ImGui::Button("Reset")) {
		body->setTransform(initial_transform);
		body->setLinearVelocity(Vector3());
		body->setAngularVelocity(Vector3());
	}

	if (ImGui::BeginTable("##RigidBodyColliders", 3, ImGuiTableFlags_Resizable)) {
		ImGui::TableSetupColumn("Type");
		ImGui::TableSetupColumn("Params");
		ImGui::TableSetupColumn("Transform");
		ImGui::TableHeadersRow();
		for (uint i = 0; i < body->getNbColliders(); ++i) {
			auto col = body->getCollider(i);
			auto shape = col->getCollisionShape();
			ImGui::TableNextRow();
			ImGui::TableNextColumn();
			ImGui::Text("%s", shape_names[(int)shape->getName()]);
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
					if (ImGui::DragFloat("Height", &height, 0.01f))
						cap->setHeight(height);
					auto radius = cap->getRadius();
					if (ImGui::DragFloat("Radius", &radius, 0.01f))
						cap->setRadius(radius);
				} break;
				case CollisionShapeName::SPHERE: {
					auto s = dynamic_cast<SphereShape*>(shape);
					auto radius = s->getRadius();
					if (ImGui::DragFloat("Radius", &radius, 0.01f))
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
	
		}
		ImGui::EndTable();
	}

	static CollisionShapeName new_shape_name = CollisionShapeName::BOX;
	ImGui::Combo("Type", (int*)&new_shape_name, shape_names, 7);
	ImGui::SameLine();
	if (ImGui::Button("Add Collider")) {
		CollisionShape* shape;
		switch (new_shape_name) {
		case CollisionShapeName::BOX:
			shape = ((rigid_body_trait_factory*)this->parent)->phy->createBoxShape(Vector3(.5f, .5f, .5f));
			break;
		case CollisionShapeName::CAPSULE:
			shape = ((rigid_body_trait_factory*)this->parent)->phy->createCapsuleShape(1.f, 1.f);
			break;
		case CollisionShapeName::SPHERE:
			shape = ((rigid_body_trait_factory*)this->parent)->phy->createSphereShape(1.f);
			break;
		}
		body->addCollider(shape, Transform::identity());
	}
}

json rigid_body_trait::serialize() const {
	return json{};
}

void rigid_body_trait_factory::add_to(struct scene_object* obj, void* create_info) {
	// this is wrong, we need to sync the scene graph transform and the physics world transform in some way
	obj->traits[id()] = std::make_unique<rigid_body_trait>(this,
		world->createRigidBody(Transform::identity()));
}

void rigid_body_trait_factory::deserialize(struct scene* scene, struct scene_object* obj, json data) {

}

void build_physics_world_gui(frame_state*, bool* window_open, reactphysics3d::PhysicsWorld* world) {
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
		ImGui::End();
	}
}
