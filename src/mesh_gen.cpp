#include "mesh_gen.h"
#define PAR_SHAPES_IMPLEMENTATION
#include "par_shapes.h"

#define MESH_GEN_IMPL(call)                                                                        \
    {                                                                                              \
        auto pmesh = (call);                                                                       \
        auto m = mesh(dev, pmesh->npoints, sizeof(vertex), pmesh->ntriangles * 3, [&](void* sb) {  \
            vertex* vs = (vertex*)sb;                                                              \
            vec3 *  ps = (vec3*)pmesh->points, *ns = (vec3*)pmesh->normals;                        \
            vec2*   ts = (vec2*)pmesh->tcoords;                                                    \
            for(int i = 0; i < pmesh->npoints; ++i) {                                              \
                *vs++ = vertex(*ps++, *ns++, *ts++);                                               \
            }                                                                                      \
            memcpy(                                                                                \
                (char*)sb + sizeof(vertex) * pmesh->npoints,                                       \
                pmesh->triangles,                                                                  \
                sizeof(uint16) * pmesh->ntriangles * 3                                             \
            );                                                                                     \
        });                                                                                        \
        par_shapes_free_mesh(pmesh);                                                               \
        return std::move(m);                                                                       \
    }

namespace mesh_gen {

mesh generate_sphere(device* dev, int slices, int stacks)
    MESH_GEN_IMPL(par_shapes_create_parametric_sphere(slices, stacks)) mesh
    generate_cylinder(device* dev, int slices, int stacks)
        MESH_GEN_IMPL(par_shapes_create_cylinder(slices, stacks)) mesh
    generate_cone(device* dev, int slices, int stacks)
        MESH_GEN_IMPL(par_shapes_create_cone(slices, stacks)) mesh
    generate_torus(device* dev, int slices, int stacks, float inner_radius)
        MESH_GEN_IMPL(par_shapes_create_torus(slices, stacks, inner_radius)) mesh
    generate_klein_bottle(device* dev, int slices, int stacks)
        MESH_GEN_IMPL(par_shapes_create_klein_bottle(slices, stacks))
    /*{
        auto pmesh = par_shapes_create_klein_bottle(slices, stacks);
        auto m = mesh(dev, pmesh->npoints, pmesh->ntriangles * 3, [&](void* sb) {
            vertex* vs = (vertex*)sb;
            vec3* ps = (vec3*)pmesh->points, * ns = (vec3*)pmesh->normals; vec2* ts =
    (vec2*)pmesh->tcoords; for (int i = 0; i < pmesh->npoints; ++i) { *vs++ = vertex(*ps++, *ns++,
    *ts++); } memcpy((char*)sb + sizeof(vertex) * pmesh->npoints, pmesh->triangles, sizeof(uint16) *
    pmesh->ntriangles * 3);
            });
        par_shapes_free_mesh(pmesh); return std::move(m);
    }*/

    mesh generate_trefoil_knot(device* dev, int slices, int stacks, float inner_radius)
        MESH_GEN_IMPL(par_shapes_create_trefoil_knot(slices, stacks, inner_radius)) mesh
    generate_plane(device* dev, int slices, int stacks)
        MESH_GEN_IMPL(par_shapes_create_plane(slices, stacks))

};
