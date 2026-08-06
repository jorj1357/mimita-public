// 08 05 2026, 22 30
/* purpose
* Background-thread weapon GLB parsing and shared GPU upload.
* Parses each weapon model path once on a worker thread (tinygltf + vertex
* expansion, no GL calls) and uploads textures/VBO/VAO on the main thread so
* equipping a weapon never freezes the game.
* Does NOT own weapon runtimes, ammo, firing, attachment, or rendering.
* Does NOT load maps, player characters, or NPC models.
*/

#include "combat/weapon-model-cache.h"

#include <atomic>
#include <cstddef>
#include <cstdio>
#include <thread>
#include <vector>

#include <glad/glad.h>
#include <glm/glm.hpp>

#include "map/map-loader-mesh.h"
#include "combat/weapon-registry.h"
#include "combat/weapon-config.h"
#include "utils/path_utils.h"
#include "world/texture-store.h"
#include "debug/debug-log.h"

extern TextureStore gTextures;

namespace {

GLuint uploadWeaponTexture(const tinygltf::Image& image)
{
    if (image.image.empty() || image.width <= 0 || image.height <= 0)
        return 0;
    if (image.component < 1 || image.component > 4)
        return 0;

    GLuint tex = 0;
    glGenTextures(1, &tex);
    if (!tex) return 0;
    glBindTexture(GL_TEXTURE_2D, tex);

    GLenum srcFormat = GL_RGBA;
    if (image.component == 1) srcFormat = GL_RED;
    else if (image.component == 2) srcFormat = GL_RG;
    else if (image.component == 3) srcFormat = GL_RGB;

    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, image.width, image.height, 0,
                 srcFormat, GL_UNSIGNED_BYTE, image.image.data());
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glGenerateMipmap(GL_TEXTURE_2D);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 4);

    return tex;
}

// Worker thread: parse the GLB into CPU data (no GL calls).
void weaponModelLoadThread(std::shared_ptr<PendingWeaponModel> d)
{
    tinygltf::TinyGLTF loader;
    tinygltf::Model model;
    std::string err, warn;

    bool ok = loader.LoadBinaryFromFile(&model, &err, &warn, d->resolvedPath);
    if (!warn.empty()) Debug::warn(Debug::Category::Weapons, "[WEAPON MODEL WARNING] %s\n", warn.c_str());
    if (!err.empty()) Debug::warn(Debug::Category::Weapons, "[WEAPON MODEL ERROR] %s\n", err.c_str());
    if (!ok) {
        Debug::warn(Debug::Category::Weapons, "[WEAPON MODEL ERROR] failed background load %s\n", d->path.c_str());
        d->ready.store(true);
        return;
    }

    d->images = std::move(model.images);

    // Material index -> image index so batch textures resolve on the main thread.
    // Read d->images (not model.images): model.images was emptied by the move above.
    d->materialToImage.assign(model.materials.size(), -1);
    for (size_t m = 0; m < model.materials.size(); ++m) {
        int texIdx = model.materials[m].pbrMetallicRoughness.baseColorTexture.index;
        if (texIdx >= 0 && texIdx < (int)model.textures.size()) {
            int imgIdx = model.textures[texIdx].source;
            if (imgIdx >= 0 && imgIdx < (int)d->images.size())
                d->materialToImage[m] = imgIdx;
        }
    }

    // Build the CPU mesh via the standard GLB scene walk. The walker assigns
    // texture handles; those are resolved to real GL textures on the main thread.
    std::vector<GLuint> zeroTextures(model.materials.size(), 0);
    Mesh mesh;
    int sceneIndex = model.defaultScene >= 0 ? model.defaultScene : 0;
    walkGLBScene(model, zeroTextures, mesh, sceneIndex, nullptr);

    d->mesh = std::move(mesh);
    d->loadOk = !d->mesh.verts.empty();
    if (!d->loadOk)
        Debug::warn(Debug::Category::Weapons, "[WEAPON MODEL ERROR] model produced no vertices: %s\n", d->path.c_str());

    d->ready.store(true);
}

} // anonymous namespace

