#include "avatar.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <numeric>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glad/glad.h>

#include "entities/player.h"
#include "world/texture-store.h"
#include "devtools/terminal.h"
#include "map/map_common.h"
#include "stb_image.h"
#include "stb_image_resize2.h"

namespace {

constexpr int ATLAS_SIZE = 2000;
constexpr int CELL_SIZE = 313;
constexpr int GAP = 8;
constexpr int PADDING = 40;
constexpr int INSET = 2;
constexpr int USABLE = CELL_SIZE - INSET * 2;

// Default rotation applied to all avatar face textures.
// Set to 0 since projectedUV now uses consistent face-relative coordinates.
// Previously was 270 to compensate for inconsistent UV axes across faces.
constexpr float DEFAULT_TEXTURE_ROTATION = 0.0f;

// ── Coordinate system (Y-up, Z-forward GLTF convention) ──────────────
//   +Y = top        -Y = bottom
//   +Z = front      -Z = back
//   +X = right      -X = left
//
// faceColumn() maps mesh triangle normals to a canonical column index:
//   0 = top, 1 = bottom, 2 = front, 3 = back, 4 = left, 5 = right
//
// faceToAtlasColumn() remaps the mesh-relative column to the atlas column.
// Empirical testing with a debug avatar (red=front, green=back, blue=left,
// yellow=right, white=top, black=bottom) revealed that the mesh's X and Z
// normals are swapped relative to the logical face names. The side faces
// are rotated 90 degrees around Y. Top and bottom are correct.
//
// Observed mapping (before fix):
//   JSON head_front → mesh right face (+X normal → column 5 → atlas right)
//   JSON head_right → mesh front face (+Z normal → column 2 → atlas front)
//   JSON head_back  → mesh left face  (-X normal → column 4 → atlas left)
//   JSON head_left  → mesh back face  (-Z normal → column 3 → atlas back)
//   JSON head_top   → mesh top face   (+Y normal → column 0 → atlas top)  ✓
//   JSON head_bottom→ mesh bottom face(-Y normal → column 1 → atlas bottom) ✓
//
// Fix: remap side columns so that column 2 (from +Z normal) samples atlas
// column 5 (right), column 5 (from +X normal) samples atlas column 2 (front),
// etc. This produces the correct visual mapping.

int partRow(const std::string& name) {
    if (name == "head") return 0;
    if (name == "torso") return 1;
    if (name == "leftArm") return 2;
    if (name == "rightArm") return 3;
    if (name == "leftLeg") return 4;
    if (name == "rightLeg") return 5;
    return -1;
}

// Maps mesh triangle normal direction to a canonical face column (0-5).
// This function captures what the MESH considers top/bottom/front/back/etc.
int faceColumn(glm::vec3 normal) {
    glm::vec3 a = glm::abs(normal);
    if (a.y >= a.x && a.y >= a.z) return normal.y >= 0.0f ? 0 : 1;
    if (a.z >= a.x) return normal.z >= 0.0f ? 2 : 3;
    return normal.x <= 0.0f ? 4 : 5;
}

// Remaps the mesh-relative face column to the actual atlas column index.
// Top (0) and bottom (1) pass through unchanged.
// Side faces (2-5) are rotated 90 degrees around Y to match the observed
// physical orientation of the character model.
int faceToAtlasColumn(int face) {
    if (face <= 1) return face; // top/bottom: no remap
    // front(2)→right(5), back(3)→left(4), left(4)→back(3), right(5)→front(2)
    const int sideMap[] = {5, 4, 3, 2};
    return sideMap[face - 2];
}

const char* faceName(int column) {
    static const char* names[] = {"top", "bottom", "front", "back", "left", "right"};
    return names[std::clamp(column, 0, 5)];
}

glm::vec2 projectedUV(const Vertex& vertex, int face, glm::vec3 mn, glm::vec3 mx) {
    glm::vec3 size = glm::max(mx - mn, glm::vec3(0.0001f));
    glm::vec2 uv;
    // Each face uses consistent local axes: U along the horizontal, V along the vertical.
    // Top/bottom (0,1): horizontal plane = XZ, front/back (2,3): vertical plane = XY, left/right (4,5): vertical plane = ZY
    if (face <= 1)
        uv = {(vertex.pos.x - mn.x) / size.x, (vertex.pos.z - mn.z) / size.z};
    else if (face <= 3)
        uv = {(vertex.pos.x - mn.x) / size.x, (vertex.pos.y - mn.y) / size.y};
    else
        uv = {(vertex.pos.z - mn.z) / size.z, (vertex.pos.y - mn.y) / size.y};
    if (face == 1 || face == 3 || face == 5)
        uv.x = 1.0f - uv.x;
    return glm::clamp(uv, glm::vec2(0.0f), glm::vec2(1.0f));
}

glm::vec2 atlasUV(int row, int column, glm::vec2 local) {
    float x = (float)(PADDING + column * (CELL_SIZE + GAP) + INSET);
    float y = (float)(PADDING + row * (CELL_SIZE + GAP) + INSET);
    float usable = (float)USABLE;
    return {(x + local.x * usable) / (float)ATLAS_SIZE,
            (y + local.y * usable) / (float)ATLAS_SIZE};
}

int faceIndexForName(const std::string& name) {
    if (name == "top") return 0;
    if (name == "bottom") return 1;
    if (name == "front") return 2;
    if (name == "back") return 3;
    if (name == "left") return 4;
    if (name == "right") return 5;
    return -1;
}

// ── Per-face texture transforms ──────────────────────────────────────
static void applyHSB(unsigned char* pixel, float hueShift, float saturation, float brightness) {
    float r = pixel[0] / 255.0f;
    float g = pixel[1] / 255.0f;
    float b = pixel[2] / 255.0f;

    if (hueShift != 0.0f) {
        float angle = glm::radians(hueShift);
        float c = std::cos(angle);
        float s = std::sin(angle);
        float nr = r * (0.213f + c * 0.787f - s * 0.213f) + g * (0.715f - c * 0.715f - s * 0.715f) + b * (0.072f - c * 0.072f + s * 0.928f);
        float ng = r * (0.213f - c * 0.213f + s * 0.143f) + g * (0.715f + c * 0.285f + s * 0.140f) + b * (0.072f - c * 0.072f - s * 0.283f);
        float nb = r * (0.213f - c * 0.213f - s * 0.787f) + g * (0.715f - c * 0.715f + s * 0.715f) + b * (0.072f + c * 0.928f + s * 0.072f);
        r = nr; g = ng; b = nb;
    }

    if (saturation != 0.0f) {
        float satMul = 1.0f + saturation / 10.0f;
        float gray = 0.299f * r + 0.587f * g + 0.114f * b;
        r = gray + (r - gray) * satMul;
        g = gray + (g - gray) * satMul;
        b = gray + (b - gray) * satMul;
    }

    if (brightness != 0.0f) {
        float briMul = 1.0f + brightness / 10.0f;
        r *= briMul;
        g *= briMul;
        b *= briMul;
    }

    pixel[0] = (unsigned char)std::clamp(r * 255.0f, 0.0f, 255.0f);
    pixel[1] = (unsigned char)std::clamp(g * 255.0f, 0.0f, 255.0f);
    pixel[2] = (unsigned char)std::clamp(b * 255.0f, 0.0f, 255.0f);
}

static void rotateImage(std::vector<unsigned char>& pixels, int size, float rotationDeg) {
    float rot = fmod(rotationDeg, 360.0f);
    if (rot < 0.0f) rot += 360.0f;
    if (rot < 0.001f || rot > 359.999f) return;

    float rad = glm::radians(rot);
    float cosA = std::cos(rad);
    float sinA = std::sin(rad);
    float half = size * 0.5f;

    std::vector<unsigned char> tmp = pixels;
    for (int y = 0; y < size; ++y) {
        for (int x = 0; x < size; ++x) {
            float sx = (x - half) * cosA - (y - half) * sinA + half;
            float sy = (x - half) * sinA + (y - half) * cosA + half;

            // Bilinear interpolation with clamp-to-edge
            int ix = (int)sx;
            int iy = (int)sy;
            float fx = sx - ix;
            float fy = sy - iy;
            if (ix < 0) { ix = 0; fx = 0.0f; }
            if (iy < 0) { iy = 0; fy = 0.0f; }
            if (ix >= size - 1) { ix = size - 2; fx = 1.0f; }
            if (iy >= size - 1) { iy = size - 2; fy = 1.0f; }

            const unsigned char* p00 = &tmp[(iy * size + ix) * 4];
            const unsigned char* p10 = &tmp[(iy * size + (ix + 1)) * 4];
            const unsigned char* p01 = &tmp[((iy + 1) * size + ix) * 4];
            const unsigned char* p11 = &tmp[((iy + 1) * size + (ix + 1)) * 4];

            unsigned char* dst = &pixels[(y * size + x) * 4];
            for (int c = 0; c < 4; ++c) {
                float v = (1.0f - fx) * (1.0f - fy) * p00[c]
                        + fx * (1.0f - fy) * p10[c]
                        + (1.0f - fx) * fy * p01[c]
                        + fx * fy * p11[c];
                dst[c] = (unsigned char)std::clamp(std::round(v), 0.0f, 255.0f);
            }
        }
    }
}

// Apply scale and offset by sampling a sub-rect from the source image.
// Scale operates around the center of the face. Offset shifts the sampling window.
// Applied BEFORE rotation and HSB in the rendering pipeline.
static void applyScaleAndOffset(std::vector<unsigned char>& pixels,
                                 int srcW, int srcH,
                                 float scaleX, float scaleY,
                                 float offsetX, float offsetY)
{
    if (scaleX < 0.001f) scaleX = 0.001f;
    if (scaleY < 0.001f) scaleY = 0.001f;

    int sampW = std::max(1, (int)(srcW / scaleX));
    int sampH = std::max(1, (int)(srcH / scaleY));

    int cx = srcW / 2 + (int)offsetX;
    int cy = srcH / 2 + (int)offsetY;

    int sx = cx - sampW / 2;
    int sy = cy - sampH / 2;
    if (sx < 0) sx = 0;
    if (sy < 0) sy = 0;
    if (sx + sampW > srcW) sampW = srcW - sx;
    if (sy + sampH > srcH) sampH = srcH - sy;
    if (sampW <= 0 || sampH <= 0) {
        pixels.assign(USABLE * USABLE * 4, 0);
        return;
    }

    if (sx == 0 && sy == 0 && sampW == srcW && sampH == srcH) {
        if (srcW != USABLE || srcH != USABLE) {
            std::vector<unsigned char> resized(USABLE * USABLE * 4);
            stbir_resize_uint8_linear(pixels.data(), srcW, srcH, 0, resized.data(), USABLE, USABLE, 0, STBIR_RGBA);
            pixels = std::move(resized);
        }
        return;
    }

    std::vector<unsigned char> cropped(sampW * sampH * 4);
    for (int y = 0; y < sampH; ++y)
        std::memcpy(&cropped[y * sampW * 4], &pixels[(sy + y) * srcW * 4 + sx * 4], sampW * 4);

    if (sampW == USABLE && sampH == USABLE) {
        pixels.resize(USABLE * USABLE * 4);
        std::memcpy(pixels.data(), cropped.data(), USABLE * USABLE * 4);
    } else {
        std::vector<unsigned char> resized(USABLE * USABLE * 4);
        stbir_resize_uint8_linear(cropped.data(), sampW, sampH, 0, resized.data(), USABLE, USABLE, 0, STBIR_RGBA);
        pixels = std::move(resized);
    }
}

static void applyCrop(std::vector<unsigned char>& pixels, int srcW, int srcH) {
    if (srcW == USABLE && srcH == USABLE) return;
    float scale = std::max((float)USABLE / srcW, (float)USABLE / srcH);
    int newW = (int)(srcW * scale);
    int newH = (int)(srcH * scale);
    std::vector<unsigned char> scaled(newW * newH * 4);
    stbir_resize_uint8_linear(pixels.data(), srcW, srcH, 0, scaled.data(), newW, newH, 0, STBIR_RGBA);

    int cropX = (newW - USABLE) / 2;
    int cropY = (newH - USABLE) / 2;
    std::vector<unsigned char> cropped(USABLE * USABLE * 4, 0);
    for (int y = 0; y < USABLE && cropY + y < newH; ++y)
        for (int x = 0; x < USABLE && cropX + x < newW; ++x)
            for (int c = 0; c < 4; ++c)
                cropped[(y * USABLE + x) * 4 + c] = scaled[((cropY + y) * newW + (cropX + x)) * 4 + c];
    pixels.assign(cropped.begin(), cropped.end());
}

} // anonymous namespace

