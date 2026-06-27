#include "outfit-system.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <numeric>

#include <glm/glm.hpp>
#include <nlohmann/json.hpp>

#include "entities/player.h"
#include "world/texture-store.h"
#include "map/map_common.h"
#include "map/map_loader.h"
#include "stb_image.h"
#include "stb_image_resize2.h"

using json = nlohmann::json;

extern TextureStore gTextures;

// ── Utility ─────────────────────────────────────────────────────────
int outfitPartRow(const std::string& name)
{
    if (name == "head") return 0;
    if (name == "torso") return 1;
    if (name == "leftArm") return 2;
    if (name == "rightArm") return 3;
    if (name == "leftLeg") return 4;
    if (name == "rightLeg") return 5;
    return -1;
}

int outfitFaceColumn(const std::string& name)
{
    if (name == "front") return 0;
    if (name == "back") return 1;
    if (name == "left") return 2;
    if (name == "right") return 3;
    if (name == "top") return 4;
    if (name == "bottom") return 5;
    return -1;
}

static int faceColumnForNormal(const glm::vec3& normal)
{
    glm::vec3 a = glm::abs(normal);
    if (a.z >= a.x && a.z >= a.y) return normal.z >= 0.0f ? 4 : 5; // top / bottom
    if (a.y >= a.x) return normal.y <= 0.0f ? 0 : 1; // front / back
    return normal.x <= 0.0f ? 2 : 3; // left / right
}

static glm::vec2 projectedUV(const Vertex& v, int face, glm::vec3 mn, glm::vec3 mx)
{
    glm::vec3 size = glm::max(mx - mn, glm::vec3(0.0001f));
    glm::vec2 uv;
    if (face == 4 || face == 5) // top/bottom
        uv = {(v.pos.x - mn.x) / size.x, (v.pos.y - mn.y) / size.y};
    else if (face <= 1) // front/back
        uv = {(v.pos.x - mn.x) / size.x, (v.pos.z - mn.z) / size.z};
    else // left/right
        uv = {(v.pos.y - mn.y) / size.y, (v.pos.z - mn.z) / size.z};
    if (face == 1 || face == 3 || face == 5)
        uv.x = 1.0f - uv.x;
    return glm::clamp(uv, glm::vec2(0.0f), glm::vec2(1.0f));
}

static glm::vec2 atlasUV(int row, int col, glm::vec2 local)
{
    float x = (float)(kAtlasPadding + col * (kAtlasCell + kAtlasGap) + kAtlasInset);
    float y = (float)(kAtlasPadding + row * (kAtlasCell + kAtlasGap) + kAtlasInset);
    return {(x + local.x * (float)kAtlasUsable) / (float)kAtlasSize,
            (y + local.y * (float)kAtlasUsable) / (float)kAtlasSize};
}

std::string OutfitSystem::outfitPath(const std::string& name)
{
    return "assets/avatars/" + name;
}

// ── Singleton ───────────────────────────────────────────────────────
OutfitSystem& OutfitSystem::instance()
{
    static OutfitSystem sys;
    return sys;
}

std::string OutfitSystem::resolvePath(const std::string& relativePath) const
{
    if (relativePath.empty()) return {};
    if (relativePath.find('/') != std::string::npos || relativePath.find('\\') != std::string::npos)
        return relativePath;
    return mBasePath + "/" + relativePath;
}

