// C:\important\mimita-priv-v8\src\map\map_loader.cpp
//
// GLB loader notes:
// - Blender .glb files can store many meshes, materials, textures, and scene nodes.
// - A mesh primitive references vertex attributes by accessor name: POSITION, NORMAL,
//   TEXCOORD_0, etc.
// - Accessors may be tightly packed or strided through a BufferView. Always honor stride.
// - A material usually points at a texture index, which points at an image. In .glb files
//   that image is often embedded in the binary, so we upload model.images[] directly.
//
// This is intentionally not PBR. The loader only extracts enough data for the engine's
// stylized gameplay shader: position, normal, UV, and one base-color texture.

#include "map_common.h"
#include "map_loader.h"
#include "map-loader-mesh.h"
#include "map-loader-material.h"
#include "tiny_obj_loader.h"
#include "tinygltf/tiny_gltf.h"

#include <cstdio>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <algorithm>
#include <unordered_set>
#include <string>
#include <vector>

#include <glad/glad.h>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

#include "utils/path_utils.h"
#include "world/texture-store.h"
#include "debug/debug-log.h"
#include "debug/gl-debug.h"

extern TextureStore gTextures;

GLBDebugData gGLBDebug;

void GLBDebugData::clear()
{
    materials.clear();
    images.clear();
    lights.clear();
    meshCount = 0;
    totalPrimitives = 0;
    loaded = false;
}

namespace {

#ifndef GL_TEXTURE_MAX_ANISOTROPY_EXT
#define GL_TEXTURE_MAX_ANISOTROPY_EXT 0x84FE
#endif
#ifndef GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT
#define GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT 0x84FF
#endif

#define GLB_LOG(...) Debug::logAuto(Debug::Category::GLB, __VA_ARGS__)

}

Mesh loadOBJ(const std::string& path)
{
    std::string resolvedPath = resolveAssetPath(path);
    printf("OBJ path = %s\n", resolvedPath.c_str());

    tinyobj::ObjReader reader;
    if (!reader.ParseFromFile(resolvedPath.c_str()))
    {
        printf("OBJ error: %s\n", reader.Error().c_str());
        return Mesh{};
    }

    const auto& attrib = reader.GetAttrib();
    const auto& shapes = reader.GetShapes();

    Mesh mesh;
    for (const auto& shape : shapes)
    {
        for (auto idx : shape.mesh.indices)
        {
            Vertex v{};
            v.pos = {
                attrib.vertices[3 * idx.vertex_index + 0],
                attrib.vertices[3 * idx.vertex_index + 1],
                attrib.vertices[3 * idx.vertex_index + 2]
            };
            v.normal = {0, 0, 1};
            v.uv = {0, 0};
            mesh.verts.push_back(v);
        }
    }

    if (mesh.verts.size() >= 3)
    {
        for (size_t i = 0; i + 2 < mesh.verts.size(); i += 3)
        {
            glm::vec3 a = mesh.verts[i + 0].pos;
            glm::vec3 b = mesh.verts[i + 1].pos;
            glm::vec3 c = mesh.verts[i + 2].pos;
            glm::vec3 n = glm::cross(b - a, c - a);
            if (glm::length(n) < 0.0001f)
                n = {0, 0, 1};
            else
                n = glm::normalize(n);
            mesh.verts[i + 0].normal = n;
            mesh.verts[i + 1].normal = n;
            mesh.verts[i + 2].normal = n;
        }
    }

    Mesh::Batch batch;
    batch.materialName = "default";
    batch.texture = gTextures.get("default");
    batch.first = 0;
    batch.count = mesh.verts.size();
    mesh.batches.push_back(batch);

    printf("OBJ verts = %zu\n", mesh.verts.size());
    return mesh;
}

