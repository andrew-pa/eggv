
#define EGGV_IMPORT
#include <iostream>
#include <fstream>
#include <vector>
#include <glm/glm.hpp>
#include "nlohmann/json.hpp"
#include "uuid.h"
#include "ndcommon.h"
#include <assimp/Importer.hpp>
#include "assimp/postprocess.h"
#include <assimp/scene.h>
#include "assimp/material.h"
#include "QuickHull.hpp"

using namespace nlohmann;

static std::mt19937 default_random_gen;
static uuids::uuid_random_generator uuid_gen = uuids::uuid_random_generator(default_random_gen);

json make_scene_object(std::string name) {
    return json{
        {"name", name},
        {"id", uuids::to_string(uuid_gen())},
        {"c", json::array()},
        {"t", json::object()}
    };
}

json convert_scene_node(json& sc, aiNode* node, const char* geometry_source_path, const aiScene* in_scene, const std::vector<uuids::uuid>& matix_to_id) {
    aiVector3D scaling, position;
    aiQuaternion rotation;
    node->mTransformation.Decompose(scaling, rotation, position);
    auto obj = make_scene_object(node->mName.C_Str());
    obj["t"][std::to_string(TRAIT_ID_TRANSFORM)] = json {
        {"t", json::array({position.x, position.y, position.z})},
        {"s", json::array({scaling.x, scaling.y, scaling.z})},
        {"r", json::array({rotation.x, rotation.y, rotation.z, rotation.w})},
    };
    for (auto i = 0; i < node->mNumChildren; ++i) {
        obj["c"].push_back(convert_scene_node(sc, node->mChildren[i], geometry_source_path, in_scene, matix_to_id));
    }
    if(node->mNumMeshes == 1) {
        obj["t"][std::to_string(TRAIT_ID_MESH)] = json {
            {"geo_src", geometry_source_path},
            {"ix", node->mMeshes[0]},
            {"mat", uuids::to_string(matix_to_id[in_scene->mMeshes[node->mMeshes[0]]->mMaterialIndex])}
        };
    } else {
        for(auto i = 0; i < node->mNumMeshes; ++i) {
            auto mesh_ch = make_scene_object(std::string(node->mName.C_Str()) + "/" + std::to_string(node->mMeshes[i]));
            mesh_ch["t"][std::to_string(TRAIT_ID_MESH)] = json {
                {"geo_src", geometry_source_path},
                {"ix", node->mMeshes[i]},
                {"mat", uuids::to_string(matix_to_id[in_scene->mMeshes[node->mMeshes[i]]->mMaterialIndex])}
            };
            obj["c"].push_back(mesh_ch);
        }
    }
    return obj;
}

