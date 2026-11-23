#include "cosmetic-system.h"
#include "game/gl-resource-bridge-module.h"
#include "avatar.h"
#include "entities/player.h"
#include "map/map_loader.h"
#include "map/map-mesh-types.h"
#include "renderer/renderer.h"
#include "gui/ui-system.h"
#include "world/texture-store.h"
#include "render/render-command.h"
#include "render/world-shader.h"
#include "debug/structured-log.h"
#include "game/job-bridge-module.h"
#include "map/map-loader-mesh.h"
#include "utils/path_utils.h"
#include "tinygltf/tiny_gltf.h"

namespace r = mimita::game::render;

#include <cstdio>
#include <algorithm>
#include <chrono>
#include <exception>
#include <filesystem>
#include <string>
#include <unordered_map>
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

std::string cosmeticTexturePath(const CosmeticSlot& slot,
                                const std::string& actorAvatar)
{
    if (slot.texture.image.empty())
        return {};
    if (slot.texture.image.find('/') != std::string::npos ||
        slot.texture.image.find('\\') != std::string::npos)
        return slot.texture.image;
    const std::string& owner = actorAvatar.empty()
        ? AvatarSystem::instance().currentName() : actorAvatar;
    return "assets/avatars/" + owner + "/" + slot.texture.image;
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
    const auto existing = mCosmetics.find(choice);
    if (existing != mCosmetics.end())
        return existing->second->ready.load(std::memory_order_acquire) &&
               existing->second->loaded;

    auto inst = std::make_shared<CosmeticInstance>();
    inst->choice = choice;
    mCosmetics.emplace(choice, inst);

    std::string path = (choice.find('/') != std::string::npos ||
                        choice.find('\\') != std::string::npos)
        ? choice
        : "assets/objects/things/cosmetics/" + choice;
    if (path.rfind(".glb") == std::string::npos)
        path += ".glb";

    const bool queued = mimita::game::runDetached([inst, path]() {
        StructuredLogger::instance().writeEvent(
            StructuredCategory::Avatar, StructuredLevel::Important,
            "cosmetic.load.started", inst->choice,
            "cosmetic GLB background parse started", 0,
            {{"choice", inst->choice}, {"path", path}},
            __FILE__, __LINE__, __FUNCTION__);
        const std::string resolvedPath = resolveAssetPath(path);
        std::string error;
        bool parsed = false;
        try {
            tinygltf::TinyGLTF loader;
            tinygltf::Model model;
            std::string warning;
            parsed = loader.LoadBinaryFromFile(
                &model, &error, &warning, resolvedPath);
            if (parsed) {
                std::vector<std::uint32_t> noTextures(model.materials.size(), 0);
                const int scene = model.defaultScene >= 0 ? model.defaultScene : 0;
                walkGLBScene(model, noTextures, inst->mesh, scene, nullptr);
                inst->loaded = !inst->mesh.verts.empty();
            }
        } catch (const std::exception& ex) {
            error = ex.what();
        } catch (...) {
            error = "unknown exception while parsing cosmetic GLB";
        }
        inst->ready.store(true, std::memory_order_release);
        StructuredLogger::instance().writeEvent(
            StructuredCategory::Avatar, StructuredLevel::Important,
            "cosmetic.load.completed", inst->choice,
            "cosmetic GLB background parse finished", 0,
            {{"choice", inst->choice}, {"path", path},
             {"parsed", parsed}, {"loaded", inst->loaded},
             {"vertices", inst->mesh.verts.size()}, {"error", error}},
            __FILE__, __LINE__, __FUNCTION__);
    }, "cosmetic-model-load");
    if (!queued) {
        StructuredLogger::instance().writeEvent(
            StructuredCategory::Avatar, StructuredLevel::Important,
            "cosmetic.load.rejected", choice,
            "cosmetic GLB background load could not be queued", 0,
            {{"choice", choice}, {"path", path}},
            __FILE__, __LINE__, __FUNCTION__);
        inst->ready.store(true, std::memory_order_release);
    }
    return false;
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
    if (it != mCosmetics.end() &&
        it->second->ready.load(std::memory_order_acquire) &&
        it->second->loaded)
        return it->second.get();
    return nullptr;
}

