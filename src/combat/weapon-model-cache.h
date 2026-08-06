// 08 05 2026, 22 30
/* purpose
* Shared cache that parses weapon viewmodel GLBs on a background thread so the
* first equip of a weapon never blocks the main thread.
* Each model path is parsed exactly once per process; GPU upload happens on the
* main thread through finalizeWeaponModelsIfReady().
* Does NOT own weapon runtimes, ammo, firing, attachment, or rendering.
* Does NOT load maps, player characters, or NPC models.
*/

#pragma once

#include <atomic>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

#include <glad/glad.h>
#include <glm/glm.hpp>

#include "map/map_common.h"
#include "tinygltf/tiny_gltf.h"

// CPU-parsed weapon model asset shared by every viewmodel that uses the path.
struct PendingWeaponModel {
    std::string path;
    std::string resolvedPath;
    std::atomic<bool> ready{false};        // set by the worker when CPU parse finishes
    bool loadOk = false;                   // worker result
    std::vector<tinygltf::Image> images;   // decoded pixels (worker)
    Mesh mesh;                             // CPU verts/batches; texture resolved on main thread
    std::vector<int> materialToImage;      // material index -> image index (worker)
    std::vector<GLuint> uploadedTextures;  // main-thread GL textures
    unsigned int vao = 0;
    unsigned int vbo = 0;
    bool gpuUploaded = false;              // set on the main thread when GL ready
};

class WeaponModelCache {
public:
    static WeaponModelCache& instance();

    // Returns the shared asset for `modelPath`, starting a background parse the
    // first time a path is requested. Returns nullptr for an empty path.
    std::shared_ptr<PendingWeaponModel> request(const std::string& modelPath);

    // Main thread only: uploads GPU resources for any assets whose CPU parse
    // finished since the last call.
    void finalizeWeaponModelsIfReady();

    // Requests every unique weapon model path so parses happen in the background
    // before the player first equips each weapon.
    void preloadAll();

private:
    struct Entry {
        std::shared_ptr<PendingWeaponModel> asset;
    };
    std::unordered_map<std::string, Entry> mEntries;
    std::mutex mMutex;
};