// ── JSON parsing ────────────────────────────────────────────────────
bool OutfitSystem::parseOutfitJson(const std::string& path)
{
    std::ifstream file(path);
    if (!file.is_open()) {
        printf("[OUTFIT] No outfit.json at %s\n", path.c_str());
        return false;
    }

    try {
        json j;
        file >> j;

        mData.name = j.value("name", mCurrentName);

        // Parse texture library
        if (j.contains("textures") && j["textures"].is_object()) {
            for (auto& [key, val] : j["textures"].items())
                mData.textures[key] = val.get<std::string>();
        }

        // Parse faces
        if (j.contains("faces") && j["faces"].is_object()) {
            for (auto& [partKey, partVal] : j["faces"].items()) {
                if (!partVal.is_object()) continue;
                for (auto& [faceKey, faceVal] : partVal.items()) {
                    PartFaceAssignment assign;
                    if (faceVal.is_string()) {
                        assign.textureAlias = faceVal.get<std::string>();
                    } else if (faceVal.is_object()) {
                        assign.textureAlias = faceVal.value("texture", "");
                    }
                    mData.faces[partKey][faceKey] = assign;
                }
            }
        }

        // Parse face overrides
        if (j.contains("faceOverrides") && j["faceOverrides"].is_object()) {
            for (auto& [key, ov] : j["faceOverrides"].items()) {
                // key format: "part_face" e.g. "head_front"
                size_t us = key.find('_');
                if (us == std::string::npos) continue;
                std::string part = key.substr(0, us);
                std::string face = key.substr(us + 1);
                auto it = mData.faces.find(part);
                if (it == mData.faces.end()) continue;
                auto fit = it->second.find(face);
                if (fit == it->second.end()) continue;
                auto& o = fit->second.overrides;
                if (ov.contains("stretchMode")) o.stretchMode = ov["stretchMode"];
                if (ov.contains("rotation")) o.rotation = ov["rotation"];
                if (ov.contains("offsetX")) o.offsetX = ov["offsetX"];
                if (ov.contains("offsetY")) o.offsetY = ov["offsetY"];
                if (ov.contains("scaleX")) o.scaleX = ov["scaleX"];
                if (ov.contains("scaleY")) o.scaleY = ov["scaleY"];
                if (ov.contains("hue")) o.hue = ov["hue"];
                if (ov.contains("saturation")) o.saturation = ov["saturation"];
                if (ov.contains("brightness")) o.brightness = ov["brightness"];
                if (ov.contains("contrast")) o.contrast = ov["contrast"];
                if (ov.contains("opacity")) o.opacity = ov["opacity"];
                if (ov.contains("tint") && ov["tint"].is_array() && ov["tint"].size() >= 3)
                    o.tint = {ov["tint"][0], ov["tint"][1], ov["tint"][2]};
            }
        }

        // Parse colors
        if (j.contains("colors") && j["colors"].is_object()) {
            for (auto& [key, val] : j["colors"].items()) {
                if (val.is_array() && val.size() >= 3)
                    mData.colors[key] = {val[0], val[1], val[2]};
            }
        }

        // Parse cosmetics
        if (j.contains("cosmetics") && j["cosmetics"].is_array()) {
            for (auto& c : j["cosmetics"]) {
                CosmeticInstance ci;
                ci.id = c.value("id", "");
                ci.parentBone = c.value("parent", "root");
                if (c.contains("position") && c["position"].is_array() && c["position"].size() >= 3)
                    ci.position = {c["position"][0], c["position"][1], c["position"][2]};
                if (c.contains("rotation") && c["rotation"].is_array() && c["rotation"].size() >= 3)
                    ci.rotation = {c["rotation"][0], c["rotation"][1], c["rotation"][2]};
                if (c.contains("scale") && c["scale"].is_array() && c["scale"].size() >= 3)
                    ci.scale = {c["scale"][0], c["scale"][1], c["scale"][2]};
                // Load GLB mesh
                if (c.contains("model")) {
                    std::string modelPath = resolvePath(c["model"].get<std::string>());
                    ci.renderMesh = loadGLB(modelPath.c_str());
                    if (ci.renderMesh.verts.empty())
                        printf("[OUTFIT] Failed to load cosmetic model: %s\n", modelPath.c_str());
                }
                mData.cosmetics.push_back(std::move(ci));
            }
        }

        printf("[OUTFIT] Parsed outfit: %s (%zu textures, %zu cosmetics)\n",
               mData.name.c_str(), mData.textures.size(), mData.cosmetics.size());
        return true;

    } catch (const std::exception& e) {
        printf("[OUTFIT] Failed to parse outfit.json: %s\n", e.what());
        return false;
    }
}

// ── Load ────────────────────────────────────────────────────────────
bool OutfitSystem::load(const std::string& outfitName)
{
    mCurrentName = outfitName;
    mBasePath = outfitPath(outfitName);
    mData = OutfitData{};
    mData.name = outfitName;
    mData.basePath = mBasePath;

    std::string jsonPath = mBasePath + "/outfit.json";
    if (!parseOutfitJson(jsonPath)) {
        printf("[OUTFIT] Falling back: no valid outfit.json for %s\n", outfitName.c_str());
        mLoaded = false;
        return false;
    }

    mLoaded = true;
    if (std::filesystem::exists(jsonPath))
        mLastWriteTime = std::filesystem::last_write_time(jsonPath);
    mLastCheckTime = std::chrono::steady_clock::now();
    printf("[OUTFIT] Loaded: %s\n", outfitName.c_str());
    return true;
}

