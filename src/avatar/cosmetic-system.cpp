#include "cosmetic-system.h"
#include "game/gl-resource-bridge-module.h"
#include "avatar.h"
#include "entities/player.h"
#include "map/map_loader.h"
#include "map/map_common.h"
#include "renderer/renderer.h"
#include "gui/ui-system.h"
#include "world/texture-store.h"
#include "render/render-command.h"
#include "render/world-shader.h"

namespace r = mimita::game::render;

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

    // Cosmetics share the game-owned world shader.
    if (!r::worldShader().ensureLoaded())
        return;
    const r::Uniform modelLoc = r::worldShader().uniform("model");
    const r::Uniform tintLoc = r::worldShader().uniform("uTint");
    const r::Uniform textureLoc = r::worldShader().uniform("uTex");
    const r::Uniform textureEnabledLoc = r::worldShader().uniform("uCosmeticTextureEnabled");
    const r::Uniform offsetLoc = r::worldShader().uniform("uCosmeticUvOffset");
    const r::Uniform scaleLoc = r::worldShader().uniform("uCosmeticUvScale");
    const r::Uniform rotationLoc = r::worldShader().uniform("uCosmeticUvRotation");
    const r::Uniform brightnessLoc = r::worldShader().uniform("uCosmeticBrightness");
    const r::Uniform opacityLoc = r::worldShader().uniform("uCosmeticOpacity");
    const r::Uniform useColorLoc = r::worldShader().uniform("uUseColor");

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

    r::pushState();

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

        if (modelLoc >= 0)
            r::uniformMatrix4fv(modelLoc, 1, glm::value_ptr(model));
        if (tintLoc >= 0) {
            const glm::vec3 tint = slot.texture.color == glm::vec3(1.0f)
                ? slot.color : slot.texture.color;
            r::uniform3f(tintLoc, tint.x, tint.y, tint.z);
        }
        if (textureLoc >= 0)
            r::uniform1i(textureLoc, 0);
        if (useColorLoc >= 0)
            r::uniform1i(useColorLoc, 0);
        if (textureEnabledLoc >= 0)
            r::uniform1i(textureEnabledLoc, slot.texture.image.empty() ? 0 : 1);
        if (offsetLoc >= 0)
            r::uniform2f(offsetLoc, slot.texture.offsetX / 1000.0f,
                         slot.texture.offsetY / 1000.0f);
        if (scaleLoc >= 0)
            r::uniform2f(scaleLoc, std::max(slot.texture.scaleX, 0.0001f),
                         std::max(slot.texture.scaleY, 0.0001f));
        if (rotationLoc >= 0)
            r::uniform1f(rotationLoc, slot.texture.rotation);
        if (brightnessLoc >= 0)
            r::uniform1f(brightnessLoc, slot.texture.brightness);
        if (opacityLoc >= 0)
            r::uniform1f(opacityLoc, slot.texture.opacity);

        const std::string texturePath = cosmeticTexturePath(slot);
        const std::uint32_t overrideTexture = texturePath.empty()
            ? 0 : gTextures.getPath(texturePath);
        r::enable(r::Capability::Blend);
        r::blendFunc(r::BlendFactor::SrcAlpha,
                     r::BlendFactor::OneMinusSrcAlpha);

        // Upload and render each batch
        static r::VertexArray cosmeticVAO = 0;
        static r::Buffer cosmeticVBO = 0;
        static const Mesh* lastMesh = nullptr;

        if (!cosmeticVAO)
            cosmeticVAO = mimita::game::genVertexArray();

        if (&mesh != lastMesh)
        {
            if (!cosmeticVBO)
                cosmeticVBO = mimita::game::genBuffer();

            r::bindVertexArray(cosmeticVAO);
            r::bufferData(r::BufferTarget::Array, cosmeticVBO, mesh.verts.data(),
                          static_cast<std::uint32_t>(mesh.verts.size() * sizeof(Vertex)),
                          r::BufferUsage::Static);

            r::enableVertexAttribArray(0);
            r::vertexAttribPointer(0, 3, r::VertexType::Float, false,
                                   sizeof(Vertex),
                                   static_cast<std::uint32_t>(offsetof(Vertex, pos)));
            r::enableVertexAttribArray(1);
            r::vertexAttribPointer(1, 2, r::VertexType::Float, false,
                                   sizeof(Vertex),
                                   static_cast<std::uint32_t>(offsetof(Vertex, uv)));
            r::enableVertexAttribArray(2);
            r::vertexAttribPointer(2, 3, r::VertexType::Float, false,
                                   sizeof(Vertex),
                                   static_cast<std::uint32_t>(offsetof(Vertex, normal)));

            lastMesh = &mesh;
        }

        r::bindVertexArray(cosmeticVAO);

        for (const auto& batch : mesh.batches)
        {
            r::activeTexture(0);
            r::bindTexture(r::TextureTarget::Texture2D,
                           overrideTexture ? overrideTexture : batch.texture);
            r::drawArrays(r::Primitive::Triangles, static_cast<std::int32_t>(batch.first),
                          static_cast<std::int32_t>(batch.count));
        }

        r::bindVertexArray(0);
    }

    if (textureEnabledLoc >= 0)
        r::uniform1i(textureEnabledLoc, 0);

    r::popState();
}
