#include "glb-loader.h"
#define TINYGLTF_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <tinygltf/tiny_gltf.h>
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <cstdio>
#include <string>
#include <vector>

struct TransformEntry {
    int nodeIdx;
    glm::mat4 transform;
};

static const unsigned char* accessorPtr(const tinygltf::Model& model, const tinygltf::Accessor& acc) {
    if (acc.bufferView < 0 || acc.bufferView >= (int)model.bufferViews.size()) {
        printf("[GLB] ERROR: accessor bufferView %d out of range\n", acc.bufferView);
        return nullptr;
    }
    const auto& view = model.bufferViews[acc.bufferView];
    if (view.buffer < 0 || view.buffer >= (int)model.buffers.size()) {
        printf("[GLB] ERROR: bufferView buffer %d out of range\n", view.buffer);
        return nullptr;
    }
    const auto& buffer = model.buffers[view.buffer];
    if (view.byteOffset + acc.byteOffset + acc.count * 16 > buffer.data.size()) {
        printf("[GLB] ERROR: accessor reads beyond buffer (offset=%zu, count=%zu, bufsize=%zu)\n",
               view.byteOffset + acc.byteOffset, acc.count, buffer.data.size());
        return nullptr;
    }
    return buffer.data.data() + view.byteOffset + acc.byteOffset;
}

static size_t accessorStride(const tinygltf::Model& model, const tinygltf::Accessor& acc) {
    const auto& view = model.bufferViews[acc.bufferView];
    size_t s = acc.ByteStride(view);
    if (s != 0) return s;
    return (size_t)tinygltf::GetComponentSizeInBytes(acc.componentType) *
           (size_t)tinygltf::GetNumComponentsInType(acc.type);
}

static glm::vec3 readVec3(const tinygltf::Model& model, const tinygltf::Accessor& acc, size_t idx) {
    const unsigned char* base = accessorPtr(model, acc);
    if (!base) return glm::vec3(0.0f);
    const float* f = (const float*)(base + idx * accessorStride(model, acc));
    return glm::vec3(f[0], f[1], f[2]);
}

static bool readIndex(const tinygltf::Model& model, const tinygltf::Accessor& acc, size_t i, unsigned int& out) {
    const unsigned char* base = accessorPtr(model, acc);
    if (!base) return false;
    const unsigned char* p = base + i * accessorStride(model, acc);
    switch (acc.componentType) {
        case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE:
            out = *(const unsigned char*)p; return true;
        case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT:
            out = *(const unsigned short*)p; return true;
        case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT:
            out = *(const unsigned int*)p; return true;
        default:
            printf("[GLB] ERROR: unsupported index componentType=%d\n", acc.componentType);
            return false;
    }
}

static glm::mat4 nodeTransform(const tinygltf::Model& model, int nodeIdx) {
    const auto& node = model.nodes[nodeIdx];
    glm::mat4 m(1.0f);
    if (node.matrix.size() == 16) {
        float mat[16];
        for (int i = 0; i < 16; ++i) mat[i] = (float)node.matrix[i];
        return glm::make_mat4(mat);
    }
    if (node.scale.size() == 3)
        m = glm::scale(m, glm::vec3((float)node.scale[0], (float)node.scale[1], (float)node.scale[2]));
    if (node.rotation.size() == 4) {
        glm::quat q((float)node.rotation[3], (float)node.rotation[0],
                    (float)node.rotation[1], (float)node.rotation[2]);
        m = glm::mat4_cast(q) * m;
    }
    if (node.translation.size() == 3)
        m = glm::translate(glm::mat4(1.0f), glm::vec3((float)node.translation[0],
                            (float)node.translation[1], (float)node.translation[2])) * m;
    return m;
}