int main(int argc, char* argv[]) {
    char *input_path = nullptr, *output_path = nullptr, *scene_path = nullptr;
    bool write_scene = false;
    bool gen_hulls = false;

    for(int i = 0; i < argc; ++i) {
        if(strcmp(argv[i], "-i") == 0) {
            input_path = argv[++i];
        }
        else if(strcmp(argv[i], "-o") == 0) {
            output_path = argv[++i];
        }
        else if (strcmp(argv[i], "--gen-hull") == 0) {
            gen_hulls = true;
        }
        else if (strcmp(argv[i], "-scene") == 0) {
            write_scene = true;
            scene_path = argv[++i];
        }
    }

    if (input_path == nullptr || output_path == nullptr) {
        std::cout << "usage: eggv_import [options] -i [input scene path] -o [output mesh data path] (-scene [output scene json path])\n";
        std::cout << "\t--gen-hull:\tcompute convex hull of each mesh\n";
        return 1;
    }

    std::cout << input_path << " => " << output_path << "\n";

    Assimp::Importer imp;
    imp.ReadFile(input_path, aiProcess_GenBoundingBoxes | aiProcessPreset_TargetRealtime_MaxQuality | aiProcess_FlipUVs);
    std::cout << "\tloaded model\n";

	quickhull::QuickHull<float> hullgen;
    
    auto scene = imp.GetScene();
    std::ofstream output(output_path, std::ios::binary);
    output.write((char*)&scene->mNumMeshes, (std::streamsize)sizeof(int32_t));
    output.seekp(sizeof(geom_file::mesh_header) * scene->mNumMeshes, std::ios::cur);
    for (size_t mesh_ix = 0; mesh_ix < scene->mNumMeshes; ++mesh_ix) {
        auto mesh = scene->mMeshes[mesh_ix];
        std::cout << "\twriting mesh #" << mesh_ix << " \"" << mesh->mName.C_Str() << "\"\n";
        auto name_fptr = output.tellp();
        output.write(mesh->mName.C_Str(), mesh->mName.length+1);
        auto vert_fptr = output.tellp();
        auto has_tex_coords = mesh->HasTextureCoords(0);
        for (size_t i = 0; i < mesh->mNumVertices; ++i) {
            geom_file::vertex v(
                vec3(mesh->mVertices[i].x, mesh->mVertices[i].y, mesh->mVertices[i].z),
                vec3(mesh->mNormals[i].x, mesh->mNormals[i].y, mesh->mNormals[i].z),
                has_tex_coords ?
                    vec2(mesh->mTextureCoords[0][i].x, mesh->mTextureCoords[0][i].y)
					: vec2(0.f)
            );
            output.write((char*)&v, sizeof(geom_file::vertex));
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
        auto hull_fptr = 0;
        if (gen_hulls) {
            hull_fptr = output.tellp();
            auto hull = hullgen.getConvexHull((float*)mesh->mVertices, mesh->mNumVertices, false, 0.01f);
            auto indices = hull.getIndexBuffer();
            uint16_t size = indices.size();
			output.write((char*)&size, sizeof(uint16_t));
            for (size_t i = 0; i < indices.size(); ++i) {
                output.write((char*)(indices.data() + i), sizeof(uint16_t));
            }
        }
        auto resume_fptr = output.tellp();
        output.seekp(sizeof(int32) + mesh_ix*sizeof(geom_file::mesh_header), std::ios::beg);
        auto header = geom_file::mesh_header(
            vert_fptr,
            idx_fptr,
            name_fptr,
            hull_fptr,
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
        std::cout << "creating scene file at " << scene_path << "\n";
        auto out_scene = json {
            {"materials", json::object()},
            {"geometries", json::array({ output_path })},
        };

        std::vector<uuids::uuid> material_index_to_id(scene->mNumMaterials, uuids::uuid());
        for(auto i = 0; i < scene->mNumMaterials; ++i) {
            auto mat = scene->mMaterials[i];
            std::cout << "\tprocessing material: " << mat->GetName().C_Str() << "\n";
            auto mat_id = uuid_gen();
            material_index_to_id[i] = mat_id;
            aiColor3D base_color(0.5, 0.0, 0.5);
            if (mat->Get(AI_MATKEY_BASE_COLOR, base_color) == aiReturn_FAILURE)
                mat->Get(AI_MATKEY_COLOR_DIFFUSE, base_color);
            auto out_mat = json{
                {"name", mat->GetName().C_Str()},
                {"base", json::array({base_color.r, base_color.g, base_color.b})},
                {"textures", json::object()}
            };
            aiString tex_path;
            bool has_tex = mat->GetTexture(aiTextureType_DIFFUSE, 0, &tex_path) == aiReturn_SUCCESS;
            if(has_tex) {
                std::cout << "\t\tdiffuse texture @ " << tex_path.C_Str() << "\n";
                out_mat["textures"]["diffuse"] = tex_path.C_Str();
            }
            out_scene["materials"][uuids::to_string(mat_id)] = out_mat;
        }

        out_scene["graph"] = convert_scene_node(out_scene, scene->mRootNode, output_path, scene, material_index_to_id);

        if(scene->HasLights()) {
            for(auto i = 0; i < scene->mNumLights; ++i) {
                auto* light = scene->mLights[i];
                if(light->mType == aiLightSource_DIRECTIONAL || light->mType == aiLightSource_POINT) {
                    std::cout << "\tprocessing light: " << light->mName.C_Str() << "\n";
                    auto light_objp = std::find_if(
                            std::begin(out_scene["graph"]["c"]), std::end(out_scene["graph"]["c"]),
                            [&](const auto& obj) {
                                return obj["name"] == light->mName.C_Str();
                            }
                        );
                    if(light_objp == std::end(out_scene["graph"]["c"])) {
                        std::cout << "\t\tscene graph missing corresponding node\n";
                        continue;
                    }
                    auto& light_obj = *light_objp;
                    if(light->mType == aiLightSource_DIRECTIONAL) {
                        light_obj["t"][std::to_string(TRAIT_ID_LIGHT)] = json {
                            {"t", light_type::directional},
                            {"p", json::array({light->mDirection.x, light->mDirection.y, light->mDirection.z})},
                            {"c", json::array({light->mColorDiffuse.r, light->mColorDiffuse.g, light->mColorDiffuse.b})}
                        };
                    } else if(light->mType == aiLightSource_POINT) {
                        std::cout << "\t\t" << light->mAttenuationConstant << ", " << light->mAttenuationLinear <<
                            ", " << light->mAttenuationQuadratic << " " << light->mSize.x << "@" << light->mSize.y << "\n";
                        light_obj["t"][std::to_string(TRAIT_ID_LIGHT)] = json {
                            {"t", light_type::point},
                            {"p", json::array({light->mAttenuationQuadratic*1e5,0,0})},
                            {"c", json::array({light->mColorDiffuse.r, light->mColorDiffuse.g, light->mColorDiffuse.b})}
                        };
                        light_obj["t"][std::to_string(TRAIT_ID_TRANSFORM)]["s"] =
                            json::array({1,1,1});
                    }
                }
            }
        }

        if(scene->HasCameras()) {
            for(auto i = 0; i < scene->mNumCameras; ++i) {
                auto* cam = scene->mCameras[i];
                    std::cout << "\tprocessing camera: " << cam->mName.C_Str() << "\n";
                    auto cam_objp = std::find_if(
                            std::begin(out_scene["graph"]["c"]), std::end(out_scene["graph"]["c"]),
                            [&](const auto& obj) {
                                return obj["name"] == cam->mName.C_Str();
                            }
                        );
                    if(cam_objp == std::end(out_scene["graph"]["c"])) {
                        std::cout << "\t\tscene graph missing corresponding node\n";
                        continue;
                    }
                    auto& cam_obj = *cam_objp;
                    cam_obj["t"][std::to_string(TRAIT_ID_CAMERA)] = json {
                        {"fov", cam->mHorizontalFOV}
                    };
            }
        }

        std::ofstream outs(scene_path);
        outs << out_scene;
    }

    return 0;
}
