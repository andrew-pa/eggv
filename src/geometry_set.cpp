#include "geometry_set.h"

geometry_set::geometry_set(const std::string& path) : data(path), path(path) {
}

std::shared_ptr<mesh> geometry_set::load_mesh(device* dev, size_t index) {
	auto cv = this->mesh_cashe.find(index);
	if (cv != this->mesh_cashe.end()) {
		return cv->second;
	} else {
		auto h = this->header(index);
		auto msh = std::make_shared<mesh>(dev, );
	}
}

int32 geometry_set::num_meshes() const {
	return *(int32*)data.data();
}

const char* geometry_set::mesh_name(size_t index) const {
	return data.data() + this->header(index).name_ptr;
}

const geom_file::mesh_header& geometry_set::header(size_t index) const {
	return *((geom_file::mesh_header*)(data.data() + sizeof(int32)) + index);
}
