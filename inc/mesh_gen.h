#pragma once
#include "renderer.h"

struct mesh;
class device;

namespace mesh_gen {

mesh generate_sphere(device* dev, int slices, int stacks);
mesh generate_cylinder(device* dev, int slices, int stacks);
mesh generate_cone(device* dev, int slices, int stacks);
mesh generate_torus(device* dev, int slices, int stacks, float inner_radius);
mesh generate_klein_bottle(device* dev, int slices, int stacks);
mesh generate_trefoil_knot(device* dev, int slices, int stacks, float inner_radius);
mesh generate_plane(device* dev, int slices, int stacks);

};  // namespace mesh_gen
