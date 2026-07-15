#include "cosmetic-system.h"
#include "avatar.h"
#include "entities/player.h"
#include "map/map_loader.h"
#include "map/map_common.h"
#include "renderer/renderer.h"
#include "gui/ui-system.h"

#include <cstdio>
#include <filesystem>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/type_ptr.hpp>



static CosmeticSystem* gInstance = nullptr;

CosmeticSystem& CosmeticSystem::instance()
{
    if (!gInstance)
        gInstance = new CosmeticSystem();
    return *gInstance;
}

void CosmeticSystem::clear()
{
    mCosmetics.clear();
}

bool CosmeticSystem::loadCosmeticGLB(const std::string& choice)
{
    if (mCosmetics.find(choice) != mCosmetics.end())
        return mCosmetics[choice].loaded;

    CosmeticInstance inst;
    inst.choice = choice;

    std::string path = "assets/objects/things/cosmetics/" + choice;
    if (path.rfind(".glb") == std::string::npos)
        path += ".glb";

    if (!std::filesystem::exists(path))
    {
        printf("[COSMETIC] file not found: %s\n", path.c_str());
        mCosmetics[choice] = inst;
        return false;
    }

    inst.mesh = loadGLB(path);
    inst.loaded = !inst.mesh.verts.empty();

    printf("[COSMETIC] loaded choice=%s path=%s verts=%zu batches=%zu\n",
           choice.c_str(), path.c_str(), inst.mesh.verts.size(), inst.mesh.batches.size());

    mCosmetics[choice] = inst;
    return inst.loaded;
}

void CosmeticSystem::loadCosmetics(const std::vector<CosmeticSlot>& slots)
{
    for (const auto& slot : slots)
    {
        if (slot.choice.empty() || slot.choice == "none")
            continue;
        loadCosmeticGLB(slot.choice);
    }
}

std::vector<std::string> CosmeticSystem::scanAvailableCosmetics() const
{
    std::vector<std::string> result;
    std::string dir = "assets/objects/things/cosmetics/";
    if (!std::filesystem::exists(dir))
        return result;

    for (const auto& entry : std::filesystem::directory_iterator(dir))
    {
        if (entry.path().extension() == ".glb")
            result.push_back(entry.path().filename().string());
    }
    return result;
}

const CosmeticInstance* CosmeticSystem::find(const std::string& choice) const
{
    auto it = mCosmetics.find(choice);
    if (it != mCosmetics.end() && it->second.loaded)
        return &it->second;
    return nullptr;
}

void CosmeticSystem::renderCosmetics(const Player& player) const
{
    const auto& cosmetics = player.getCosmetics();
    if (cosmetics.empty())
        return;

    // Get currently active shader (set by the rendering pipeline before Player::render)
    GLint currentShader = 0;
    glGetIntegerv(GL_CURRENT_PROGRAM, &currentShader);
    if (!currentShader)
        return;

    // Find attachment point transforms from player skeleton
    auto findNodeTransform = [&](const std::string& name) -> glm::mat4 {
        for (const auto& part : player.physicalBody.parts)
        {
            if (part.name == name)
                return part.worldTransform;
        }
        for (const auto& node : player.perfectPoseSkeleton.nodes)
        {
            if (node.name == name)
                return node.worldTransform;
        }
        return glm::mat4(1.0f);
    };

    glm::quat rootRot(glm::vec3(0, glm::radians(player.yaw), 0));
    glm::mat4 rootTransform = transformMatrix(player.pos, rootRot);

    for (const auto& slot : cosmetics)
    {
        if (slot.choice.empty() || slot.choice == "none")
            continue;

        const CosmeticInstance* inst = find(slot.choice);
        if (!inst)
            continue;

        const Mesh& mesh = inst->mesh;
        if (mesh.verts.empty() || mesh.batches.empty())
            continue;

        // Determine attachment transform
        glm::mat4 attachTransform;
        std::string attachTarget = slot.attachTo.empty() ? "head" : slot.attachTo;
        if (attachTarget == "root")
            attachTransform = rootTransform;
        else
            attachTransform = findNodeTransform(attachTarget);

        // Build cosmetic model matrix: attach * translate * rotate * scale
        glm::mat4 model = attachTransform;
        model = glm::translate(model, slot.offset);
        model = glm::rotate(model, glm::radians(slot.rotation.x), glm::vec3(1, 0, 0));
        model = glm::rotate(model, glm::radians(slot.rotation.y), glm::vec3(0, 1, 0));
        model = glm::rotate(model, glm::radians(slot.rotation.z), glm::vec3(0, 0, 1));
        model = glm::scale(model, slot.scale);

        GLint modelLoc = glGetUniformLocation(currentShader, "model");
        GLint colorLoc = glGetUniformLocation(currentShader, "color");
        if (modelLoc >= 0)
            glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(model));
        if (colorLoc >= 0)
            glUniform3f(colorLoc, slot.color.x, slot.color.y, slot.color.z);

        // Upload and render each batch
        static GLuint cosmeticVAO = 0;
        static GLuint cosmeticVBO = 0;
        static size_t lastVerts = 0;

        if (!cosmeticVAO)
            glGenVertexArrays(1, &cosmeticVAO);

        if (mesh.verts.size() != lastVerts)
        {
            if (!cosmeticVBO)
                glGenBuffers(1, &cosmeticVBO);

            glBindVertexArray(cosmeticVAO);
            glBindBuffer(GL_ARRAY_BUFFER, cosmeticVBO);
            glBufferData(GL_ARRAY_BUFFER, mesh.verts.size() * sizeof(Vertex), mesh.verts.data(), GL_STATIC_DRAW);

            glEnableVertexAttribArray(0);
            glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, pos));
            glEnableVertexAttribArray(1);
            glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, normal));
            glEnableVertexAttribArray(2);
            glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, uv));

            lastVerts = mesh.verts.size();
        }

        glBindVertexArray(cosmeticVAO);

        for (const auto& batch : mesh.batches)
        {
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, batch.texture);
            glDrawArrays(GL_TRIANGLES, (GLint)batch.first, (GLsizei)batch.count);
        }

        glBindVertexArray(0);
    }
}