Mesh loadGLB(const std::string& path, bool /*storeDebugInfo*/, Mesh* skyMesh)
{
    std::string resolvedPath = resolveAssetPath(path);
    GLB_LOG("[GLB] path = %s\n", resolvedPath.c_str());

    tinygltf::TinyGLTF loader;
    tinygltf::Model model;
    std::string err;
    std::string warn;

    Debug::log(Debug::Category::GLB, "[GLB] before tinygltf LoadBinaryFromFile\n");
    bool ok = loader.LoadBinaryFromFile(&model, &err, &warn, resolvedPath);
    Debug::log(Debug::Category::GLB, "[GLB] after tinygltf LoadBinaryFromFile ok=%d\n", ok ? 1 : 0);

    if (!warn.empty()) GLB_LOG("[GLB WARNING] %s\n", warn.c_str());
    if (!err.empty()) GLB_LOG("[GLB ERROR] %s\n", err.c_str());
    if (!ok)
    {
        GLB_LOG("[GLB ERROR] Failed to load GLB\n");
        return Mesh{};
    }

    Debug::logOnce(Debug::Category::GLB, resolvedPath.c_str(), "[GLB] model meshes=%zu nodes=%zu materials=%zu textures=%zu images=%zu scenes=%zu buffers=%zu bufferViews=%zu accessors=%zu defaultScene=%d\n",
            model.meshes.size(), model.nodes.size(), model.materials.size(), model.textures.size(),
            model.images.size(), model.scenes.size(), model.buffers.size(), model.bufferViews.size(),
            model.accessors.size(), model.defaultScene);

    for (int meshIndex = 0; meshIndex < (int)model.meshes.size(); ++meshIndex)
    {
        const tinygltf::Mesh& gltfMesh = model.meshes[meshIndex];
        bool verbose = DebugConfig::GLB_VERBOSE || std::getenv("MIMITA_GLB_VERBOSE") != nullptr;
        if (verbose)
            GLB_LOG("[GLB] inspect mesh=%d name=%s primitives=%zu\n",
                    meshIndex, gltfMesh.name.c_str(), gltfMesh.primitives.size());
        for (int primIndex = 0; primIndex < (int)gltfMesh.primitives.size(); ++primIndex)
        {
            const tinygltf::Primitive& prim = gltfMesh.primitives[primIndex];
            if (verbose)
            {
                GLB_LOG("[GLB] inspect mesh=%d prim=%d mode=%d material=%d indices=%d attrCount=%zu\n",
                        meshIndex, primIndex, prim.mode, prim.material, prim.indices, prim.attributes.size());
                for (const auto& attr : prim.attributes)
                {
                    GLB_LOG("[GLB] inspect mesh=%d prim=%d attr=%s accessor=%d\n",
                            meshIndex, primIndex, attr.first.c_str(), attr.second);
                }
            }
        }
    }

    // Derive GLB directory for external texture resolution
    std::string glbDir;
    {
        size_t slashPos = resolvedPath.find_last_of("/\\");
        if (slashPos != std::string::npos)
            glbDir = resolvedPath.substr(0, slashPos);
    }

    std::vector<GLuint> imageTextures(model.images.size(), 0);
    std::vector<GLuint> colorTextures;
    std::vector<GLuint> materialTextures(model.materials.size(), 0);

    processGLBMaterials(model, glbDir, imageTextures, materialTextures, colorTextures);

    Mesh mesh;
    for (GLuint texture : imageTextures)
    {
        if (texture)
            mesh.ownedTextures.push_back(texture);
    }
    for (GLuint texture : colorTextures)
    {
        if (texture)
            mesh.ownedTextures.push_back(texture);
    }

    int sceneIndex = model.defaultScene >= 0 ? model.defaultScene : 0;
    walkGLBScene(model, materialTextures, mesh, sceneIndex, skyMesh);

    std::vector<Mesh::Batch> merged;
    for (const Mesh::Batch& batch : mesh.batches)
    {
        if (!merged.empty() &&
            merged.back().texture == batch.texture &&
            merged.back().first + merged.back().count == batch.first)
        {
            merged.back().count += batch.count;
        }
        else
        {
            merged.push_back(batch);
        }
    }
    if (merged.size() != mesh.batches.size())
        GLB_LOG("[GLB] merged material batches %zu -> %zu to reduce texture binds\n", mesh.batches.size(), merged.size());
    mesh.batches = merged;

    // Store GLB debug data
    {
        gGLBDebug.clear();
        gGLBDebug.meshCount = (int)model.meshes.size();
        gGLBDebug.loaded = true;

        for (size_t i = 0; i < model.materials.size(); ++i)
        {
            const tinygltf::Material& mat = model.materials[i];
            const auto& pbr = mat.pbrMetallicRoughness;
            GLBMaterialInfo mi;
            mi.index = (int)i;
            mi.name = mat.name;
            mi.baseColorTextureIndex = pbr.baseColorTexture.index;
            mi.hasTexture = mi.baseColorTextureIndex >= 0;
            mi.hasColorFactor = pbr.baseColorFactor.size() >= 3;
            for (int k = 0; k < 4 && k < (int)pbr.baseColorFactor.size(); ++k)
                mi.baseColorFactor[k] = pbr.baseColorFactor[k];
            for (int k = pbr.baseColorFactor.size(); k < 4; ++k)
                mi.baseColorFactor[k] = (k == 3) ? 1.0 : 0.0;

            mi.hasKhrTextureTransform = false;
            auto extIt = pbr.baseColorTexture.extensions.find("KHR_texture_transform");
            if (extIt != pbr.baseColorTexture.extensions.end())
            {
                mi.hasKhrTextureTransform = true;
                const tinygltf::Value& extVal = extIt->second;
                if (extVal.IsObject())
                {
                    tinygltf::Value::Object extObj = extVal.Get<tinygltf::Value::Object>();
                    auto offIt = extObj.find("offset");
                    if (offIt != extObj.end() && offIt->second.IsArray())
                    {
                        tinygltf::Value::Array arr = offIt->second.Get<tinygltf::Value::Array>();
                        if (arr.size() >= 2)
                        {
                            mi.texTransformOffset[0] = arr[0].GetNumberAsDouble();
                            mi.texTransformOffset[1] = arr[1].GetNumberAsDouble();
                        }
                    }
                    auto scaleIt = extObj.find("scale");
                    if (scaleIt != extObj.end() && scaleIt->second.IsArray())
                    {
                        tinygltf::Value::Array arr = scaleIt->second.Get<tinygltf::Value::Array>();
                        if (arr.size() >= 2)
                        {
                            mi.texTransformScale[0] = arr[0].GetNumberAsDouble();
                            mi.texTransformScale[1] = arr[1].GetNumberAsDouble();
                        }
                    }
                }
            }
            gGLBDebug.materials.push_back(mi);
        }

        for (size_t i = 0; i < model.images.size(); ++i)
        {
            const tinygltf::Image& img = model.images[i];
            GLBImageInfo ii;
            ii.index = (int)i;
            ii.name = img.name;
            ii.width = img.width;
            ii.height = img.height;
            ii.components = img.component;
            ii.embedded = img.bufferView >= 0;
            ii.uri = img.uri;
            gGLBDebug.images.push_back(ii);
        }

        // Import KHR_lights_punctual if present
        {
            auto lightsExtIt = model.extensions.find("KHR_lights_punctual");
            if (lightsExtIt != model.extensions.end())
            {
                const tinygltf::Value& lightsExt = lightsExtIt->second;
                if (lightsExt.IsObject())
                {
                    tinygltf::Value::Object lightsData = lightsExt.Get<tinygltf::Value::Object>();
                    auto lightsArrayIt = lightsData.find("lights");
                    if (lightsArrayIt != lightsData.end() && lightsArrayIt->second.IsArray())
                    {
                        tinygltf::Value::Array lightsArr = lightsArrayIt->second.Get<tinygltf::Value::Array>();
                        for (size_t li = 0; li < lightsArr.size(); ++li)
                        {
                            const tinygltf::Value& lv = lightsArr[li];
                            if (!lv.IsObject()) continue;
                            tinygltf::Value::Object l = lv.Get<tinygltf::Value::Object>();
                            GLBLightInfo info;
                            auto nameIt = l.find("name");
                            info.name = (nameIt != l.end() && nameIt->second.IsString())
                                ? nameIt->second.Get<std::string>() : ("light_" + std::to_string(li));
                            auto typeIt = l.find("type");
                            info.type = (typeIt != l.end() && typeIt->second.IsString())
                                ? typeIt->second.Get<std::string>() : "point";
                            auto colorIt = l.find("color");
                            if (colorIt != l.end() && colorIt->second.IsArray())
                            {
                                tinygltf::Value::Array cArr = colorIt->second.Get<tinygltf::Value::Array>();
                                for (int k = 0; k < 3 && k < (int)cArr.size(); ++k)
                                    info.color[k] = cArr[k].GetNumberAsDouble();
                            }
                            auto intensityIt = l.find("intensity");
                            info.intensity = (intensityIt != l.end() && intensityIt->second.IsNumber())
                                ? intensityIt->second.Get<double>() : 1.0;
                            auto rangeIt = l.find("range");
                            info.range = (rangeIt != l.end() && rangeIt->second.IsNumber())
                                ? rangeIt->second.Get<double>() : 0.0;
                            info.position[0] = info.position[1] = info.position[2] = 0.0;
                            info.direction[0] = info.direction[1] = info.direction[2] = 0.0;
                            info.innerConeAngle = 0.0;
                            info.outerConeAngle = 0.0;
                            if (info.type == "spot")
                            {
                                auto innerIt = l.find("innerConeAngle");
                                info.innerConeAngle = (innerIt != l.end() && innerIt->second.IsNumber())
                                    ? innerIt->second.Get<double>() : 0.0;
                                auto outerIt = l.find("outerConeAngle");
                                info.outerConeAngle = (outerIt != l.end() && outerIt->second.IsNumber())
                                    ? outerIt->second.Get<double>() : 0.7853981633974483;
                            }
                            gGLBDebug.lights.push_back(info);
                            printf("[GLB LIGHT] imported light=%zu name=%s type=%s intensity=%.2f range=%.2f\n",
                                   li, info.name.c_str(), info.type.c_str(), info.intensity, info.range);
                        }
                    }
                }
            }
        }

        // For each node, check if it references a light (KHR_lights_punctual node extension)
        for (size_t ni = 0; ni < model.nodes.size(); ++ni)
        {
            const tinygltf::Node& node = model.nodes[ni];
            auto nodeLightIt = node.extensions.find("KHR_lights_punctual");
            if (nodeLightIt != node.extensions.end())
            {
                const tinygltf::Value& nodeLightVal = nodeLightIt->second;
                if (nodeLightVal.IsObject())
                {
                    tinygltf::Value::Object nodeLightData = nodeLightVal.Get<tinygltf::Value::Object>();
                    auto lightIdxIt = nodeLightData.find("light");
                    if (lightIdxIt != nodeLightData.end() && lightIdxIt->second.IsNumber())
                    {
                        int lightIdx = lightIdxIt->second.Get<int>();
                        if (lightIdx >= 0 && lightIdx < (int)gGLBDebug.lights.size())
                        {
                            glm::mat4 nodeXform(1.0f);
                            {
                                glm::mat4 out(1.0f);
                                if (node.matrix.size() == 16)
                                {
                                    for (int col = 0; col < 4; ++col)
                                        for (int row = 0; row < 4; ++row)
                                            out[col][row] = (float)node.matrix[col * 4 + row];
                                }
                                else
                                {
                                    glm::vec3 t(0.0f);
                                    if (node.translation.size() == 3)
                                        t = {(float)node.translation[0], (float)node.translation[1], (float)node.translation[2]};
                                    glm::quat r(1, 0, 0, 0);
                                    if (node.rotation.size() == 4)
                                        r = glm::quat((float)node.rotation[3], (float)node.rotation[0], (float)node.rotation[1], (float)node.rotation[2]);
                                    glm::vec3 s(1.0f);
                                    if (node.scale.size() == 3)
                                        s = {(float)node.scale[0], (float)node.scale[1], (float)node.scale[2]};
                                    out = glm::translate(glm::mat4(1.0f), t) * glm::mat4_cast(r) * glm::scale(glm::mat4(1.0f), s);
                                }
                                nodeXform = out;
                            }
                            auto& lightInfo = gGLBDebug.lights[lightIdx];
                            lightInfo.position[0] = nodeXform[3][0];
                            lightInfo.position[1] = nodeXform[3][1];
                            lightInfo.position[2] = nodeXform[3][2];
                            glm::vec3 dir = nodeXform * glm::vec4(0, 0, -1, 0);
                            lightInfo.direction[0] = dir.x;
                            lightInfo.direction[1] = dir.y;
                            lightInfo.direction[2] = dir.z;
                            printf("[GLB LIGHT] node=%zu name=%s lightIdx=%d pos=(%.2f,%.2f,%.2f)\n",
                                   ni, node.name.c_str(), lightIdx,
                                   lightInfo.position[0], lightInfo.position[1], lightInfo.position[2]);
                        }
                    }
                }
            }
        }
    }

    GLB_LOG("[GLB] verts=%zu triangles=%zu batches=%zu\n", mesh.verts.size(), mesh.verts.size() / 3, mesh.batches.size());
    return mesh;
}

void releaseMeshGLResources(Mesh& mesh)
{
    if (!mesh.ownedTextures.empty())
    {
        glDeleteTextures(
            (GLsizei)mesh.ownedTextures.size(),
            mesh.ownedTextures.data());
        GLB_LOG("[GLB] released owned textures=%zu\n", mesh.ownedTextures.size());
    }
    mesh.ownedTextures.clear();
}