// ── Atlas building ──────────────────────────────────────────────────
bool OutfitSystem::buildAtlas(Player& player)
{
    if (!mLoaded) return false;

    std::vector<unsigned char> atlasPixels(kAtlasSize * kAtlasSize * 4, 128);
    std::vector<unsigned char> scaled(kAtlasUsable * kAtlasUsable * 4);

    // For each body part × face, load the referenced texture into the atlas cell
    for (int pi = 0; pi < kOutfitPartCount; ++pi) {
        std::string partKey = kOutfitPartKeys[pi];
        for (int fi = 0; fi < kOutfitFaceCount; ++fi) {
            std::string faceKey = kOutfitFaceKeys[fi];

            // Look up the texture alias for this part/face
            auto pit = mData.faces.find(partKey);
            if (pit == mData.faces.end()) continue;
            auto fit = pit->second.find(faceKey);
            if (fit == pit->second.end()) continue;

            std::string alias = fit->second.textureAlias;
            auto tit = mData.textures.find(alias);
            if (tit == mData.textures.end()) continue;

            std::string fullPath = resolvePath(tit->second);
            if (!std::filesystem::exists(fullPath)) continue;

            int w, h, n;
            unsigned char* data = stbi_load(fullPath.c_str(), &w, &h, &n, 4);
            if (!data) continue;

            if (w != kAtlasUsable || h != kAtlasUsable)
                stbir_resize_uint8_linear(data, w, h, 0, scaled.data(), kAtlasUsable, kAtlasUsable, 0, STBIR_RGBA);
            else
                std::memcpy(scaled.data(), data, kAtlasUsable * kAtlasUsable * 4);
            stbi_image_free(data);

            int cellX = kAtlasPadding + fi * (kAtlasCell + kAtlasGap) + kAtlasInset;
            int cellY = kAtlasPadding + pi * (kAtlasCell + kAtlasGap) + kAtlasInset;
            for (int y = 0; y < kAtlasUsable; ++y) {
                std::memcpy(&atlasPixels[((cellY + y) * kAtlasSize + cellX) * 4],
                            &scaled[y * kAtlasUsable * 4], kAtlasUsable * 4);
            }
        }
    }

    // Upload to GPU
    if (mData.atlasTexture) {
        glDeleteTextures(1, &mData.atlasTexture);
        mData.atlasTexture = 0;
    }

    glGenTextures(1, &mData.atlasTexture);
    glBindTexture(GL_TEXTURE_2D, mData.atlasTexture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, kAtlasSize, kAtlasSize, 0, GL_RGBA, GL_UNSIGNED_BYTE, atlasPixels.data());
    glGenerateMipmap(GL_TEXTURE_2D);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 4);

    printf("[OUTFIT] Built atlas for: %s\n", mCurrentName.c_str());
    return true;
}

// ── Apply to player ─────────────────────────────────────────────────
bool OutfitSystem::applyAtlasToPlayer(Player& player)
{
    if (!mData.atlasTexture) return false;

    for (size_t partIndex = 0; partIndex < player.physicalBody.parts.size(); ++partIndex) {
        if (partIndex >= player.physicalBody.partMeshes.size()) break;
        const std::string& name = player.physicalBody.parts[partIndex].name;
        int row = outfitPartRow(name);
        if (row < 0) continue;

        Mesh& mesh = player.physicalBody.partMeshes[partIndex];
        if (mesh.verts.empty()) continue;

        glm::vec3 mn = mesh.verts[0].pos;
        glm::vec3 mx = mesh.verts[0].pos;
        for (const Vertex& v : mesh.verts) {
            mn = glm::min(mn, v.pos);
            mx = glm::max(mx, v.pos);
        }

        // Get face overrides for this part (from outfit.json faceOverrides)
        const FaceOverride* partOverrides[kOutfitFaceCount] = {};
        std::string partKey = name;
        auto pit = mData.faces.find(partKey);
        if (pit != mData.faces.end()) {
            for (int fi = 0; fi < kOutfitFaceCount; ++fi) {
                auto fit = pit->second.find(kOutfitFaceKeys[fi]);
                if (fit != pit->second.end())
                    partOverrides[fi] = &fit->second.overrides;
            }
        }

        for (size_t i = 0; i + 2 < mesh.verts.size(); i += 3) {
            glm::vec3 normal = mesh.verts[i].normal + mesh.verts[i + 1].normal + mesh.verts[i + 2].normal;
            if (glm::dot(normal, normal) < 0.000001f)
                normal = glm::cross(mesh.verts[i + 1].pos - mesh.verts[i].pos,
                                    mesh.verts[i + 2].pos - mesh.verts[i].pos);
            int col = faceColumnForNormal(normal);

            for (size_t v = i; v < i + 3; ++v) {
                glm::vec2 uv = projectedUV(mesh.verts[v], col, mn, mx);

                // Apply face overrides
                if (col >= 0 && col < kOutfitFaceCount && partOverrides[col]) {
                    const FaceOverride& ov = *partOverrides[col];
                    // Offset
                    uv.x += ov.offsetX;
                    uv.y += ov.offsetY;
                    // Scale
                    uv.x = 0.5f + (uv.x - 0.5f) * ov.scaleX;
                    uv.y = 0.5f + (uv.y - 0.5f) * ov.scaleY;
                    // Rotation (simplified: 90-degree steps via axis swap)
                    if (ov.rotation >= 45.0f && ov.rotation < 135.0f) {
                        float tmp = uv.x; uv.x = 1.0f - uv.y; uv.y = tmp;
                    } else if (ov.rotation >= 135.0f && ov.rotation < 225.0f) {
                        uv.x = 1.0f - uv.x; uv.y = 1.0f - uv.y;
                    } else if (ov.rotation >= 225.0f && ov.rotation < 315.0f) {
                        float tmp = uv.x; uv.x = uv.y; uv.y = 1.0f - tmp;
                    }
                    // Clamp
                    uv = glm::clamp(uv, glm::vec2(0.0f), glm::vec2(1.0f));
                }

                mesh.verts[v].uv = atlasUV(row, col, uv);
            }
        }

        for (auto& batch : mesh.batches)
            batch.texture = mData.atlasTexture;
    }

    player.bodyPartMeshes = player.physicalBody.partMeshes;
    printf("[OUTFIT] Applied atlas to player\n");
    return true;
}

