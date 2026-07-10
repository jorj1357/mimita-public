#include "map_common.h"
#include "map_loader.h"
#include "map-loader-mesh.h"
#include "map-loader-gltf-accessors.h"
#include "tinygltf/tiny_gltf.h"

#include <cstdio>
#include <cmath>
#include <cstdint>
#include <algorithm>
#include <string>
#include <vector>

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

#include "world/texture-store.h"
#include "debug/debug-log.h"

extern TextureStore gTextures;

#define GLB_LOG(...) Debug::logAuto(Debug::Category::GLB, __VA_ARGS__)

void appendPrimitive(
    const tinygltf::Model& model,
    int meshIndex,
    int primitiveIndex,
    const tinygltf::Primitive& primitive,
    const glm::mat4& transform,
    const std::vector<GLuint>& materialTextures,
    Mesh& mesh
) {
    if (shouldLogPrimitiveDetails(meshIndex, primitiveIndex))
        GLB_LOG("[GLB] mesh=%d prim=%d begin mode=%d material=%d indices=%d attributes=%zu\n",
                meshIndex, primitiveIndex, primitive.mode, primitive.material, primitive.indices, primitive.attributes.size());

    if (primitive.mode != TINYGLTF_MODE_TRIANGLES)
    {
        GLB_LOG("[GLB WARNING] mesh=%d prim=%d skipping non-triangle primitive mode=%d\n",
                meshIndex, primitiveIndex, primitive.mode);
        return;
    }

    auto posIt = primitive.attributes.find("POSITION");
    if (posIt == primitive.attributes.end())
    {
        GLB_LOG("[GLB WARNING] mesh=%d prim=%d skipped: missing POSITION\n", meshIndex, primitiveIndex);
        return;
    }

    const tinygltf::Accessor* posAccessor = nullptr;
    if (!validateAccessor(model, posIt->second, "POSITION", meshIndex, primitiveIndex,
                          TINYGLTF_TYPE_VEC3, TINYGLTF_COMPONENT_TYPE_FLOAT, &posAccessor))
    {
        GLB_LOG("[GLB WARNING] mesh=%d prim=%d skipped: invalid POSITION accessor\n", meshIndex, primitiveIndex);
        return;
    }

    const tinygltf::Accessor* normalAccessor = nullptr;
    const tinygltf::Accessor* uvAccessor = nullptr;

    auto nIt = primitive.attributes.find("NORMAL");
    if (nIt != primitive.attributes.end())
    {
        if (!validateAccessor(model, nIt->second, "NORMAL", meshIndex, primitiveIndex,
                              TINYGLTF_TYPE_VEC3, TINYGLTF_COMPONENT_TYPE_FLOAT, &normalAccessor))
        {
            GLB_LOG("[GLB WARNING] mesh=%d prim=%d invalid NORMAL; generating fallback normals\n",
                    meshIndex, primitiveIndex);
            normalAccessor = nullptr;
        }
    }
    else
        GLB_LOG("[GLB WARNING] mesh=%d prim=%d missing NORMAL; generating fallback normals\n", meshIndex, primitiveIndex);

    auto uvIt = primitive.attributes.find("TEXCOORD_0");
    if (uvIt != primitive.attributes.end())
    {
        if (!validateAccessor(model, uvIt->second, "TEXCOORD_0", meshIndex, primitiveIndex,
                              TINYGLTF_TYPE_VEC2, TINYGLTF_COMPONENT_TYPE_FLOAT, &uvAccessor))
        {
            printf("[GLB UV WARNING] mesh=%d prim=%d invalid TEXCOORD_0; using uv=(0,0)\n",
                    meshIndex, primitiveIndex);
            uvAccessor = nullptr;
        }
        else if (uvAccessor && uvAccessor->count > 0)
        {
            glm::vec2 uv0 = readVec2(model, *uvAccessor, 0, {0,0});
            glm::vec2 uv1 = readVec2(model, *uvAccessor, std::min((size_t)1, uvAccessor->count - 1), {0,0});
            printf("[GLB UV] mesh=%d prim=%d hasTexcoord0=yes uvCount=%zu vertexCount=%zu uv0=(%.4f,%.4f) uv1=(%.4f,%.4f)\n",
                   meshIndex, primitiveIndex, uvAccessor->count, posAccessor->count,
                   uv0.x, uv0.y, uv1.x, uv1.y);
        }
    }
    else
        printf("[GLB UV] mesh=%d prim=%d hasTexcoord0=no; using uv=(0,0)\n", meshIndex, primitiveIndex);

    const tinygltf::Accessor* indexAccessor = nullptr;
    if (primitive.indices >= 0)
    {
        if (!validateAccessor(model, primitive.indices, "INDICES", meshIndex, primitiveIndex,
                              TINYGLTF_TYPE_SCALAR, 0, &indexAccessor))
        {
            GLB_LOG("[GLB WARNING] mesh=%d prim=%d skipped: invalid indices accessor\n", meshIndex, primitiveIndex);
            return;
        }

        if (indexAccessor->componentType != TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE &&
            indexAccessor->componentType != TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT &&
            indexAccessor->componentType != TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT)
        {
            GLB_LOG("[GLB WARNING] mesh=%d prim=%d skipped: unsupported index component=%s\n",
                    meshIndex, primitiveIndex, componentTypeName(indexAccessor->componentType));
            return;
        }
    }

    if (primitive.material >= (int)model.materials.size())
        GLB_LOG("[GLB WARNING] mesh=%d prim=%d material index out of range material=%d materials=%zu; using default\n",
                meshIndex, primitiveIndex, primitive.material, model.materials.size());

    Mesh::Batch batch;
    batch.materialIndex = primitive.material;
    batch.materialName = primitive.material >= 0 && primitive.material < (int)model.materials.size()
        ? model.materials[primitive.material].name
        : "default";
    batch.doubleSided = primitive.material >= 0 && primitive.material < (int)model.materials.size()
        ? model.materials[primitive.material].doubleSided
        : false;
    static GLuint defaultTexture = 0;
    if (!defaultTexture)
        defaultTexture = gTextures.get("default");
    batch.texture = defaultTexture;
    if (primitive.material >= 0 && primitive.material < (int)materialTextures.size() && materialTextures[primitive.material])
        batch.texture = materialTextures[primitive.material];
    batch.first = mesh.verts.size();

    glm::mat3 normalXform(1.0f);
    float det = glm::determinant(glm::mat3(transform));
    if (std::fabs(det) > 0.000001f && std::isfinite(det))
        normalXform = glm::transpose(glm::inverse(glm::mat3(transform)));
    else
        GLB_LOG("[GLB WARNING] mesh=%d prim=%d non-invertible transform for normals; using identity normal transform\n",
                meshIndex, primitiveIndex);

    auto pushVertex = [&](unsigned int vi) {
        if (vi >= posAccessor->count)
        {
            GLB_LOG("[GLB WARNING] mesh=%d prim=%d vertex index out of POSITION range vi=%u count=%zu\n",
                    meshIndex, primitiveIndex, vi, posAccessor->count);
            return false;
        }

        Vertex v{};
        glm::vec3 pos = readVec3(model, *posAccessor, vi, {0,0,0});
        if (!validVec3(pos))
        {
            GLB_LOG("[GLB WARNING] mesh=%d prim=%d POSITION NaN/INF vi=%u; vertex skipped\n",
                    meshIndex, primitiveIndex, vi);
            return false;
        }

        v.pos = glm::vec3(transform * glm::vec4(pos, 1.0f));
        if (!validVec3(v.pos))
        {
            GLB_LOG("[GLB WARNING] mesh=%d prim=%d transformed POSITION NaN/INF vi=%u; vertex skipped\n",
                    meshIndex, primitiveIndex, vi);
            return false;
        }

        if (normalAccessor && vi < normalAccessor->count)
        {
            glm::vec3 n = readVec3(model, *normalAccessor, vi, {0,0,1});
            glm::vec3 transformedN = normalXform * n;
            if (glm::length(transformedN) > 0.000001f && validVec3(transformedN))
                v.normal = glm::normalize(transformedN);
            else
                v.normal = {0,0,1};
        }
        else
        {
            v.normal = {0,0,0};
        }

        if (uvAccessor && vi < uvAccessor->count)
            v.uv = readVec2(model, *uvAccessor, vi, {0,0});
        else
            v.uv = {0,0};

        mesh.verts.push_back(v);
        return true;
    };

    if (primitive.indices >= 0)
    {
        if (shouldLogPrimitiveDetails(meshIndex, primitiveIndex))
            GLB_LOG("[GLB] mesh=%d prim=%d extracting indexed vertices indexCount=%zu positionCount=%zu\n",
                    meshIndex, primitiveIndex, indexAccessor->count, posAccessor->count);
        for (size_t i = 0; i < indexAccessor->count; ++i)
        {
            unsigned int vi = 0;
            if (!readIndex(model, *indexAccessor, i, vi))
            {
                GLB_LOG("[GLB WARNING] mesh=%d prim=%d failed reading index i=%zu; skipped\n",
                        meshIndex, primitiveIndex, i);
                continue;
            }
            pushVertex(vi);
        }
    }
    else
    {
        if (shouldLogPrimitiveDetails(meshIndex, primitiveIndex))
            GLB_LOG("[GLB] mesh=%d prim=%d extracting non-indexed vertices positionCount=%zu\n",
                    meshIndex, primitiveIndex, posAccessor->count);
        for (size_t i = 0; i < posAccessor->count; ++i)
            pushVertex((unsigned int)i);
    }

    batch.count = mesh.verts.size() - batch.first;
    if (batch.count < 3)
    {
        GLB_LOG("[GLB WARNING] mesh=%d prim=%d produced fewer than 3 vertices; skipped batch\n",
                meshIndex, primitiveIndex);
        mesh.verts.resize(batch.first);
        return;
    }

    size_t remainder = batch.count % 3;
    if (remainder != 0)
    {
        GLB_LOG("[GLB WARNING] mesh=%d prim=%d vertex count not multiple of 3 count=%zu; truncating remainder=%zu\n",
                meshIndex, primitiveIndex, batch.count, remainder);
        mesh.verts.resize(mesh.verts.size() - remainder);
        batch.count -= remainder;
    }

    if (!normalAccessor)
        generateTriangleNormals(mesh.verts, batch.first, batch.count);

    mesh.batches.push_back(batch);
    static int loggedPrimitives = 0;
    if (loggedPrimitives < 20 || (loggedPrimitives % 100) == 0)
        GLB_LOG("[GLB] loaded mesh=%d prim=%d material=%s verts=%zu triangles=%zu texture=%u\n",
                meshIndex, primitiveIndex, batch.materialName.c_str(), batch.count, batch.count / 3, batch.texture);
    loggedPrimitives++;
}
