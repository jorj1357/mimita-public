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
#include "game/gl-resource-bridge-module.h"
#include "game/job-bridge-module.h"

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
#include "render/render-command.h"

namespace r = mimita::game::render;

extern TextureStore gTextures;

namespace {

std::uint32_t uploadWeaponTexture(const tinygltf::Image& image)
{
    if (image.image.empty() || image.width <= 0 || image.height <= 0)
        return 0;
    if (image.component < 1 || image.component > 4)
        return 0;

    std::uint32_t tex = 0;
    tex = mimita::game::genTexture();
    if (!tex) return 0;
    r::bindTexture(r::TextureTarget::Texture2D, tex);

    r::PixelFormat srcFormat = r::PixelFormat::RGBA;
    if (image.component == 1) srcFormat = r::PixelFormat::Red;
    else if (image.component == 2) srcFormat = r::PixelFormat::RG;
    else if (image.component == 3) srcFormat = r::PixelFormat::RGB;

    r::pixelStorei(r::PixelStoreParameter::UnpackAlignment, 1);
    r::texImage2D(r::TextureTarget::Texture2D, 0, r::TextureFormat::RGBA,
                  image.width, image.height, 0, srcFormat,
                  r::PixelType::UnsignedByte, image.image.data());
    r::texParameteri(r::TextureTarget::Texture2D, r::TextureParameter::WrapS,
                     r::TextureValue::Repeat);
    r::texParameteri(r::TextureTarget::Texture2D, r::TextureParameter::WrapT,
                     r::TextureValue::Repeat);
    r::texParameteri(r::TextureTarget::Texture2D, r::TextureParameter::MinFilter,
                     r::TextureValue::LinearMipmapLinear);
    r::texParameteri(r::TextureTarget::Texture2D, r::TextureParameter::MagFilter,
                     r::TextureValue::Linear);
    r::generateMipmap(r::TextureTarget::Texture2D);
    r::pixelStorei(r::PixelStoreParameter::UnpackAlignment, 4);

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
    std::vector<std::uint32_t> zeroTextures(model.materials.size(), 0);
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

    mimita::game::runDetached([asset] { weaponModelLoadThread(asset); }, "weapon-model-load");
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
        asset->vao = mimita::game::genVertexArray();
        asset->vbo = mimita::game::genBuffer();
        r::bindVertexArray(asset->vao);
        r::bindBuffer(r::BufferTarget::Array, asset->vbo);
        r::bufferData(r::BufferTarget::Array, asset->vbo, asset->mesh.verts.data(),
                      static_cast<std::uint32_t>(asset->mesh.verts.size() * sizeof(Vertex)),
                      r::BufferUsage::Static);
        r::enableVertexAttribArray(0);
        r::vertexAttribPointer(0, 3, r::VertexType::Float, false, sizeof(Vertex),
                               static_cast<std::uint32_t>(offsetof(Vertex, pos)));
        r::enableVertexAttribArray(1);
        r::vertexAttribPointer(1, 3, r::VertexType::Float, false, sizeof(Vertex),
                               static_cast<std::uint32_t>(offsetof(Vertex, normal)));
        r::enableVertexAttribArray(2);
        r::vertexAttribPointer(2, 2, r::VertexType::Float, false, sizeof(Vertex),
                               static_cast<std::uint32_t>(offsetof(Vertex, uv)));
        r::bindVertexArray(0);

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
