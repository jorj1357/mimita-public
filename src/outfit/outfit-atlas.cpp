#include "outfit-system.h"

#include <algorithm>
#include <cstdio>
#include <cstring>

#include <glm/glm.hpp>

#include "entities/player.h"
#include "world/texture-store.h"
#include "map/map_common.h"
#include "stb_image.h"
#include "stb_image_resize2.h"

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
    if (a.z >= a.x && a.z >= a.y) return normal.z >= 0.0f ? 4 : 5;
    if (a.y >= a.x) return normal.y <= 0.0f ? 0 : 1;
    return normal.x <= 0.0f ? 2 : 3;
}

static glm::vec2 projectedUV(const Vertex& v, int face, glm::vec3 mn, glm::vec3 mx)
{
    glm::vec3 size = glm::max(mx - mn, glm::vec3(0.0001f));
    glm::vec2 uv;
    if (face == 4 || face == 5)
        uv = {(v.pos.x - mn.x) / size.x, (v.pos.y - mn.y) / size.y};
    else if (face <= 1)
        uv = {(v.pos.x - mn.x) / size.x, (v.pos.z - mn.z) / size.z};
    else
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

// ── Atlas building ──────────────────────────────────────────────────
bool OutfitSystem::buildAtlas(Player& player)
{
    (void)player;
    if (!mLoaded) return false;

    std::vector<unsigned char> atlasPixels(kAtlasSize * kAtlasSize * 4, 128);
    std::vector<unsigned char> scaled(kAtlasUsable * kAtlasUsable * 4);

    for (int pi = 0; pi < kOutfitPartCount; ++pi) {
        std::string partKey = kOutfitPartKeys[pi];
        for (int fi = 0; fi < kOutfitFaceCount; ++fi) {
            std::string faceKey = kOutfitFaceKeys[fi];
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

            int stretchMode = 0, rotation = 0;
            auto pitO = mData.faces.find(partKey);
            if (pitO != mData.faces.end()) {
                auto fitO = pitO->second.find(faceKey);
                if (fitO != pitO->second.end()) {
                    stretchMode = fitO->second.overrides.stretchMode;
                    rotation = (int)fitO->second.overrides.rotation;
                }
            }

            if (stretchMode == 1) {
                float imgAspect = (float)w / (float)h;
                float cellAspect = 1.0f;
                int fitW, fitH;
                if (imgAspect > cellAspect) {
                    fitW = kAtlasUsable;
                    fitH = (int)(kAtlasUsable / imgAspect);
                } else {
                    fitH = kAtlasUsable;
                    fitW = (int)(kAtlasUsable * imgAspect);
                }
                fitW = std::max(1, fitW);
                fitH = std::max(1, fitH);
                std::vector<unsigned char> fitted(fitW * fitH * 4);
                stbir_resize_uint8_linear(data, w, h, 0, fitted.data(), fitW, fitH, 0, STBIR_RGBA);
                stbi_image_free(data);
                std::memset(scaled.data(), 0, kAtlasUsable * kAtlasUsable * 4);
                int offsetX = (kAtlasUsable - fitW) / 2;
                int offsetY = (kAtlasUsable - fitH) / 2;
                for (int fy = 0; fy < fitH; ++fy)
                    std::memcpy(&scaled[((offsetY + fy) * kAtlasUsable + offsetX) * 4],
                                &fitted[fy * fitW * 4], fitW * 4);
            } else {
                if (w != kAtlasUsable || h != kAtlasUsable)
                    stbir_resize_uint8_linear(data, w, h, 0, scaled.data(), kAtlasUsable, kAtlasUsable, 0, STBIR_RGBA);
                else
                    std::memcpy(scaled.data(), data, kAtlasUsable * kAtlasUsable * 4);
                stbi_image_free(data);
            }

            if (rotation != 0) {
                int steps = ((rotation % 360) + 360) / 90 % 4;
                if (steps > 0) {
                    std::vector<unsigned char> rotated(kAtlasUsable * kAtlasUsable * 4);
                    for (int y = 0; y < kAtlasUsable; ++y) {
                        for (int x = 0; x < kAtlasUsable; ++x) {
                            int sx = x, sy = y;
                            for (int s = 0; s < steps; ++s) {
                                int nx = kAtlasUsable - 1 - sy;
                                int ny = sx;
                                sx = nx; sy = ny;
                            }
                            int srcIdx = (y * kAtlasUsable + x) * 4;
                            int dstIdx = (sy * kAtlasUsable + sx) * 4;
                            rotated[dstIdx + 0] = scaled[srcIdx + 0];
                            rotated[dstIdx + 1] = scaled[srcIdx + 1];
                            rotated[dstIdx + 2] = scaled[srcIdx + 2];
                            rotated[dstIdx + 3] = scaled[srcIdx + 3];
                        }
                    }
                    std::memcpy(scaled.data(), rotated.data(), kAtlasUsable * kAtlasUsable * 4);
                }
            }

            int cellX = kAtlasPadding + fi * (kAtlasCell + kAtlasGap) + kAtlasInset;
            int cellY = kAtlasPadding + pi * (kAtlasCell + kAtlasGap) + kAtlasInset;
            for (int y = 0; y < kAtlasUsable; ++y)
                std::memcpy(&atlasPixels[((cellY + y) * kAtlasSize + cellX) * 4],
                            &scaled[y * kAtlasUsable * 4], kAtlasUsable * 4);
        }
    }

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
                if (col >= 0 && col < kOutfitFaceCount && partOverrides[col]) {
                    const FaceOverride& ov = *partOverrides[col];
                    uv.x += ov.offsetX;
                    uv.y += ov.offsetY;
                    uv.x = 0.5f + (uv.x - 0.5f) * ov.scaleX;
                    uv.y = 0.5f + (uv.y - 0.5f) * ov.scaleY;
                    if (ov.rotation >= 45.0f && ov.rotation < 135.0f) {
                        float tmp = uv.x; uv.x = 1.0f - uv.y; uv.y = tmp;
                    } else if (ov.rotation >= 135.0f && ov.rotation < 225.0f) {
                        uv.x = 1.0f - uv.x; uv.y = 1.0f - uv.y;
                    } else if (ov.rotation >= 225.0f && ov.rotation < 315.0f) {
                        float tmp = uv.x; uv.x = uv.y; uv.y = 1.0f - tmp;
                    }
                    uv = glm::clamp(uv, glm::vec2(0.0f), glm::vec2(1.0f));
                }
                mesh.verts[v].uv = atlasUV(row, col, uv);
            }
        }

        for (auto& batch : mesh.batches)
            batch.texture = mData.atlasTexture;
    }

    player.bodyPartMeshes = player.physicalBody.partMeshes;

    player.outfitPartColors.clear();
    for (size_t pi = 0; pi < player.physicalBody.parts.size(); ++pi) {
        auto it = mData.colors.find(player.physicalBody.parts[pi].name);
        player.outfitPartColors.push_back(it != mData.colors.end() ? it->second : glm::vec3(1.0f));
    }

    printf("[OUTFIT] Applied atlas to player\n");
    return true;
}

// ── Apply single texture (replaces OutfitAtlas) ─────────────────────
bool OutfitSystem::applySingleTexture(Player& player, const std::string& texturePath, bool reloadTexture)
{
    GLuint texture = gTextures.getPath(texturePath, reloadTexture);
    if (!texture) return false;

    for (size_t partIndex = 0; partIndex < player.physicalBody.partMeshes.size(); ++partIndex) {
        Mesh& mesh = player.physicalBody.partMeshes[partIndex];
        if (mesh.verts.empty()) continue;

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
