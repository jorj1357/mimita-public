#include "cosmetic-system.h"
#include "avatar.h"
#include "entities/player.h"
#include "map/map_loader.h"
#include "map/map_common.h"
#include "renderer/renderer.h"
#include "gui/ui-system.h"
#include "world/texture-store.h"

#include <cstdio>
#include <algorithm>
#include <filesystem>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/type_ptr.hpp>

namespace {

const std::string& cosmeticGlb(const CosmeticSlot& slot)
{
    return slot.glb.empty() ? slot.choice : slot.glb;
}

const std::string& cosmeticAnchor(const CosmeticSlot& slot)
{
    return slot.anchorPart.empty() ? slot.attachTo : slot.anchorPart;
}

std::string cosmeticTexturePath(const CosmeticSlot& slot)
{
    if (slot.texture.image.empty())
        return {};
    if (slot.texture.image.find('/') != std::string::npos ||
        slot.texture.image.find('\\') != std::string::npos)
        return slot.texture.image;
    return "assets/avatars/" + AvatarSystem::instance().currentName() + "/" + slot.texture.image;
}

} // namespace



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

    std::string path = (choice.find('/') != std::string::npos ||
                        choice.find('\\') != std::string::npos)
        ? choice
        : "assets/objects/things/cosmetics/" + choice;
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
        const std::string& choice = cosmeticGlb(slot);
        if (!slot.enabled || choice.empty() || choice == "none")
            continue;
        loadCosmeticGLB(choice);
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

    const GLboolean blendWas = glIsEnabled(GL_BLEND);
    GLint blendSrcWas = GL_SRC_ALPHA;
    GLint blendDstWas = GL_ONE_MINUS_SRC_ALPHA;
    glGetIntegerv(GL_BLEND_SRC_ALPHA, &blendSrcWas);
    glGetIntegerv(GL_BLEND_DST_ALPHA, &blendDstWas);

    for (const auto& slot : cosmetics)
    {
        const std::string& choice = cosmeticGlb(slot);
        if (!slot.enabled || choice.empty() || choice == "none")
            continue;

        const CosmeticInstance* inst = find(choice);
        if (!inst)
            continue;

        const Mesh& mesh = inst->mesh;
        if (mesh.verts.empty() || mesh.batches.empty())
            continue;

        // Determine attachment transform
        glm::mat4 attachTransform;
        std::string attachTarget = cosmeticAnchor(slot);
        if (attachTarget.empty())
            attachTarget = "head";
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
        GLint tintLoc = glGetUniformLocation(currentShader, "uTint");
        GLint textureLoc = glGetUniformLocation(currentShader, "uTex");
        GLint textureEnabledLoc = glGetUniformLocation(currentShader, "uCosmeticTextureEnabled");
        GLint offsetLoc = glGetUniformLocation(currentShader, "uCosmeticUvOffset");
        GLint scaleLoc = glGetUniformLocation(currentShader, "uCosmeticUvScale");
        GLint rotationLoc = glGetUniformLocation(currentShader, "uCosmeticUvRotation");
        GLint brightnessLoc = glGetUniformLocation(currentShader, "uCosmeticBrightness");
        GLint opacityLoc = glGetUniformLocation(currentShader, "uCosmeticOpacity");
        GLint useColorLoc = glGetUniformLocation(currentShader, "uUseColor");
        if (modelLoc >= 0)
            glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(model));
        if (tintLoc >= 0) {
            const glm::vec3 tint = slot.texture.color == glm::vec3(1.0f)
                ? slot.color : slot.texture.color;
            glUniform3f(tintLoc, tint.x, tint.y, tint.z);
        }
        if (textureLoc >= 0)
            glUniform1i(textureLoc, 0);
        if (useColorLoc >= 0)
            glUniform1i(useColorLoc, 0);
        if (textureEnabledLoc >= 0)
            glUniform1i(textureEnabledLoc, slot.texture.image.empty() ? 0 : 1);
        if (offsetLoc >= 0)
            glUniform2f(offsetLoc, slot.texture.offsetX / 1000.0f,
                        slot.texture.offsetY / 1000.0f);
        if (scaleLoc >= 0)
            glUniform2f(scaleLoc, std::max(slot.texture.scaleX, 0.0001f),
                        std::max(slot.texture.scaleY, 0.0001f));
        if (rotationLoc >= 0)
            glUniform1f(rotationLoc, slot.texture.rotation);
        if (brightnessLoc >= 0)
            glUniform1f(brightnessLoc, slot.texture.brightness);
        if (opacityLoc >= 0)
            glUniform1f(opacityLoc, slot.texture.opacity);

        const std::string texturePath = cosmeticTexturePath(slot);
        const GLuint overrideTexture = texturePath.empty()
            ? 0 : gTextures.getPath(texturePath);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        // Upload and render each batch
        static GLuint cosmeticVAO = 0;
        static GLuint cosmeticVBO = 0;
        static const Mesh* lastMesh = nullptr;

        if (!cosmeticVAO)
            glGenVertexArrays(1, &cosmeticVAO);

        if (&mesh != lastMesh)
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

            lastMesh = &mesh;
        }

        glBindVertexArray(cosmeticVAO);

        for (const auto& batch : mesh.batches)
        {
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, overrideTexture ? overrideTexture : batch.texture);
            glDrawArrays(GL_TRIANGLES, (GLint)batch.first, (GLsizei)batch.count);
        }

        glBindVertexArray(0);
    }

    if (blendWas)
        glEnable(GL_BLEND);
    else
        glDisable(GL_BLEND);
    glBlendFunc(blendSrcWas, blendDstWas);
}