void CosmeticSystem::renderCosmetics(const Player& player,
                                     const glm::mat4& view,
                                     const glm::mat4& projection) const
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
    const r::Uniform colorLoc = r::worldShader().uniform("uColor");
    const r::Uniform viewLoc = r::worldShader().uniform("view");
    const r::Uniform projectionLoc = r::worldShader().uniform("projection");
    const r::Uniform usePartModelsLoc = r::worldShader().uniform("uUsePartModels");
    const r::Uniform useInstanceModelLoc = r::worldShader().uniform("uUseInstanceModel");
    const r::Uniform outlinePassLoc = r::worldShader().uniform("uOutlinePass");
    const r::Uniform alphaCutoffLoc = r::worldShader().uniform("uAlphaCutoff");
    const r::Uniform debugViewLoc = r::worldShader().uniform("uDebugView");
    const std::string actorAvatar = player.avatarName();

    // Cosmetics use the shared world shader, so the previous body/outline/world
    // pass can leave matrix-selection and outline uniforms active. pushState()
    // restores GL state, not program uniforms; set the complete cosmetic state.
    r::useProgram(r::worldShader().program());
    if (viewLoc >= 0) r::uniformMatrix4fv(viewLoc, 1, &view[0][0]);
    if (projectionLoc >= 0)
        r::uniformMatrix4fv(projectionLoc, 1, &projection[0][0]);
    if (usePartModelsLoc >= 0) r::uniform1i(usePartModelsLoc, 0);
    if (useInstanceModelLoc >= 0) r::uniform1i(useInstanceModelLoc, 0);
    if (outlinePassLoc >= 0) r::uniform1i(outlinePassLoc, 0);
    if (alphaCutoffLoc >= 0) r::uniform1f(alphaCutoffLoc, 0.0f);
    if (debugViewLoc >= 0) r::uniform1i(debugViewLoc, 0);

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

        const std::string texturePath = cosmeticTexturePath(slot, actorAvatar);
        const std::uint32_t overrideTexture = texturePath.empty()
            ? 0 : gTextures.getPath(texturePath);
        // Cosmetic GLBs often contain geometry only (no material/texture).
        // Never sample a missing/default texture in that case; render the
        // authored color instead of black geometry.
        const bool solidFallback = overrideTexture == 0;
        if (useColorLoc >= 0)
            r::uniform1i(useColorLoc, solidFallback ? 1 : 0);
        if (solidFallback && colorLoc >= 0) {
            const glm::vec3 color = slot.texture.color == glm::vec3(1.0f)
                ? slot.color : slot.texture.color;
            r::uniform4f(colorLoc, color.r, color.g, color.b,
                         std::clamp(slot.texture.opacity, 0.0f, 1.0f));
        }
        if (textureEnabledLoc >= 0)
            r::uniform1i(textureEnabledLoc, solidFallback ? 0 : 1);

        {
            static std::unordered_map<std::string, std::uint64_t> lastLogMs;
            const std::string key = actorAvatar + "|" + choice + "|" +
                texturePath + "|" + (solidFallback ? "solid" : "texture");
            const auto nowMs = static_cast<std::uint64_t>(
                std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::steady_clock::now().time_since_epoch()).count());
            auto& last = lastLogMs[key];
            if (last == 0 || nowMs - last >= 5000) {
                last = nowMs;
                StructuredLogger::instance().writeEvent(
                    StructuredCategory::Rendering, StructuredLevel::Important,
                    "cosmetic.render", "cosmetic",
                    solidFallback
                        ? "cosmetic rendered with solid-color fallback"
                        : "cosmetic rendered with texture",
                    0,
                    nlohmann::json{
                        {"actor_avatar", actorAvatar},
                        {"glb", choice},
                        {"anchor", attachTarget},
                        {"texture_path", texturePath},
                        {"texture_id", overrideTexture},
                        {"solid_fallback", solidFallback},
                        {"tint", {slot.color.r, slot.color.g, slot.color.b}},
                        {"brightness", slot.texture.brightness},
                        {"opacity", slot.texture.opacity},
                        {"uv_offset", {slot.texture.offsetX, slot.texture.offsetY}},
                        {"uv_scale", {slot.texture.scaleX, slot.texture.scaleY}},
                        {"uv_rotation", slot.texture.rotation}},
                    __FILE__, __LINE__, __FUNCTION__);
            }
        }
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

        {
            static std::unordered_map<std::string, std::uint64_t> lastTransformLogMs;
            const std::string key = actorAvatar + "|" + choice + "|" + attachTarget;
            const auto nowMs = static_cast<std::uint64_t>(
                std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::steady_clock::now().time_since_epoch()).count());
            auto& last = lastTransformLogMs[key];
            if (last == 0 || nowMs - last >= 5000) {
                last = nowMs;
                auto translation = [](const glm::mat4& m) {
                    return glm::vec3(m[3]);
                };
                auto scale = [](const glm::mat4& m) {
                    return glm::vec3(glm::length(glm::vec3(m[0])),
                                     glm::length(glm::vec3(m[1])),
                                     glm::length(glm::vec3(m[2])));
                };
                const glm::vec3 attachPos = translation(attachTransform);
                const glm::vec3 attachScale = scale(attachTransform);
                const glm::vec3 modelPos = translation(model);
                const glm::vec3 modelScale = scale(model);
                StructuredLogger::instance().writeEvent(
                    StructuredCategory::Rendering, StructuredLevel::Important,
                    "cosmetic.transform", "cosmetic",
                    "cosmetic attachment and model transform", 0,
                    nlohmann::json{
                        {"actor_avatar", actorAvatar},
                        {"glb", choice},
                        {"anchor", attachTarget},
                        {"mesh_vertices", mesh.verts.size()},
                        {"attach_position", {attachPos.x, attachPos.y, attachPos.z}},
                        {"attach_scale", {attachScale.x, attachScale.y, attachScale.z}},
                        {"model_position", {modelPos.x, modelPos.y, modelPos.z}},
                        {"model_scale", {modelScale.x, modelScale.y, modelScale.z}},
                        {"slot_scale", {slot.scale.x, slot.scale.y, slot.scale.z}},
                        {"use_part_models", 0},
                        {"use_instance_model", 0},
                        {"outline_pass", 0}},
                    __FILE__, __LINE__, __FUNCTION__);
            }
        }

        for (const auto& batch : mesh.batches)
        {
            r::activeTexture(0);
            if (!solidFallback) {
                r::bindTexture(r::TextureTarget::Texture2D,
                               overrideTexture ? overrideTexture : batch.texture);
            }
            r::drawArrays(r::Primitive::Triangles,
                          static_cast<std::int32_t>(batch.first),
                          static_cast<std::int32_t>(batch.count));
        }

        r::bindVertexArray(0);
    }

    if (textureEnabledLoc >= 0)
        r::uniform1i(textureEnabledLoc, 0);
    if (useColorLoc >= 0)
        r::uniform1i(useColorLoc, 0);

    r::popState();
}