static int collectMeshEntries(const tinygltf::Model& model, std::vector<TransformEntry>& entries) {
    std::vector<int> roots;
    if (!model.scenes.empty()) {
        int defaultScene = model.defaultScene >= 0 ? model.defaultScene : 0;
        const auto& scene = model.scenes[defaultScene];
        printf("[GLB] default scene %d has %zu root nodes\n", defaultScene, scene.nodes.size());
        for (int n : scene.nodes) roots.push_back(n);
    }
    if (roots.empty()) {
        printf("[GLB] no scenes with root nodes, using all nodes as roots\n");
        for (size_t i = 0; i < model.nodes.size(); ++i)
            roots.push_back((int)i);
    }

    std::vector<std::pair<int, glm::mat4>> nodeStack;
    for (int r : roots) nodeStack.push_back({r, glm::mat4(1.0f)});

    int visited = 0;
    while (!nodeStack.empty()) {
        auto [idx, parentXform] = nodeStack.back();
        nodeStack.pop_back();
        visited++;
        const auto& node = model.nodes[idx];
        glm::mat4 worldXform = parentXform * nodeTransform(model, idx);
        if (node.mesh >= 0) {
            entries.push_back({idx, worldXform});
            printf("[GLB] node %d has mesh %d, children=%zu\n",
                   idx, node.mesh, node.children.size());
        }
        for (int child : node.children)
            nodeStack.push_back({child, worldXform});
    }
    printf("[GLB] visited %d nodes, %zu with meshes\n", visited, entries.size());
    return visited;
}

static bool resolvePositionAccessor(const tinygltf::Model& model, const tinygltf::Primitive& prim,
                                    size_t primIdx, const tinygltf::Accessor*& posAcc) {
    auto posIt = prim.attributes.find("POSITION");
    if (posIt == prim.attributes.end()) {
        printf("[GLB]   primitive %zu: skipping (no POSITION attribute)\n", primIdx);
        return false;
    }
    int posAccIdx = posIt->second;
    if (posAccIdx < 0 || posAccIdx >= (int)model.accessors.size()) {
        printf("[GLB]   primitive %zu: POSITION accessor %d out of range\n", primIdx, posAccIdx);
        return false;
    }
    posAcc = &model.accessors[posAccIdx];
    printf("[GLB]   primitive %zu: POSITION accessor %d (count=%zu, type=%d, comp=%d)\n",
           primIdx, posAccIdx, posAcc->count, posAcc->type, posAcc->componentType);
    if (posAcc->componentType != TINYGLTF_COMPONENT_TYPE_FLOAT) return false;
    if (posAcc->type != TINYGLTF_TYPE_VEC3) return false;
    return true;
}

static void processIndexedPrimitive(const tinygltf::Model& model, const tinygltf::Accessor& posAcc,
                                     const tinygltf::Accessor& idxAcc, const glm::mat4& transform,
                                     size_t primIdx, std::vector<Triangle>& outTriangles,
                                     size_t& totalTriangles) {
    size_t count = idxAcc.count;
    printf("[GLB]   primitive %zu: indexed (%zu indices, type=%d)\n",
           primIdx, count, idxAcc.componentType);
    if (count % 3 != 0)
        printf("[GLB]   WARNING: index count %zu not divisible by 3\n", count);
    unsigned int ia, ib, ic;
    for (size_t i = 0; i + 2 < count; i += 3) {
        if (!readIndex(model, idxAcc, i, ia) || !readIndex(model, idxAcc, i+1, ib) || !readIndex(model, idxAcc, i+2, ic)) {
            printf("[GLB]   ERROR reading indices at %zu\n", i);
            continue;
        }
        glm::vec3 a = glm::vec3(transform * glm::vec4(readVec3(model, posAcc, ia), 1.0f));
        glm::vec3 b = glm::vec3(transform * glm::vec4(readVec3(model, posAcc, ib), 1.0f));
        glm::vec3 c = glm::vec3(transform * glm::vec4(readVec3(model, posAcc, ic), 1.0f));
        outTriangles.push_back(Triangle(a, b, c));
        totalTriangles++;
    }
}

