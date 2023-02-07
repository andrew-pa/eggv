
#define EGGV_IMPORT
#include "QuickHull.hpp"
#include "assimp/material.h"
#include "assimp/postprocess.h"
#include "ndcommon.h"
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <filesystem>
#include <fstream>
#include <glm/glm.hpp>
#include <iostream>
#include <optional>
#include <vector>

geom_file::vertex vertex_in_mesh(aiMesh* mesh, size_t i) {
    return {
        vec3(mesh->mVertices[i].x, mesh->mVertices[i].y, mesh->mVertices[i].z),
        vec3(mesh->mNormals[i].x, mesh->mNormals[i].y, mesh->mNormals[i].z),
        mesh->HasTextureCoords(0) ? vec2(mesh->mTextureCoords[0][i].x, mesh->mTextureCoords[0][i].y)
                                  : vec2(0.f)};
}

class geo_file {
    std::ofstream               output;
    bool                        gen_hulls;
    quickhull::QuickHull<float> hullgen;
    size_t                      mesh_count{}, max_mesh_count;

  public:
    geo_file(const std::filesystem::path& path, size_t max_mesh_count, bool gen_hulls)
        : output(path, std::ios::binary), gen_hulls(gen_hulls), max_mesh_count(max_mesh_count) {
        output.write((char*)&max_mesh_count, sizeof(int32_t));
        output.seekp(sizeof(geom_file::mesh_header) * max_mesh_count, std::ios::cur);
    }

    void write_mesh(aiMesh* mesh) {
        assert(mesh_count < max_mesh_count);
        auto mesh_index = mesh_count;
        mesh_count++;

        auto name_fptr = output.tellp();
        output.write(mesh->mName.C_Str(), mesh->mName.length + 1);

        auto vert_fptr = output.tellp();
        for(size_t i = 0; i < mesh->mNumVertices; ++i) {
            auto v = vertex_in_mesh(mesh, i);
            output.write((char*)&v, sizeof(geom_file::vertex));
        }

        auto idx_fptr = output.tellp();
        auto num_idx  = 0;
        for(size_t i = 0; i < mesh->mNumFaces; ++i) {
            for(size_t j = 0; j < mesh->mFaces[i].mNumIndices; ++j) {
                uint16 x = mesh->mFaces[i].mIndices[j];
                output.write((char*)&x, sizeof(uint16));
                num_idx++;
            }
        }

        auto hull_fptr = 0;
        if(gen_hulls) {
            hull_fptr = output.tellp();
            auto hull = hullgen.getConvexHull(
                (float*)mesh->mVertices, mesh->mNumVertices, false, true, 0.01f
            );
            auto     indices = hull.getIndexBuffer();
            uint16_t size    = indices.size();
            output.write((char*)&size, sizeof(uint16_t));
            for(size_t i = 0; i < indices.size(); ++i)
                output.write((char*)(indices.data() + i), sizeof(uint16_t));
        }

        auto resume_fptr = output.tellp();
        output.seekp(sizeof(int32) + mesh_index * sizeof(geom_file::mesh_header), std::ios::beg);
        auto header = geom_file::mesh_header(
            vert_fptr,
            idx_fptr,
            name_fptr,
            hull_fptr,
            mesh->mNumVertices,
            num_idx,
            mesh->mName.length + 1,
            mesh->mMaterialIndex,
            vec3(mesh->mAABB.mMin.x, mesh->mAABB.mMin.y, mesh->mAABB.mMin.z),
            vec3(mesh->mAABB.mMax.x, mesh->mAABB.mMax.y, mesh->mAABB.mMax.z)
        );
        output.write((char*)&header, sizeof(geom_file::mesh_header));
        output.seekp(resume_fptr);
    }
};

int main(int argc, char* argv[]) {
    std::optional<std::filesystem::path> output_path = {};
    std::vector<std::filesystem::path>   input_paths;

    bool gen_hulls = false;

    for(int i = 1; i < argc; ++i)
        if(strcmp(argv[i], "-o") == 0)
            output_path = argv[++i];
        else if(strcmp(argv[i], "--gen-hull") == 0)
            gen_hulls = true;
        else
            input_paths.emplace_back(argv[i]);

    if(input_paths.empty() || !output_path.has_value()) {
        std::cout << "usage: eggv_import [options] [input scene paths...] -o [output mesh data "
                     "path]\n";
        std::cout << "\t--gen-hull:\tcompute convex hull of each mesh\n";
        return 1;
    }

    std::vector<const aiScene*> inputs;
    size_t                      total_mesh_count = 0;

    std::cout << "loading meshes from " << input_paths.size() << " files\n";
    Assimp::Importer imp;
    for(const auto& input_path : input_paths) {
        std::cout << "\tloading " << input_path << "\n";
        imp.ReadFile(
            input_path,
            aiProcess_GenBoundingBoxes | aiProcessPreset_TargetRealtime_MaxQuality
                | aiProcess_FlipUVs
        );
        // TODO: this just leaks the scenes, which is fine for now since we're just going to write
        // them out into a file and exit, but is messy if you let the Importer own the scenes, it
        // will free them when you call ReadFile() again
        const auto* scene = imp.GetOrphanedScene();
        total_mesh_count += scene->mNumMeshes;
        inputs.push_back(scene);
    }
    std::cout << "finished loading meshes, got " << total_mesh_count << " meshes total\n";

    std::cout << "writing geometry set to " << output_path.value() << "\n";
    geo_file output(output_path.value(), total_mesh_count, gen_hulls);
    for(const auto* input : inputs) {
        std::cout << "\twriting meshes from \"" << input->mName.C_Str() << "\"\n";
        for(size_t i = 0; i < input->mNumMeshes; ++i) {
            std::cout << "\t\twriting mesh " << input->mMeshes[i]->mName.C_Str() << "\n";
            output.write_mesh(input->mMeshes[i]);
        }
    }

    std::cout << "finished!\n";

    return 0;
}
