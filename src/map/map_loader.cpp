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
#include "tiny_obj_loader.h"
#include "tinygltf/tiny_gltf.h"

#include <cstdio>
#include <string>
#include <vector>

#include <glad/glad.h>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

#include "utils/path_utils.h"
#include "world/texture-store.h"

extern TextureStore gTextures;

namespace {

GLuint uploadGLBImage(const tinygltf::Image& image, int imageIndex)
{
    if (image.image.empty() || image.width <= 0 || image.height <= 0)
    {
        printf("[GLB TEXTURE WARNING] image %d is empty; using assets/textures/default.png\n", imageIndex);
        return gTextures.get("default");
    }

    // Mipmaps are smaller prefiltered versions of the same texture.
    // Without mipmaps, distant surfaces shimmer because a single screen pixel samples
    // a huge number of source texels. Mipmaps trade detail for stable movement clarity.
    // Trilinear filtering blends between mip levels so movement does not pop.
    // Anisotropic filtering helps surfaces viewed at glancing angles, like floors.
    GLuint tex = 0;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);

    GLenum srcFormat = GL_RGBA;
    if (image.component == 1) srcFormat = GL_RED;
    else if (image.component == 3) srcFormat = GL_RGB;
    else srcFormat = GL_RGBA;

    printf("[GLB] loaded texture image=%d name=%s size=%dx%d components=%d tex=%u\n",
           imageIndex, image.name.c_str(), image.width, image.height, image.component, tex);
    printf("[TEXTURE] Uploading GLB image to GPU and generating mipmaps\n");

    glTexImage2D(
        GL_TEXTURE_2D,
        0,
        GL_RGBA,
        image.width,
        image.height,
        0,
        srcFormat,
        GL_UNSIGNED_BYTE,
        image.image.data()
    );

    // REPEAT lets UVs outside 0..1 tile. This is useful for blockout/material-style maps.
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

    // Minification = texture is smaller on screen than in memory. Use mipmaps.
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);

    // Magnification = texture is larger on screen than in memory. NEAREST preserves a
    // crisp PS2-ish/pixel edge; switch to GL_LINEAR for smoother texture enlargement.
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    glGenerateMipmap(GL_TEXTURE_2D);
    printf("[TEXTURE] mipmaps generated for GLB texture tex=%u\n", tex);

    GLfloat maxAniso = 1.0f;
    glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY, &maxAniso);
    if (maxAniso > 1.0f)
    {
        GLfloat useAniso = maxAniso < 4.0f ? maxAniso : 4.0f;
        glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY, useAniso);
        printf("[TEXTURE] anisotropic filtering %.1fx applied to GLB texture\n", useAniso);
    }

    return tex;
}

const unsigned char* accessorPtr(const tinygltf::Model& model, const tinygltf::Accessor& accessor)
{
    const tinygltf::BufferView& view = model.bufferViews[accessor.bufferView];
    const tinygltf::Buffer& buffer = model.buffers[view.buffer];
    return buffer.data.data() + view.byteOffset + accessor.byteOffset;
}

size_t accessorStride(const tinygltf::Model& model, const tinygltf::Accessor& accessor)
{
    const tinygltf::BufferView& view = model.bufferViews[accessor.bufferView];
    size_t stride = accessor.ByteStride(view);
    if (stride != 0)
        return stride;
    return (size_t)tinygltf::GetComponentSizeInBytes(accessor.componentType) *
           (size_t)tinygltf::GetNumComponentsInType(accessor.type);
}

glm::vec3 readVec3(const tinygltf::Model& model, const tinygltf::Accessor& accessor, size_t index, glm::vec3 fallback)
{
    if (accessor.componentType != TINYGLTF_COMPONENT_TYPE_FLOAT || accessor.type != TINYGLTF_TYPE_VEC3)
        return fallback;
    const unsigned char* base = accessorPtr(model, accessor);
    const float* f = reinterpret_cast<const float*>(base + index * accessorStride(model, accessor));
    return {f[0], f[1], f[2]};
}

glm::vec2 readVec2(const tinygltf::Model& model, const tinygltf::Accessor& accessor, size_t index, glm::vec2 fallback)
{
    if (accessor.componentType != TINYGLTF_COMPONENT_TYPE_FLOAT || accessor.type != TINYGLTF_TYPE_VEC2)
        return fallback;
    const unsigned char* base = accessorPtr(model, accessor);
    const float* f = reinterpret_cast<const float*>(base + index * accessorStride(model, accessor));
    return {f[0], f[1]};
}

unsigned int readIndex(const tinygltf::Model& model, const tinygltf::Accessor& accessor, size_t i)
{
    const unsigned char* base = accessorPtr(model, accessor);
    const unsigned char* p = base + i * accessorStride(model, accessor);
    switch (accessor.componentType)
    {
        case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE:
            return *reinterpret_cast<const unsigned char*>(p);
        case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT:
            return *reinterpret_cast<const unsigned short*>(p);
        case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT:
            return *reinterpret_cast<const unsigned int*>(p);
        default:
            printf("[GLB WARNING] unsupported index component type %d; using 0\n", accessor.componentType);
            return 0;
    }
}

