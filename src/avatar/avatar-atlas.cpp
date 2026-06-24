#include "avatar.h"

#include <algorithm>
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

int partRow(const std::string& name) {
    if (name == "head") return 0;
    if (name == "torso") return 1;
    if (name == "leftArm") return 2;
    if (name == "rightArm") return 3;
    if (name == "leftLeg") return 4;
    if (name == "rightLeg") return 5;
    return -1;
}

int faceColumn(glm::vec3 normal) {
    glm::vec3 a = glm::abs(normal);
    if (a.z >= a.x && a.z >= a.y) return normal.z >= 0.0f ? 0 : 1;
    if (a.y >= a.x) return normal.y <= 0.0f ? 2 : 3;
    return normal.x <= 0.0f ? 4 : 5;
}

const char* faceName(int column) {
    static const char* names[] = {"top", "bottom", "front", "back", "left", "right"};
    return names[std::clamp(column, 0, 5)];
}

glm::vec2 projectedUV(const Vertex& vertex, int face, glm::vec3 mn, glm::vec3 mx) {
    glm::vec3 size = glm::max(mx - mn, glm::vec3(0.0001f));
    glm::vec2 uv;
    if (face <= 1)
        uv = {(vertex.pos.x - mn.x) / size.x, (vertex.pos.y - mn.y) / size.y};
    else if (face <= 3)
        uv = {(vertex.pos.x - mn.x) / size.x, (vertex.pos.z - mn.z) / size.z};
    else
        uv = {(vertex.pos.y - mn.y) / size.y, (vertex.pos.z - mn.z) / size.z};
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

}

bool AvatarSystem::buildAtlas(Player& player, bool reloadTextures) {
    if (!mAvatar.advancedMode)
        const_cast<AvatarDefinition&>(mAvatar).expandSimple();

    std::vector<unsigned char> atlasPixels(ATLAS_SIZE * ATLAS_SIZE * 4, 128);
    std::vector<unsigned char> scaled(USABLE * USABLE * 4);

    auto blitToCell = [&](int row, int col, const std::string& path) {
        if (path.empty()) return;
        std::string fullPath = resolvePath(path);
        if (!std::filesystem::exists(fullPath)) {
            Terminal::instance().addLog("[AVATAR] Missing texture: " + fullPath);
            return;
        }

        int w, h, n;
        unsigned char* data = stbi_load(fullPath.c_str(), &w, &h, &n, 4);
        if (!data) {
            Terminal::instance().addLog("[AVATAR] Failed to load: " + fullPath);
            return;
        }

        if (w != USABLE || h != USABLE) {
            stbir_resize_uint8_linear(data, w, h, 0, scaled.data(), USABLE, USABLE, 0, STBIR_RGBA);
        } else {
            std::memcpy(scaled.data(), data, USABLE * USABLE * 4);
        }
        stbi_image_free(data);

        int cellX = PADDING + col * (CELL_SIZE + GAP) + INSET;
        int cellY = PADDING + row * (CELL_SIZE + GAP) + INSET;
        for (int y = 0; y < USABLE; ++y) {
            std::memcpy(
                &atlasPixels[((cellY + y) * ATLAS_SIZE + cellX) * 4],
                &scaled[y * USABLE * 4],
                USABLE * 4
            );
        }
    };

    const std::string parts[] = {"head", "torso", "leftArm", "rightArm", "leftLeg", "rightLeg"};
    const std::string faces[] = {"top", "bottom", "front", "back", "left", "right"};
    AvatarPartFaces AvatarDefinition::*partPtrs[] = {
        &AvatarDefinition::head, &AvatarDefinition::torso,
        &AvatarDefinition::leftArm, &AvatarDefinition::rightArm,
        &AvatarDefinition::leftLeg, &AvatarDefinition::rightLeg
    };

    for (int pi = 0; pi < 6; ++pi) {
        const AvatarPartFaces& part = mAvatar.*partPtrs[pi];
        for (int fi = 0; fi < 6; ++fi) {
            blitToCell(pi, fi, part.byName(faces[fi]));
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
            const int face = faceColumn(normal);
            for (size_t v = i; v < i + 3; ++v)
                mesh.verts[v].uv = atlasUV(row, face, projectedUV(mesh.verts[v], face, mn, mx));
        }

        for (Mesh::Batch& batch : mesh.batches)
            batch.texture = mAtlasTexture;
    }

    player.bodyPartMeshes = player.physicalBody.partMeshes;
    Terminal::instance().addLog("[AVATAR] Applied atlas to player");
    return true;
}