bool AvatarSystem::buildAtlas(Player& player, bool reloadTextures) {
    if (!mAvatar.advancedMode)
        const_cast<AvatarDefinition&>(mAvatar).expandSimple();

    std::vector<unsigned char> atlasPixels(ATLAS_SIZE * ATLAS_SIZE * 4, 0);

    const std::string parts[] = {"head", "torso", "leftArm", "rightArm", "leftLeg", "rightLeg"};
    const std::string faces[] = {"top", "bottom", "front", "back", "left", "right"};
    FaceVector AvatarDefinition::*partPtrs[] = {
        &AvatarDefinition::head, &AvatarDefinition::torso,
        &AvatarDefinition::leftArm, &AvatarDefinition::rightArm,
        &AvatarDefinition::leftLeg, &AvatarDefinition::rightLeg
    };

    for (int pi = 0; pi < 6; ++pi) {
        const FaceVector& part = mAvatar.*partPtrs[pi];
        for (int fi = 0; fi < 6; ++fi) {
            const FaceSettings& fs = part.byName(faces[fi]);
            std::string path = fs.texture;
            if (path.empty()) continue;

            std::string fullPath = resolvePath(path);
            if (!std::filesystem::exists(fullPath)) {
                Terminal::instance().addLog("[AVATAR] Missing texture: " + fullPath + " (face " + parts[pi] + "/" + faces[fi] + ")");
                continue;
            }

            int w, h, n;
            unsigned char* data = stbi_load(fullPath.c_str(), &w, &h, &n, 4);
            if (!data) {
                Terminal::instance().addLog("[AVATAR] Failed to load: " + fullPath);
                continue;
            }

            bool hasScaleOffset = (fs.transform.scaleX != 1.0f || fs.transform.scaleY != 1.0f ||
                                   fs.transform.offsetX != 0.0f || fs.transform.offsetY != 0.0f);

            // Rendering pipeline: load → scale+offset → resize → rotation → color/transparency → HSB
            std::vector<unsigned char> cellPixels;
            if (hasScaleOffset) {
                // Apply scale+offset on full-resolution data, then resize to USABLE
                cellPixels.assign(data, data + w * h * 4);
                stbi_image_free(data);
                applyScaleAndOffset(cellPixels, w, h,
                                    fs.transform.scaleX, fs.transform.scaleY,
                                    fs.transform.offsetX, fs.transform.offsetY);
            } else if (fs.transform.stretchMode == 1) {
                // Crop mode: preserve aspect ratio, fill cell
                cellPixels.assign(data, data + w * h * 4);
                stbi_image_free(data);
                applyCrop(cellPixels, w, h);
            } else {
                // Stretch mode: resize directly to fill cell
                std::vector<unsigned char> scaled(USABLE * USABLE * 4);
                if (w != USABLE || h != USABLE)
                    stbir_resize_uint8_linear(data, w, h, 0, scaled.data(), USABLE, USABLE, 0, STBIR_RGBA);
                else
                    std::memcpy(scaled.data(), data, USABLE * USABLE * 4);
                stbi_image_free(data);
                cellPixels = std::move(scaled);
            }

            // Apply default 90-degree CCW rotation, then per-face rotation on top.
            // Rotation is applied before color/transparency so rotation works on
            // the original pixel values and color/transparency are the final pass.
            float totalRotation = DEFAULT_TEXTURE_ROTATION;
            if (fs.transform.rotation != 0.0f)
                totalRotation = fmod(totalRotation + fs.transform.rotation, 360.0f);
            if (totalRotation != 0.0f)
                rotateImage(cellPixels, USABLE, totalRotation);

            if (totalRotation != 0.0f || fs.transform.scaleX != 1.0f || fs.transform.scaleY != 1.0f ||
                fs.transform.offsetX != 0.0f || fs.transform.offsetY != 0.0f) {
                printf("[Avatar] %s/%s rotation=%.1f° scale=%.2f,%.2f offset=%.0f,%.0f\n",
                       parts[pi].c_str(), faces[fi].c_str(),
                       totalRotation, fs.transform.scaleX, fs.transform.scaleY,
                       fs.transform.offsetX, fs.transform.offsetY);
            }

            // Apply per-face color multiplier (RGB multiply) and transparency (alpha multiply)
            float opacity = 1.0f - std::clamp(fs.transform.transparency, 0.0f, 1.0f);
            if (fs.transform.color != glm::vec3(1.0f) || opacity < 1.0f) {
                for (int py = 0; py < USABLE; ++py)
                    for (int px = 0; px < USABLE; ++px) {
                        unsigned char* p = &cellPixels[(py * USABLE + px) * 4];
                        p[0] = (unsigned char)std::clamp(p[0] * fs.transform.color.r, 0.0f, 255.0f);
                        p[1] = (unsigned char)std::clamp(p[1] * fs.transform.color.g, 0.0f, 255.0f);
                        p[2] = (unsigned char)std::clamp(p[2] * fs.transform.color.b, 0.0f, 255.0f);
                        if (opacity < 1.0f)
                            p[3] = (unsigned char)((float)p[3] * opacity);
                    }
            }

            if (fs.transform.hueShift != 0.0f || fs.transform.saturation != 0.0f || fs.transform.brightness != 0.0f) {
                for (int py = 0; py < USABLE; ++py)
                    for (int px = 0; px < USABLE; ++px)
                        applyHSB(&cellPixels[(py * USABLE + px) * 4],
                                 fs.transform.hueShift, fs.transform.saturation, fs.transform.brightness);
            }

            // Blit to atlas (textures are stored in logical column order: top, bottom, front, back, left, right)
            int cellX = PADDING + fi * (CELL_SIZE + GAP) + INSET;
            int cellY = PADDING + pi * (CELL_SIZE + GAP) + INSET;
            for (int y = 0; y < USABLE; ++y) {
                std::memcpy(
                    &atlasPixels[((cellY + y) * ATLAS_SIZE + cellX) * 4],
                    &cellPixels[y * USABLE * 4],
                    USABLE * 4
                );
            }
        }
    }

    if (mAtlasTexture) {
        glDeleteTextures(1, &mAtlasTexture);
        mAtlasTexture = 0;
    }

    glGenTextures(1, &mAtlasTexture);
    glBindTexture(GL_TEXTURE_2D, mAtlasTexture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, ATLAS_SIZE, ATLAS_SIZE, 0, GL_RGBA, GL_UNSIGNED_BYTE, atlasPixels.data());
    glGenerateMipmap(GL_TEXTURE_2D);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 4);

    Terminal::instance().addLog("[AVATAR] Built atlas texture for: " + mAvatarName);
    return true;
}

bool AvatarSystem::applyAtlasToPlayer(Player& player) {
    if (!mAtlasTexture) return false;

    for (size_t partIndex = 0; partIndex < player.physicalBody.parts.size(); ++partIndex) {
        if (partIndex >= player.physicalBody.partMeshes.size())
            break;
        const std::string& name = player.physicalBody.parts[partIndex].name;
        const int row = partRow(name);
        if (row < 0) continue;

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
            // Get the mesh-relative face column from the triangle normal
            const int meshFace = faceColumn(normal);
            // Remap to atlas column to correct for mesh coordinate system offset
            const int atlasCol = faceToAtlasColumn(meshFace);
            for (size_t v = i; v < i + 3; ++v)
                mesh.verts[v].uv = atlasUV(row, atlasCol, projectedUV(mesh.verts[v], meshFace, mn, mx));
        }

        for (Mesh::Batch& batch : mesh.batches)
            batch.texture = mAtlasTexture;
    }

    player.bodyPartMeshes = player.physicalBody.partMeshes;
    Terminal::instance().addLog("[AVATAR] Applied atlas to player");
    return true;
}