bool OutfitSystem::applyToPlayer(Player& player)
{
    if (!mLoaded || mCurrentName.empty()) return false;
    if (!buildAtlas(player)) return false;
    return applyAtlasToPlayer(player);
}

// ── Apply single texture (replaces OutfitAtlas) ─────────────────────
bool OutfitSystem::applySingleTexture(Player& player, const std::string& texturePath, bool reloadTexture)
{
    GLuint texture = gTextures.getPath(texturePath, reloadTexture);
    if (!texture) return false;

    for (size_t partIndex = 0; partIndex < player.physicalBody.partMeshes.size(); ++partIndex) {
        Mesh& mesh = player.physicalBody.partMeshes[partIndex];
        if (mesh.verts.empty()) continue;

        // Compute per-triangle face column UVs (same as atlas but without atlas lookup)
        glm::vec3 mn = mesh.verts[0].pos;
        glm::vec3 mx = mesh.verts[0].pos;
        for (const Vertex& v : mesh.verts) {
            mn = glm::min(mn, v.pos);
            mx = glm::max(mx, v.pos);
        }
        for (size_t i = 0; i + 2 < mesh.verts.size(); i += 3) {
            glm::vec3 normal = mesh.verts[i].normal + mesh.verts[i + 1].normal + mesh.verts[i + 2].normal;
            if (glm::dot(normal, normal) < 0.000001f)
                normal = glm::cross(mesh.verts[i + 1].pos - mesh.verts[i].pos,
                                    mesh.verts[i + 2].pos - mesh.verts[i].pos);
            int col = faceColumnForNormal(normal);
            for (size_t v = i; v < i + 3; ++v)
                mesh.verts[v].uv = projectedUV(mesh.verts[v], col, mn, mx);
        }
        for (auto& batch : mesh.batches)
            batch.texture = texture;
    }
    player.bodyPartMeshes = player.physicalBody.partMeshes;
    return true;
}

// ── Hot reload ──────────────────────────────────────────────────────
void OutfitSystem::pollHotReload(Player* player)
{
    if (!mLoaded || mCurrentName.empty()) return;
    auto now = std::chrono::steady_clock::now();
    if (std::chrono::duration<float>(now - mLastCheckTime).count() < mPollInterval)
        return;
    mLastCheckTime = now;

    std::string jsonPath = mBasePath + "/outfit.json";
    if (!std::filesystem::exists(jsonPath)) return;

    auto writeTime = std::filesystem::last_write_time(jsonPath);
    if (writeTime == mLastWriteTime) return;
    mLastWriteTime = writeTime;

    printf("[OUTFIT] Hot reload detected for: %s\n", mCurrentName.c_str());
    load(mCurrentName);
    if (player)
        applyToPlayer(*player);
}

// ── List outfits ────────────────────────────────────────────────────
std::vector<std::string> OutfitSystem::listOutfits() const
{
    std::vector<std::string> result;
    const std::string dir = "assets/avatars";
    if (!std::filesystem::exists(dir)) return result;
    for (const auto& entry : std::filesystem::directory_iterator(dir)) {
        if (entry.is_directory())
            result.push_back(entry.path().filename().string());
    }
    std::sort(result.begin(), result.end());
    return result;
}