WeaponModelCache& WeaponModelCache::instance()
{
    static WeaponModelCache cache;
    return cache;
}

std::shared_ptr<PendingWeaponModel> WeaponModelCache::request(const std::string& modelPath)
{
    if (modelPath.empty())
        return nullptr;

    // The GLB scene walker reads the shared default texture on the worker thread;
    // warm it on the main thread first so no GL call / throttled log ever happens
    // off the main thread.
    getGLBDefaultTexture();

    std::lock_guard<std::mutex> lock(mMutex);
    auto it = mEntries.find(modelPath);
    if (it != mEntries.end())
        return it->second.asset;

    auto asset = std::make_shared<PendingWeaponModel>();
    asset->path = modelPath;
    asset->resolvedPath = resolveAssetPath(modelPath);

    mEntries.emplace(modelPath, Entry{asset});

    std::thread t(weaponModelLoadThread, asset);
    t.detach();
    return asset;
}

void WeaponModelCache::finalizeWeaponModelsIfReady()
{
    std::vector<std::shared_ptr<PendingWeaponModel>> readyAssets;
    {
        std::lock_guard<std::mutex> lock(mMutex);
        readyAssets.reserve(mEntries.size());
        for (auto& kv : mEntries) {
            std::shared_ptr<PendingWeaponModel>& asset = kv.second.asset;
            if (asset->ready.load() && !asset->gpuUploaded)
                readyAssets.push_back(asset);
        }
    }

    for (auto& asset : readyAssets) {
        if (asset->gpuUploaded)
            continue;
        if (!asset->ready.load())
            continue;
        if (!asset->loadOk) {
            asset->gpuUploaded = true; // never retry a failed parse
            continue;
        }

        // Upload textures (must be on the main thread).
        asset->uploadedTextures.reserve(asset->images.size());
        for (const auto& img : asset->images)
            asset->uploadedTextures.push_back(uploadWeaponTexture(img));

        // Resolve batch textures from the worker-computed material->image map.
        for (auto& batch : asset->mesh.batches) {
            int matIdx = batch.materialIndex;
            int imgIdx = (matIdx >= 0 && matIdx < (int)asset->materialToImage.size())
                ? asset->materialToImage[matIdx] : -1;
            if (imgIdx >= 0 && imgIdx < (int)asset->uploadedTextures.size() &&
                asset->uploadedTextures[imgIdx])
                batch.texture = asset->uploadedTextures[imgIdx];
            else if (!batch.texture)
                batch.texture = gTextures.get("default");
        }

        // Upload the mesh to the GPU (shared by all viewmodels).
        glGenVertexArrays(1, &asset->vao);
        glGenBuffers(1, &asset->vbo);
        glBindVertexArray(asset->vao);
        glBindBuffer(GL_ARRAY_BUFFER, asset->vbo);
        glBufferData(GL_ARRAY_BUFFER, asset->mesh.verts.size() * sizeof(Vertex),
                     asset->mesh.verts.data(), GL_STATIC_DRAW);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, pos));
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, normal));
        glEnableVertexAttribArray(2);
        glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, uv));
        glBindVertexArray(0);

        asset->gpuUploaded = true;
        Debug::log(Debug::Category::Weapons, "[WEAPON MODEL] uploaded=%s verts=%zu vao=%u\n",
                   asset->path.c_str(), asset->mesh.verts.size(), asset->vao);
    }
}

void WeaponModelCache::preloadAll()
{
    const WeaponRegistry& reg = WeaponRegistry::instance();
    for (const auto& pair : reg.all()) {
        const WeaponDefinition& def = pair.second;
        std::string modelPath = def.modelPath;
        const WeaponViewModelConfig* vmcfg = WeaponConfig::instance().get(def.id);
        if (vmcfg && !vmcfg->modelPath.empty())
            modelPath = vmcfg->modelPath;
        if (!modelPath.empty())
            request(modelPath);
    }
    Debug::log(Debug::Category::Weapons, "[WEAPON MODEL] preload requested %zu model paths\n", mEntries.size());
}
