#include "render/outfit-atlas.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <filesystem>

#include <glm/glm.hpp>

#include "devtools/terminal.h"
#include "entities/player.h"
#include "world/texture-store.h"

namespace {
int partRow(const std::string& name)
{
    if (name == "head") return 0;
    if (name == "torso") return 1;
    if (name == "leftArm") return 2;
    if (name == "rightArm") return 3;
    if (name == "leftLeg") return 4;
    if (name == "rightLeg") return 5;
    return -1;
}

int faceColumn(glm::vec3 normal)
{
    glm::vec3 a = glm::abs(normal);
    if (a.z >= a.x && a.z >= a.y) return normal.z >= 0.0f ? 0 : 1;
    if (a.y >= a.x) return normal.y <= 0.0f ? 2 : 3;
    return normal.x <= 0.0f ? 4 : 5;
}

const char* faceName(int column)
{
    static const char* names[] = {"TOP", "BOTTOM", "FRONT", "BACK", "LEFT", "RIGHT"};
    return names[std::clamp(column, 0, 5)];
}

glm::vec2 projectedUV(const Vertex& vertex, int face, glm::vec3 mn, glm::vec3 mx)
{
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

glm::vec2 atlasUV(int row, int column, glm::vec2 local)
{
    constexpr float image = 2000.0f;
    constexpr float padding = 40.0f;
    constexpr float gap = 8.0f;
    constexpr float cell = 313.0f;
    constexpr float inset = 2.0f;
    float x = padding + column * (cell + gap) + inset;
    float y = padding + row * (cell + gap) + inset;
    float usable = cell - inset * 2.0f;
    return {(x + local.x * usable) / image, (y + local.y * usable) / image};
}
}

OutfitAtlas& OutfitAtlas::instance()
{
    static OutfitAtlas atlas;
    return atlas;
}

bool OutfitAtlas::apply(Player& player, const std::string& path, bool reloadTexture)
{
    if (!std::filesystem::exists(path)) {
        Terminal::instance().addLog("[ERROR] outfit PNG not found: " + path);
        return false;
    }

    const unsigned int texture = gTextures.getPath(path, reloadTexture);
    mPath = path;
    mMappings.clear();

    for (size_t partIndex = 0; partIndex < player.physicalBody.parts.size(); ++partIndex) {
        if (partIndex >= player.physicalBody.partMeshes.size())
            break;
        const std::string& name = player.physicalBody.parts[partIndex].name;
        const int row = partRow(name);
        if (row < 0)
            continue;

        Mesh& mesh = player.physicalBody.partMeshes[partIndex];
        if (mesh.verts.empty())
            continue;
        glm::vec3 mn = mesh.verts[0].pos;
        glm::vec3 mx = mesh.verts[0].pos;
        for (const Vertex& vertex : mesh.verts) {
            mn = glm::min(mn, vertex.pos);
            mx = glm::max(mx, vertex.pos);
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
            batch.texture = texture;

        static const char* partNames[] = {"HEAD", "TORSO", "LEFT_ARM", "RIGHT_ARM", "LEFT_LEG", "RIGHT_LEG"};
        for (int face = 0; face < 6; ++face)
            mMappings.push_back(std::string(partNames[row]) + "_" + faceName(face) +
                                " -> row " + std::to_string(row) + ", column " + std::to_string(face));
    }

    player.bodyPartMeshes = player.physicalBody.partMeshes;
    Terminal::instance().addLog("[OK] outfit atlas applied: " + path);
    return true;
}

void OutfitAtlas::printDebug() const
{
    Terminal::instance().addLog("Outfit atlas: " + (mPath.empty() ? std::string("<none>") : mPath));
    for (const std::string& mapping : mMappings)
        Terminal::instance().addLog("  " + mapping);
}