glm::mat4 nodeMatrix(const tinygltf::Node& node)
{
    if (node.matrix.size() == 16)
    {
        glm::mat4 m(1.0f);
        for (int col = 0; col < 4; ++col)
            for (int row = 0; row < 4; ++row)
                m[col][row] = (float)node.matrix[col * 4 + row];
        return m;
    }

    glm::vec3 t(0.0f);
    if (node.translation.size() == 3)
        t = {(float)node.translation[0], (float)node.translation[1], (float)node.translation[2]};

    glm::quat r(1, 0, 0, 0);
    if (node.rotation.size() == 4)
        r = glm::quat((float)node.rotation[3], (float)node.rotation[0], (float)node.rotation[1], (float)node.rotation[2]);

    glm::vec3 s(1.0f);
    if (node.scale.size() == 3)
        s = {(float)node.scale[0], (float)node.scale[1], (float)node.scale[2]};

    return glm::translate(glm::mat4(1.0f), t) * glm::mat4_cast(r) * glm::scale(glm::mat4(1.0f), s);
}

void generateTriangleNormals(std::vector<Vertex>& verts, size_t first, size_t count)
{
    for (size_t i = first; i + 2 < first + count; i += 3)
    {
        glm::vec3 a = verts[i + 0].pos;
        glm::vec3 b = verts[i + 1].pos;
        glm::vec3 c = verts[i + 2].pos;
        glm::vec3 n = glm::cross(b - a, c - a);
        if (glm::length(n) < 0.0001f)
            n = {0, 0, 1};
        else
            n = glm::normalize(n);
        verts[i + 0].normal = n;
        verts[i + 1].normal = n;
        verts[i + 2].normal = n;
    }
}

void appendPrimitive(
    const tinygltf::Model& model,
    const tinygltf::Primitive& primitive,
    const glm::mat4& transform,
    const std::vector<GLuint>& materialTextures,
    Mesh& mesh
) {
    auto posIt = primitive.attributes.find("POSITION");
    if (posIt == primitive.attributes.end())
    {
        printf("[GLB WARNING] primitive skipped: missing POSITION\n");
        return;
    }

    const tinygltf::Accessor& posAccessor = model.accessors[posIt->second];
    const tinygltf::Accessor* normalAccessor = nullptr;
    const tinygltf::Accessor* uvAccessor = nullptr;

    auto nIt = primitive.attributes.find("NORMAL");
    if (nIt != primitive.attributes.end())
        normalAccessor = &model.accessors[nIt->second];
    else
        printf("[GLB WARNING] primitive missing NORMAL; generating fallback normals\n");

    auto uvIt = primitive.attributes.find("TEXCOORD_0");
    if (uvIt != primitive.attributes.end())
        uvAccessor = &model.accessors[uvIt->second];
    else
        printf("[GLB WARNING] primitive missing TEXCOORD_0; using uv=(0,0)\n");

    Mesh::Batch batch;
    batch.materialIndex = primitive.material;
    batch.materialName = primitive.material >= 0 && primitive.material < (int)model.materials.size()
        ? model.materials[primitive.material].name
        : "default";
    static GLuint defaultTexture = 0;
    if (!defaultTexture)
        defaultTexture = gTextures.get("default");
    batch.texture = defaultTexture;
    if (primitive.material >= 0 && primitive.material < (int)materialTextures.size() && materialTextures[primitive.material])
        batch.texture = materialTextures[primitive.material];
    batch.first = mesh.verts.size();

    glm::mat3 normalXform = glm::transpose(glm::inverse(glm::mat3(transform)));

    auto pushVertex = [&](unsigned int vi) {
        Vertex v{};
        glm::vec3 pos = readVec3(model, posAccessor, vi, {0,0,0});
        v.pos = glm::vec3(transform * glm::vec4(pos, 1.0f));

        if (normalAccessor)
        {
            glm::vec3 n = readVec3(model, *normalAccessor, vi, {0,0,1});
            v.normal = glm::normalize(normalXform * n);
        }
        else
        {
            v.normal = {0,0,0};
        }

        if (uvAccessor)
            v.uv = readVec2(model, *uvAccessor, vi, {0,0});
        else
            v.uv = {0,0};

        mesh.verts.push_back(v);
    };

    if (primitive.indices >= 0)
    {
        const tinygltf::Accessor& indexAccessor = model.accessors[primitive.indices];
        for (size_t i = 0; i < indexAccessor.count; ++i)
            pushVertex(readIndex(model, indexAccessor, i));
    }
    else
    {
        for (size_t i = 0; i < posAccessor.count; ++i)
            pushVertex((unsigned int)i);
    }

    batch.count = mesh.verts.size() - batch.first;
    if (!normalAccessor)
        generateTriangleNormals(mesh.verts, batch.first, batch.count);

    mesh.batches.push_back(batch);
    static int loggedPrimitives = 0;
    if (loggedPrimitives < 20 || (loggedPrimitives % 100) == 0)
        printf("[GLB] loaded mesh primitive material=%s verts=%zu triangles=%zu texture=%u\n",
               batch.materialName.c_str(), batch.count, batch.count / 3, batch.texture);
    loggedPrimitives++;
}