static void processNonIndexedPrimitive(const tinygltf::Model& model, const tinygltf::Accessor& posAcc,
                                        const glm::mat4& transform, size_t primIdx,
                                        std::vector<Triangle>& outTriangles,
                                        size_t& totalTriangles) {
    size_t count = posAcc.count;
    printf("[GLB]   primitive %zu: non-indexed (%zu vertices)\n", primIdx, count);
    if (count % 3 != 0)
        printf("[GLB]   WARNING: vertex count %zu not divisible by 3\n", count);
    for (size_t i = 0; i + 2 < count; i += 3) {
        glm::vec3 a = glm::vec3(transform * glm::vec4(readVec3(model, posAcc, i), 1.0f));
        glm::vec3 b = glm::vec3(transform * glm::vec4(readVec3(model, posAcc, i+1), 1.0f));
        glm::vec3 c = glm::vec3(transform * glm::vec4(readVec3(model, posAcc, i+2), 1.0f));
        outTriangles.push_back(Triangle(a, b, c));
        totalTriangles++;
    }
}

static bool processPrimitive(const tinygltf::Model& model, const tinygltf::Primitive& prim,
                             const glm::mat4& transform, size_t primIdx,
                             std::vector<Triangle>& outTriangles, size_t& totalTriangles) {
    const tinygltf::Accessor* posAcc = nullptr;
    if (!resolvePositionAccessor(model, prim, primIdx, posAcc))
        return false;
    if (prim.indices >= 0)
        processIndexedPrimitive(model, *posAcc, model.accessors[prim.indices], transform, primIdx, outTriangles, totalTriangles);
    else
        processNonIndexedPrimitive(model, *posAcc, transform, primIdx, outTriangles, totalTriangles);
    return true;
}

static void processMeshGeometry(const tinygltf::Model& model, const TransformEntry& entry,
                                std::vector<Triangle>& outTriangles, size_t& totalTriangles,
                                size_t& meshIdx) {
    const auto& node = model.nodes[entry.nodeIdx];
    if (node.mesh < 0 || node.mesh >= (int)model.meshes.size()) {
        printf("[GLB] WARNING: node %d has invalid mesh index %d\n",
               entry.nodeIdx, node.mesh);
        return;
    }

    const auto& mesh = model.meshes[node.mesh];
    printf("[GLB] processing mesh %d (%zu primitives)\n", node.mesh, mesh.primitives.size());

    size_t primIdx = 0;
    for (const auto& prim : mesh.primitives) {
        processPrimitive(model, prim, entry.transform, primIdx, outTriangles, totalTriangles);
        primIdx++;
    }
    meshIdx++;
}

bool loadGLBMap(const char* path, TestMap& outMap) {
    printf("[GLB] opening file: %s\n", path);

    tinygltf::TinyGLTF loader;
    tinygltf::Model model;
    std::string err, warn;

    printf("[GLB] parse start\n");
    bool ok = loader.LoadBinaryFromFile(&model, &err, &warn, path);

    if (!warn.empty())
        printf("[GLB] parse warning: %s\n", warn.c_str());

    if (!err.empty()) {
        printf("[GLB] parse error: %s\n", err.c_str());
        return false;
    }

    if (!ok) {
        printf("[GLB] parse failed (unknown reason)\n");
        return false;
    }

    printf("[GLB] parse success\n");
    printf("[GLB] scenes=%zu\n", model.scenes.size());
    printf("[GLB] nodes=%zu\n", model.nodes.size());
    printf("[GLB] meshes=%zu\n", model.meshes.size());
    printf("[GLB] accessors=%zu\n", model.accessors.size());
    printf("[GLB] bufferViews=%zu\n", model.bufferViews.size());
    printf("[GLB] buffers=%zu\n", model.buffers.size());

    outMap.triangles.clear();
    outMap.name = path;
    outMap.spawnPosition = glm::vec3(0.0f, 0.0f, 2.0f);

    std::vector<TransformEntry> entries;
    printf("[GLB] walking scene graph...\n");
    collectMeshEntries(model, entries);

    size_t totalTriangles = 0;
    size_t meshIdx = 0;
    for (const auto& entry : entries)
        processMeshGeometry(model, entry, outMap.triangles, totalTriangles, meshIdx);

    printf("[GLB] extracted %zu triangles from %zu meshes\n", totalTriangles, meshIdx);
    return totalTriangles > 0;
}
