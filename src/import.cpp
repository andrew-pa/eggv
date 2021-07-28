
#define EGGV_IMPORT
#include "cmmn.h"
#include "mesh.h"
#include <assimp/Importer.hpp>
#include "assimp/postprocess.h"
#include <assimp/scene.h>

std::shared_ptr<scene_object> convert_scene_node(scene& sc, aiNode* node) {
    auto obj = std::make_shared<scene_object>(node->mName.C_Str());
    for (auto i = 0; i < node->mNumChildren; ++i) {
        obj->children.push_back(convert_scene_node(sc, node->mChildren[i]));
    }
    return obj;
}

int main(int argc, char* argv[]) {
    char *input_path = nullptr, *output_path = nullptr, *scene_path = nullptr;
    bool write_scene = false;

    for(int i = 0; i < argc; ++i) {
        if(strcmp(argv[i], "-i") == 0) {
            input_path = argv[++i];
        }
        else if(strcmp(argv[i], "-o") == 0) {
            output_path = argv[++i];
        }
        else if (strcmp(argv[i], "-scene") == 0) {
            write_scene = true;
            scene_path = argv[++i];
        }
    }

    std::cout << input_path << " => " << output_path << "\n";

    Assimp::Importer imp;
    imp.ReadFile(input_path, aiProcess_GenBoundingBoxes | aiProcessPreset_TargetRealtime_MaxQuality);
    std::cout << "\t loaded model\n";
    
    auto scene = imp.GetScene();
    std::ofstream output(output_path, std::ios::binary);
    output.write((char*)&scene->mNumMeshes, (std::streamsize)sizeof(int32_t));
    output.seekp(sizeof(geom_file::mesh_header) * scene->mNumMeshes, std::ios::cur);
    for (size_t mesh_ix = 0; mesh_ix < scene->mNumMeshes; ++mesh_ix) {
        auto mesh = scene->mMeshes[mesh_ix];
        std::cout << "\t writing mesh #" << mesh_ix << " \"" << mesh->mName.C_Str() << "\"\n";
        auto name_fptr = output.tellp();
        output.write(mesh->mName.C_Str(), mesh->mName.length+1);
        auto vert_fptr = output.tellp();
        auto has_tex_coords = mesh->HasTextureCoords(0);
        for (size_t i = 0; i < mesh->mNumVertices; ++i) {
            vertex v(
                vec3(mesh->mVertices[i].x, mesh->mVertices[i].y, mesh->mVertices[i].z),
                vec3(mesh->mNormals[i].x, mesh->mNormals[i].y, mesh->mNormals[i].z),
                has_tex_coords ?
                    vec2(mesh->mTextureCoords[0][i].x, mesh->mTextureCoords[0][i].y)
					: vec2(0.f)
            );
            output.write((char*)&v, sizeof(vertex));
        }
        auto idx_fptr = output.tellp();
        auto num_idx = 0;
        for (size_t i = 0; i < mesh->mNumFaces; ++i) {
            for (size_t j = 0; j < mesh->mFaces[i].mNumIndices; ++j) {
                uint16 x = mesh->mFaces[i].mIndices[j];
                output.write((char*)&x, sizeof(uint16));
                num_idx++;
            }
        }
        auto resume_fptr = output.tellp();
        output.seekp(sizeof(int32) + mesh_ix*sizeof(geom_file::mesh_header), std::ios::beg);
        auto header = geom_file::mesh_header(
            vert_fptr,
            idx_fptr,
            name_fptr,
            mesh->mNumVertices,
            num_idx, mesh->mName.length+1, 
            mesh->mMaterialIndex,
            vec3(mesh->mAABB.mMin.x,mesh->mAABB.mMin.y,mesh->mAABB.mMin.z),
            vec3(mesh->mAABB.mMax.x,mesh->mAABB.mMax.y,mesh->mAABB.mMax.z)
        );
        output.write((char*)&header, sizeof(geom_file::mesh_header));
        output.seekp(resume_fptr);
    }

    if (write_scene) {
        std::cout << "\tcreating scene file\n";
        ::scene out_scene({}, (std::shared_ptr<scene_object>)nullptr);
        out_scene.root = convert_scene_node(out_scene, scene->mRootNode);
        std::ofstream outs(scene_path);
        outs << out_scene.serialize();
    }

    return 0;
}