void walkNode(
    const tinygltf::Model& model,
    int nodeIndex,
    const glm::mat4& parent,
    const std::vector<GLuint>& materialTextures,
    Mesh& mesh
) {
    if (nodeIndex < 0 || nodeIndex >= (int)model.nodes.size())
        return;

    const tinygltf::Node& node = model.nodes[nodeIndex];
    glm::mat4 world = parent * nodeMatrix(node);

    static int loggedNodes = 0;
    if (node.mesh >= 0 && node.mesh < (int)model.meshes.size())
    {
        const tinygltf::Mesh& gltfMesh = model.meshes[node.mesh];
        if (loggedNodes < 20 || (loggedNodes % 100) == 0)
            printf("[GLB] node mesh=%s primitives=%zu\n", gltfMesh.name.c_str(), gltfMesh.primitives.size());
        loggedNodes++;
        for (const tinygltf::Primitive& primitive : gltfMesh.primitives)
            appendPrimitive(model, primitive, world, materialTextures, mesh);
    }

    for (int child : node.children)
        walkNode(model, child, world, materialTextures, mesh);
}

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

    generateTriangleNormals(mesh.verts, 0, mesh.verts.size());
    Mesh::Batch batch;
    batch.materialName = "default";
    batch.texture = gTextures.get("default");
    batch.first = 0;
    batch.count = mesh.verts.size();
    mesh.batches.push_back(batch);

    printf("OBJ verts = %zu\n", mesh.verts.size());
    return mesh;
}

Mesh loadGLB(const std::string& path)
{
    std::string resolvedPath = resolveAssetPath(path);
    printf("[GLB] path = %s\n", resolvedPath.c_str());

    tinygltf::TinyGLTF loader;
    tinygltf::Model model;
    std::string err;
    std::string warn;

    bool ok = loader.LoadBinaryFromFile(&model, &err, &warn, resolvedPath);

    if (!warn.empty()) printf("[GLB WARNING] %s\n", warn.c_str());
    if (!err.empty()) printf("[GLB ERROR] %s\n", err.c_str());
    if (!ok)
    {
        printf("[GLB ERROR] Failed to load GLB\n");
        return Mesh{};
    }

    printf("[GLB] model meshes=%zu materials=%zu textures=%zu images=%zu scenes=%zu\n",
           model.meshes.size(), model.materials.size(), model.textures.size(), model.images.size(), model.scenes.size());

    std::vector<GLuint> imageTextures(model.images.size(), 0);
    for (size_t i = 0; i < model.images.size(); ++i)
        imageTextures[i] = uploadGLBImage(model.images[i], (int)i);

    std::vector<GLuint> materialTextures(model.materials.size(), 0);
    for (size_t i = 0; i < model.materials.size(); ++i)
    {
        const tinygltf::Material& mat = model.materials[i];
        int texIndex = mat.pbrMetallicRoughness.baseColorTexture.index;
        if (texIndex >= 0 && texIndex < (int)model.textures.size())
        {
            int imageIndex = model.textures[texIndex].source;
            if (imageIndex >= 0 && imageIndex < (int)imageTextures.size())
                materialTextures[i] = imageTextures[imageIndex];
        }
        if (!materialTextures[i])
        {
            printf("[GLB TEXTURE WARNING] material %zu (%s) has no baseColor texture; using default.png\n", i, mat.name.c_str());
            materialTextures[i] = gTextures.get("default");
        }
        printf("[GLB] material %zu name=%s texture=%u\n", i, mat.name.c_str(), materialTextures[i]);
    }

    Mesh mesh;
    int sceneIndex = model.defaultScene >= 0 ? model.defaultScene : 0;
    if (sceneIndex >= 0 && sceneIndex < (int)model.scenes.size())
    {
        const tinygltf::Scene& scene = model.scenes[sceneIndex];
        printf("[GLB] walking scene=%d nodes=%zu\n", sceneIndex, scene.nodes.size());
        for (int node : scene.nodes)
            walkNode(model, node, glm::mat4(1.0f), materialTextures, mesh);
    }
    else
    {
        printf("[GLB WARNING] no valid scene; loading meshes without node transforms\n");
        for (const tinygltf::Mesh& gltfMesh : model.meshes)
            for (const tinygltf::Primitive& primitive : gltfMesh.primitives)
                appendPrimitive(model, primitive, glm::mat4(1.0f), materialTextures, mesh);
    }

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
        printf("[GLB] merged material batches %zu -> %zu to reduce texture binds\n", mesh.batches.size(), merged.size());
    mesh.batches = merged;

    printf("[GLB] verts=%zu triangles=%zu batches=%zu\n", mesh.verts.size(), mesh.verts.size() / 3, mesh.batches.size());
    return mesh;
}
