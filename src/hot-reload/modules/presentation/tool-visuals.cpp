// 09 16 2026
/* purpose
* The single hot recipe registry for tool/presentation visuals. Each weapon is a
* `ToolVisualRecipeV1` selected by a stable tool key (gameHash(weaponId)); there
* is no weapon enum, switch, or renderer branch. Server-disagreement appearance
* is a recipe on the same registry idea.
* Editing this file and saving changes the running client's presentation with no
* EXE rebuild (the recipes are re-read every frame).
* Does NOT link into the EXE; only into the replaceable game DLL.
*/
#if defined(MIMITA_GAME_DLL)

#include "hot-reload/hot-tool-visual.h"

#include "hot-reload/hot-package.h"
#include "hot-reload/hot-presentation.h"
#include "hot-reload/hot-behavior-source.h"

#include "combat/weapon-types.h"

#include <cstdint>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/quaternion.hpp>

namespace {

// Compact authoring macros for tool phase keyframes (part order: torso, head,
// leftArm, rightArm, leftLeg, rightLeg). Values are degrees. Locomotion phases
// (idle/carry) drive arms only; weapon actions drive the whole upper body
// (torso+head+arms) so the motion reads strongly.
#define WPN_ARMS(t, lrx, lry, lrz, rrx, rry, rrz)                              \
    {t, {{0,0,0,0,0,0}, {0,0,0,0,0,0}, {0,0,0,lrx,lry,lrz},                    \
         {0,0,0,rrx,rry,rrz}, {0,0,0,0,0,0}, {0,0,0,0,0,0}}}
#define WPN_UPPER(t, torx, tory, torz, hdx, hdy, hdz, lrx, lry, lrz, rrx, rry, rrz) \
    {t, {{0,0,0,torx,tory,torz}, {0,0,0,hdx,hdy,hdz}, {0,0,0,lrx,lry,lrz},     \
         {0,0,0,rrx,rry,rrz}, {0,0,0,0,0,0}, {0,0,0,0,0,0}}}
#define WPN_PH(phase, dur, loop, mask, frames)                                 \
    {phase, dur, loop, mask,                                                   \
     (std::uint32_t)(sizeof(frames) / sizeof(HotAnim::Keyframe)), frames}

ToolVisualRecipeV1 makeBaseVisual(std::uint64_t toolKey, const char* modelPath,
                                  std::uint64_t meshId)
{
    ToolVisualRecipeV1 r{};
    r.toolKey = toolKey;
    r.modelPath = modelPath;
    r.meshId = meshId;
    // 0 = use the model's own embedded GLB texture (correct per-weapon material).
    r.textureId = 0;
    r.texturePath = nullptr;
    r.socket = gameHash("rightArm");
    r.viewRotation[3] = 1.0f;
    r.viewScale = 1.0f;
    r.viewScaleXYZ[0] = r.viewScaleXYZ[1] = r.viewScaleXYZ[2] = 1.0f;
    r.worldRotation[3] = 1.0f;
    r.worldScale = 1.0f;
    r.worldScaleXYZ[0] = r.worldScaleXYZ[1] = r.worldScaleXYZ[2] = 1.0f;
    // Muzzle default: bright untextured sphere, one 60 Hz tick.
    r.muzzle.meshId = HOT_MESH_SPHERE;
    r.muzzle.textureId = 0;
    r.muzzle.color[0] = 1.0f;
    r.muzzle.color[1] = 0.92f;
    r.muzzle.color[2] = 0.62f;
    r.muzzle.color[3] = 1.0f;
    r.muzzle.scale = 0.16f;
    r.muzzle.lifetime = 1.0f / 60.0f;
    r.muzzle.growth = 0.0f;
    r.muzzle.fadeStart = 0.5f;
    r.impact.color[0] = 1.0f;
    r.impact.color[1] = 0.8f;
    r.impact.color[2] = 0.4f;
    r.impact.color[3] = 1.0f;
    r.impact.scale = 0.2f;
    r.impact.lifetime = 0.4f;
    r.explosion.flashColor[0] = 1.0f;
    r.explosion.flashColor[1] = 0.75f;
    r.explosion.flashColor[2] = 0.25f;
    r.explosion.flashColor[3] = 1.0f;
    r.explosion.smokeColor[0] = 0.35f;
    r.explosion.smokeColor[1] = 0.35f;
    r.explosion.smokeColor[2] = 0.35f;
    r.explosion.smokeColor[3] = 1.0f;
    r.explosion.scale = 1.0f;
    r.explosion.lifetime = 0.8f;
    r.flags = 1u;  // complete recipe
    return r;
}

void setEulerDegrees(float q[4], float x, float y, float z)
{
    glm::mat4 rotation(1.0f);
    rotation = glm::rotate(rotation, glm::radians(x), glm::vec3(1, 0, 0));
    rotation = glm::rotate(rotation, glm::radians(y), glm::vec3(0, 1, 0));
    rotation = glm::rotate(rotation, glm::radians(z), glm::vec3(0, 0, 1));
    const glm::quat result = glm::normalize(glm::quat_cast(rotation));
    q[0] = result.x; q[1] = result.y; q[2] = result.z; q[3] = result.w;
}

void setScale(float out[3], float x, float y, float z)
{
    out[0] = x; out[1] = y; out[2] = z;
}

void addFirearmMuzzle(ToolVisualRecipeV1& r, float sr, float sg, float sb,
                      float scale, float lightR, float lightG, float lightB,
                      float intensity, float radius)
{
    r.muzzle.color[0] = sr;
    r.muzzle.color[1] = sg;
    r.muzzle.color[2] = sb;
    r.muzzle.scale = scale;
    r.muzzle.hasLight = 1u;
    r.muzzle.lightColor[0] = lightR;
    r.muzzle.lightColor[1] = lightG;
    r.muzzle.lightColor[2] = lightB;
    r.muzzle.lightIntensity = intensity;
    r.muzzle.lightRadius = radius;
    r.muzzle.lightLifetime = 0.06f;
}

} // namespace

// JSON is an optional authoring layer for the same ToolAnimPhaseV1 data that
// the C++ recipes below provide. The cache is fixed-size on purpose: pose
// generation can keep stable pointers between reloads and never allocates in
// its per-frame lookup path.
namespace {

struct JsonToolAnimationCache {
    static constexpr std::uint32_t kMaxTools = 24;
    static constexpr std::uint32_t kMaxPhases = 8;
    static constexpr std::uint32_t kMaxFrames = 32;

    nlohmann::json root;
    HotAnim::Keyframe frames[kMaxTools][kMaxPhases][kMaxFrames]{};
    ToolAnimPhaseV1 phases[kMaxTools][kMaxPhases]{};
    bool valid[kMaxTools][kMaxPhases]{};
    std::uint32_t counts[kMaxTools]{};
    bool loaded = false;
    std::filesystem::file_time_type write{};
};

int jsonToolPhaseIndex(const std::string& name)
{
    static constexpr const char* names[] = {
        "idle", "shoot", "just_shot", "reload", "equip", "unequip",
        "slash", "lunge"};
    for (int i = 0; i < 8; ++i)
        if (name == names[i])
            return i;
    return -1;
}

std::uint64_t jsonToolPhaseId(int index)
{
    static constexpr std::uint64_t ids[] = {
        TOOL_PHASE_IDLE, TOOL_PHASE_SHOOT, TOOL_PHASE_JUST_SHOT,
        TOOL_PHASE_RELOAD, TOOL_PHASE_EQUIP, TOOL_PHASE_UNEQUIP,
        TOOL_PHASE_SLASH, TOOL_PHASE_LUNGE};
    return index >= 0 && index < 8 ? ids[index] : 0;
}

std::uint32_t jsonToolMask(const nlohmann::json& value,
                           std::uint32_t fallback)
{
    if (value.is_number_unsigned() || value.is_number_integer())
        return value.get<std::uint32_t>();
    if (!value.is_string())
        return fallback;
    const std::string name = value.get<std::string>();
    if (name == "arms") return HotAnim::MaskArms;
    if (name == "upper") return HotAnim::MaskUpper;
    if (name == "full") return HotAnim::MaskFull;
    return fallback;
}

void clearJsonToolCache(JsonToolAnimationCache& cache)
{
    for (std::uint32_t t = 0; t < JsonToolAnimationCache::kMaxTools; ++t) {
        cache.counts[t] = 0;
        for (std::uint32_t p = 0; p < JsonToolAnimationCache::kMaxPhases; ++p) {
            cache.valid[t][p] = false;
            cache.phases[t][p] = {};
        }
    }
    cache.root = nlohmann::json::object();
    cache.loaded = false;
}

void loadJsonToolAnimations(JsonToolAnimationCache& cache,
                            const ToolVisualRecipeV1* recipes,
                            std::uint32_t recipeCount)
{
    clearJsonToolCache(cache);
    std::ifstream file("config/animations.json");
    try {
        cache.root = nlohmann::json::parse(file, nullptr, true, true);
        if (cache.root.value("behaviorSource", "cpp") != "json")
            return;
        const auto tools = cache.root.value("tools", nlohmann::json::object());
        if (!tools.is_object())
            return;
        const std::uint32_t limit = std::min(recipeCount,
                                             JsonToolAnimationCache::kMaxTools);
        for (std::uint32_t t = 0; t < limit; ++t) {
            const char* id = recipes[t].definition.id;
            if (!id || !tools.contains(id) || !tools[id].is_object())
                continue;
            const auto& toolNode = tools[id];
            const auto phaseSetName = toolNode.value("phaseSet", "");
            const auto phaseSets = cache.root.value("phaseSets", nlohmann::json::object());
            const auto phases = !phaseSetName.empty() && phaseSets.is_object()
                ? phaseSets.value(phaseSetName, nlohmann::json::object())
                : toolNode.value("phases", nlohmann::json::object());
            if (!phases.is_object())
                continue;
            cache.counts[t] = recipes[t].definition.phaseCount;
            const std::uint32_t baseCount = std::min(
                recipes[t].definition.phaseCount,
                JsonToolAnimationCache::kMaxPhases);
            for (std::uint32_t p = 0; p < baseCount; ++p)
                cache.phases[t][p] = recipes[t].definition.phases[p];
            for (auto it = phases.begin(); it != phases.end(); ++it) {
                const int p = jsonToolPhaseIndex(it.key());
                if (p < 0 || !it.value().is_object())
                    continue;
                const auto& item = it.value();
                auto& phase = cache.phases[t][p];
                phase.phaseId = jsonToolPhaseId(p);
                phase.duration = std::max(0.001f,
                    item.value("duration", phase.duration > 0.0f ? phase.duration : 0.1f));
                phase.loop = item.value("loop", phase.loop != 0) ? 1u : 0u;
                phase.mask = jsonToolMask(item.value("mask", nlohmann::json()),
                                          phase.mask != 0 ? phase.mask : HotAnim::MaskUpper);
                const auto frames = item.value("keyframes", nlohmann::json::array());
                if (!frames.is_array() || frames.empty())
                    continue;
                const std::uint32_t count = std::min<std::uint32_t>(
                    static_cast<std::uint32_t>(frames.size()),
                    JsonToolAnimationCache::kMaxFrames);
                std::uint32_t mask = 0;
                for (std::uint32_t f = 0; f < count; ++f) {
                    auto& dst = cache.frames[t][p][f];
                    for (std::uint32_t part = 0; part < HotAnim::PartCount; ++part)
                        for (int k = 0; k < 6; ++k)
                            dst.part[part][k] = 0.0f;
                    const auto& src = frames[f];
                    dst.t = src.value("time", src.value("tick", 0.0f) / 60.0f);
                    if (!std::isfinite(dst.t) || dst.t < 0.0f)
                        dst.t = 0.0f;
                    const auto parts = src.value("parts", nlohmann::json::object());
                    if (!parts.is_object())
                        continue;
                    for (auto pit = parts.begin(); pit != parts.end(); ++pit) {
                        const int part = HotAnim::jsonPartIndex(pit.key());
                        if (part < 0 || !pit.value().is_object())
                            continue;
                        const auto tr = pit.value().value("translation", nlohmann::json::array());
                        const auto ro = pit.value().value("rotation", nlohmann::json::array());
                        for (int k = 0; k < 3 && k < static_cast<int>(tr.size()); ++k)
                            if (tr[k].is_number() && std::isfinite(tr[k].get<float>()))
                                dst.part[part][k] = tr[k].get<float>();
                        for (int k = 0; k < 3 && k < static_cast<int>(ro.size()); ++k)
                            if (ro[k].is_number() && std::isfinite(ro[k].get<float>()))
                                dst.part[part][3 + k] = ro[k].get<float>();
                        mask |= 1u << part;
                    }
                }
                if (mask != 0) {
                    phase.mask = mask;
                    phase.frames = cache.frames[t][p];
                    phase.frameCount = count;
                    if (item.find("duration") == item.end())
                        phase.duration = std::max(0.001f,
                            cache.frames[t][p][count - 1].t);
                    cache.valid[t][p] = true;
                    if (p >= static_cast<int>(cache.counts[t]))
                        cache.counts[t] = static_cast<std::uint32_t>(p + 1);
                }
            }
        }
        cache.loaded = true;
    } catch (...) {
        clearJsonToolCache(cache);
    }
}

void refreshJsonToolAnimations(ToolVisualRecipeV1* recipes,
                               std::uint32_t recipeCount)
{
    static JsonToolAnimationCache cache;
    static const ToolAnimPhaseV1* baselinePhases[
        JsonToolAnimationCache::kMaxTools]{};
    static std::uint32_t baselineCounts[JsonToolAnimationCache::kMaxTools]{};
    static bool baselineReady = false;
    std::error_code ec;
    const auto write = std::filesystem::last_write_time("config/animations.json", ec);
    const std::uint32_t limit = std::min(recipeCount,
                                         JsonToolAnimationCache::kMaxTools);
    if (!baselineReady) {
        for (std::uint32_t t = 0; t < limit; ++t) {
            baselinePhases[t] = recipes[t].definition.phases;
            baselineCounts[t] = recipes[t].definition.phaseCount;
        }
        baselineReady = true;
    }
    if (!ec && (!cache.loaded || write != cache.write)) {
        for (std::uint32_t t = 0; t < limit; ++t) {
            recipes[t].definition.phases = baselinePhases[t];
            recipes[t].definition.phaseCount = baselineCounts[t];
        }
        loadJsonToolAnimations(cache, recipes, recipeCount);
        cache.write = write;
    }
    const bool jsonActive = cache.loaded &&
        cache.root.value("behaviorSource", "cpp") == "json";
    for (std::uint32_t t = 0; t < limit; ++t) {
        if (jsonActive && cache.counts[t] != 0) {
            recipes[t].definition.phases = cache.phases[t];
            recipes[t].definition.phaseCount = cache.counts[t];
        } else {
            recipes[t].definition.phases = baselinePhases[t];
            recipes[t].definition.phaseCount = baselineCounts[t];
        }
    }
}

} // namespace

// ── Per-weapon animation phase tables (procedural C++ keyframes) ──────
namespace {

// Revolver.
static constexpr HotAnim::Keyframe kRevIdle[] = {
    WPN_ARMS(0.00f, -58, 0, 6, -70, 0, -4),
    WPN_ARMS(1.00f, -58, 0, 6, -70, 0, -4),
};
static constexpr HotAnim::Keyframe kRevShoot[] = {
    WPN_UPPER(0.00f, -8, 0, 0, -4, 0, 0, -70, 0, 10, -85, 0, -2),
    WPN_UPPER(0.05f, -17, 0, 0, -11, 0, 0, -74, 0, 12, -128, 0, -2),
    WPN_UPPER(0.14f, -6, 0, 0, -2, 0, 0, -70, 0, 10, -82, 0, -2),
};
static constexpr HotAnim::Keyframe kRevJustShot[] = {
    WPN_UPPER(0.00f, -4, 0, 0, -1, 0, 0, -70, 0, 10, -92, 0, -2),
    WPN_UPPER(0.12f, 0, 0, 0, 0, 0, 0, -58, 0, 6, -70, 0, -4),
};
static constexpr HotAnim::Keyframe kRevReload[] = {
    WPN_UPPER(0.00f, 6, 0, 0, 12, 0, 0, 55, 0, 30, -60, 0, -2),
    WPN_UPPER(0.30f, 10, -14, 0, 8, -10, 0, -20, 0, 15, -58, 0, -2),
    WPN_UPPER(0.60f, 2, 0, 0, 0, 0, 0, -58, 0, 6, -70, 0, -4),
};
static constexpr HotAnim::Keyframe kRevEquip[] = {
    WPN_UPPER(0.00f, 8, 0, 0, 6, 0, 0, -20, 0, 4, -30, 0, -4),
    WPN_UPPER(0.18f, 0, 0, 0, 0, 0, 0, -58, 0, 6, -70, 0, -4),
};
static constexpr HotAnim::Keyframe kRevUnequip[] = {
    WPN_UPPER(0.00f, 0, 0, 0, 0, 0, 0, -58, 0, 6, -70, 0, -4),
    WPN_UPPER(0.18f, 8, 0, 0, 6, 0, 0, -20, 0, 4, -30, 0, -4),
};
static const ToolAnimPhaseV1 kRevPhases[] = {
    WPN_PH(TOOL_PHASE_IDLE, 1.00f, 1, HotAnim::MaskArms, kRevIdle),
    WPN_PH(TOOL_PHASE_SHOOT, 0.14f, 0, HotAnim::MaskUpper, kRevShoot),
    WPN_PH(TOOL_PHASE_JUST_SHOT, 0.12f, 0, HotAnim::MaskUpper, kRevJustShot),
    WPN_PH(TOOL_PHASE_RELOAD, 0.60f, 0, HotAnim::MaskUpper, kRevReload),
    WPN_PH(TOOL_PHASE_EQUIP, 0.18f, 0, HotAnim::MaskUpper, kRevEquip),
    WPN_PH(TOOL_PHASE_UNEQUIP, 0.18f, 0, HotAnim::MaskUpper, kRevUnequip),
};

// Shotgun.
static constexpr HotAnim::Keyframe kShotIdle[] = {
    WPN_ARMS(0.00f, -60, 0, 10, -72, 0, -6),
    WPN_ARMS(1.00f, -60, 0, 10, -72, 0, -6),
};
static constexpr HotAnim::Keyframe kShotShoot[] = {
    WPN_UPPER(0.00f, -9, 0, 0, -5, 0, 0, -72, 0, 12, -88, 0, -4),
    WPN_UPPER(0.06f, -20, 0, 0, -13, 0, 0, -76, 0, 14, -135, 0, -4),
    WPN_UPPER(0.16f, -6, 0, 0, -2, 0, 0, -72, 0, 12, -88, 0, -4),
};
static constexpr HotAnim::Keyframe kShotJustShot[] = {
    WPN_UPPER(0.00f, -5, 0, 0, -2, 0, 0, -72, 0, 12, -96, 0, -4),
    WPN_UPPER(0.14f, 0, 0, 0, 0, 0, 0, -60, 0, 10, -72, 0, -6),
};
static constexpr HotAnim::Keyframe kShotReload[] = {
    WPN_UPPER(0.00f, 8, 0, 0, 14, 0, 0, 60, 0, 34, -60, 0, -2),
    WPN_UPPER(0.30f, 12, -18, 0, 10, -12, 0, 10, 0, 26, -58, 0, -2),
    WPN_UPPER(0.55f, 8, 10, 0, 4, 8, 0, -30, 0, 14, -62, 0, 0),
    WPN_UPPER(0.80f, 2, 0, 0, 0, 0, 0, -60, 0, 10, -72, 0, -6),
};
static constexpr HotAnim::Keyframe kShotEquip[] = {
    WPN_UPPER(0.00f, 9, 0, 0, 7, 0, 0, -22, 0, 6, -34, 0, -6),
    WPN_UPPER(0.22f, 0, 0, 0, 0, 0, 0, -60, 0, 10, -72, 0, -6),
};
static constexpr HotAnim::Keyframe kShotUnequip[] = {
    WPN_UPPER(0.00f, 0, 0, 0, 0, 0, 0, -60, 0, 10, -72, 0, -6),
    WPN_UPPER(0.22f, 9, 0, 0, 7, 0, 0, -22, 0, 6, -34, 0, -6),
};
static const ToolAnimPhaseV1 kShotPhases[] = {
    WPN_PH(TOOL_PHASE_IDLE, 1.00f, 1, HotAnim::MaskArms, kShotIdle),
    WPN_PH(TOOL_PHASE_SHOOT, 0.16f, 0, HotAnim::MaskUpper, kShotShoot),
    WPN_PH(TOOL_PHASE_JUST_SHOT, 0.14f, 0, HotAnim::MaskUpper, kShotJustShot),
    WPN_PH(TOOL_PHASE_RELOAD, 0.80f, 0, HotAnim::MaskUpper, kShotReload),
    WPN_PH(TOOL_PHASE_EQUIP, 0.22f, 0, HotAnim::MaskUpper, kShotEquip),
    WPN_PH(TOOL_PHASE_UNEQUIP, 0.22f, 0, HotAnim::MaskUpper, kShotUnequip),
};

// Rocket launcher.
static constexpr HotAnim::Keyframe kRocketIdle[] = {
    WPN_ARMS(0.00f, -62, 0, 8, -66, 0, -10),
    WPN_ARMS(1.00f, -62, 0, 8, -66, 0, -10),
};
static constexpr HotAnim::Keyframe kRocketShoot[] = {
    WPN_UPPER(0.00f, -10, 0, 0, -6, 0, 0, -64, 0, 8, -80, 0, -8),
    WPN_UPPER(0.07f, -22, 0, 0, -14, 0, 0, -68, 0, 10, -120, 0, -8),
    WPN_UPPER(0.18f, -6, 0, 0, -2, 0, 0, -64, 0, 8, -80, 0, -8),
};
static constexpr HotAnim::Keyframe kRocketJustShot[] = {
    WPN_UPPER(0.00f, -5, 0, 0, -2, 0, 0, -62, 0, 8, -86, 0, -10),
    WPN_UPPER(0.14f, 0, 0, 0, 0, 0, 0, -62, 0, 8, -66, 0, -10),
};
static constexpr HotAnim::Keyframe kRocketReload[] = {
    WPN_UPPER(0.00f, -8, 0, 0, -4, 0, 0, 50, 0, 28, -55, 0, -6),
    WPN_UPPER(0.45f, 6, 12, 0, 4, 10, 0, -25, 0, 14, -55, 0, -6),
    WPN_UPPER(0.90f, 0, 0, 0, 0, 0, 0, -62, 0, 8, -66, 0, -10),
};
static constexpr HotAnim::Keyframe kRocketEquip[] = {
    WPN_UPPER(0.00f, 6, 0, 0, 5, 0, 0, -24, 0, 6, -32, 0, -8),
    WPN_UPPER(0.24f, 0, 0, 0, 0, 0, 0, -62, 0, 8, -66, 0, -10),
};
static constexpr HotAnim::Keyframe kRocketUnequip[] = {
    WPN_UPPER(0.00f, 0, 0, 0, 0, 0, 0, -62, 0, 8, -66, 0, -10),
    WPN_UPPER(0.24f, 6, 0, 0, 5, 0, 0, -24, 0, 6, -32, 0, -8),
};
static const ToolAnimPhaseV1 kRocketPhases[] = {
    WPN_PH(TOOL_PHASE_IDLE, 1.00f, 1, HotAnim::MaskArms, kRocketIdle),
    WPN_PH(TOOL_PHASE_SHOOT, 0.18f, 0, HotAnim::MaskUpper, kRocketShoot),
    WPN_PH(TOOL_PHASE_JUST_SHOT, 0.14f, 0, HotAnim::MaskUpper, kRocketJustShot),
    WPN_PH(TOOL_PHASE_RELOAD, 0.90f, 0, HotAnim::MaskUpper, kRocketReload),
    WPN_PH(TOOL_PHASE_EQUIP, 0.24f, 0, HotAnim::MaskUpper, kRocketEquip),
    WPN_PH(TOOL_PHASE_UNEQUIP, 0.24f, 0, HotAnim::MaskUpper, kRocketUnequip),
};

// Grenade launcher.
static constexpr HotAnim::Keyframe kGrenadeIdle[] = {
    WPN_ARMS(0.00f, -60, 0, 12, -72, 0, -2),
    WPN_ARMS(1.00f, -60, 0, 12, -72, 0, -2),
};
static constexpr HotAnim::Keyframe kGrenadeShoot[] = {
    WPN_UPPER(0.00f, -9, 0, 0, -5, 0, 0, -72, 0, 12, -86, 0, -2),
    WPN_UPPER(0.06f, -20, 0, 0, -13, 0, 0, -76, 0, 14, -130, 0, -2),
    WPN_UPPER(0.16f, -6, 0, 0, -2, 0, 0, -72, 0, 12, -86, 0, -2),
};
static constexpr HotAnim::Keyframe kGrenadeJustShot[] = {
    WPN_UPPER(0.00f, -5, 0, 0, -2, 0, 0, -72, 0, 12, -94, 0, -2),
    WPN_UPPER(0.14f, 0, 0, 0, 0, 0, 0, -60, 0, 12, -72, 0, -2),
};
static constexpr HotAnim::Keyframe kGrenadeReload[] = {
    WPN_UPPER(0.00f, 8, 0, 0, 14, 0, 0, 58, 0, 32, -60, 0, 0),
    WPN_UPPER(0.35f, 10, -16, 0, 8, -12, 0, -25, 0, 14, -58, 0, 0),
    WPN_UPPER(0.70f, 2, 0, 0, 0, 0, 0, -60, 0, 12, -72, 0, -2),
};
static constexpr HotAnim::Keyframe kGrenadeEquip[] = {
    WPN_UPPER(0.00f, 9, 0, 0, 7, 0, 0, -22, 0, 6, -34, 0, -4),
    WPN_UPPER(0.22f, 0, 0, 0, 0, 0, 0, -60, 0, 12, -72, 0, -2),
};
static constexpr HotAnim::Keyframe kGrenadeUnequip[] = {
    WPN_UPPER(0.00f, 0, 0, 0, 0, 0, 0, -60, 0, 12, -72, 0, -2),
    WPN_UPPER(0.22f, 9, 0, 0, 7, 0, 0, -22, 0, 6, -34, 0, -4),
};
static const ToolAnimPhaseV1 kGrenadePhases[] = {
    WPN_PH(TOOL_PHASE_IDLE, 1.00f, 1, HotAnim::MaskArms, kGrenadeIdle),
    WPN_PH(TOOL_PHASE_SHOOT, 0.16f, 0, HotAnim::MaskUpper, kGrenadeShoot),
    WPN_PH(TOOL_PHASE_JUST_SHOT, 0.14f, 0, HotAnim::MaskUpper, kGrenadeJustShot),
    WPN_PH(TOOL_PHASE_RELOAD, 0.70f, 0, HotAnim::MaskUpper, kGrenadeReload),
    WPN_PH(TOOL_PHASE_EQUIP, 0.22f, 0, HotAnim::MaskUpper, kGrenadeEquip),
    WPN_PH(TOOL_PHASE_UNEQUIP, 0.22f, 0, HotAnim::MaskUpper, kGrenadeUnequip),
};

// Swordsword.
static constexpr HotAnim::Keyframe kSwordIdle[] = {
    WPN_ARMS(0.00f, -30, 0, 10, -52, 0, -4),
    WPN_ARMS(1.00f, -30, 0, 10, -52, 0, -4),
};
static constexpr HotAnim::Keyframe kSwordShoot[] = {
    WPN_UPPER(0.00f, 4, 0, 0, -2, 0, 0, -25, 0, 8, -70, 0, -4),
    WPN_UPPER(0.18f, 16, 0, 0, -8, 0, 0, -25, 0, 8, -108, 0, -4),
};
static constexpr HotAnim::Keyframe kSwordSlash[] = {
    WPN_UPPER(0.00f, 0, -16, 0, 0, -12, 0, -20, 0, 6, -155, -6, 0),
    WPN_UPPER(0.12f, 0, 22, 0, 0, 14, 0, -25, 0, 6, -30, 45, 0),
    WPN_UPPER(0.30f, 0, 10, 0, 0, 6, 0, -15, 0, 6, 12, 22, 0),
};
static constexpr HotAnim::Keyframe kSwordLunge[] = {
    WPN_UPPER(0.00f, 24, 0, 0, -6, 0, 0, -30, 0, 6, -110, 0, -4),
    WPN_UPPER(0.28f, 34, 0, 0, -9, 0, 0, -20, 0, 6, -128, 0, -4),
};
static constexpr HotAnim::Keyframe kSwordEquip[] = {
    WPN_UPPER(0.00f, 8, 0, 0, 6, 0, 0, -15, 0, 6, -28, 0, -4),
    WPN_UPPER(0.20f, 0, 0, 0, 0, 0, 0, -30, 0, 10, -52, 0, -4),
};
static constexpr HotAnim::Keyframe kSwordUnequip[] = {
    WPN_UPPER(0.00f, 0, 0, 0, 0, 0, 0, -30, 0, 10, -52, 0, -4),
    WPN_UPPER(0.20f, 8, 0, 0, 6, 0, 0, -15, 0, 6, -28, 0, -4),
};
static const ToolAnimPhaseV1 kSwordPhases[] = {
    WPN_PH(TOOL_PHASE_IDLE, 1.00f, 1, HotAnim::MaskArms, kSwordIdle),
    WPN_PH(TOOL_PHASE_SHOOT, 0.18f, 0, HotAnim::MaskUpper, kSwordShoot),
    WPN_PH(TOOL_PHASE_SLASH, 0.30f, 0, HotAnim::MaskUpper, kSwordSlash),
    WPN_PH(TOOL_PHASE_LUNGE, 0.28f, 0, HotAnim::MaskUpper, kSwordLunge),
    WPN_PH(TOOL_PHASE_EQUIP, 0.20f, 0, HotAnim::MaskUpper, kSwordEquip),
    WPN_PH(TOOL_PHASE_UNEQUIP, 0.20f, 0, HotAnim::MaskUpper, kSwordUnequip),
};

// Spy knife.
static constexpr HotAnim::Keyframe kKnifeIdle[] = {
    WPN_ARMS(0.00f, -20, 0, 8, -40, 0, -6),
    WPN_ARMS(1.00f, -20, 0, 8, -40, 0, -6),
};
static constexpr HotAnim::Keyframe kKnifeShoot[] = {
    WPN_UPPER(0.00f, 2, 0, 0, -1, 0, 0, -20, 0, 8, -40, 0, -6),
    WPN_UPPER(0.10f, 10, -14, 0, -4, -10, 0, -18, 0, 8, -105, 0, -6),
    WPN_UPPER(0.25f, 0, 0, 0, 0, 0, 0, -20, 0, 8, -40, 0, -6),
};
static constexpr HotAnim::Keyframe kKnifeSlash[] = {
    WPN_UPPER(0.00f, 0, -14, 0, 0, -10, 0, -20, 0, 8, -40, 0, -6),
    WPN_UPPER(0.10f, 0, 20, 0, 0, 14, 0, -18, 0, 8, -120, 0, -6),
    WPN_UPPER(0.25f, 0, 8, 0, 0, 4, 0, -20, 0, 8, -35, 0, -6),
};
static constexpr HotAnim::Keyframe kKnifeEquip[] = {
    WPN_UPPER(0.00f, 7, 0, 0, 5, 0, 0, -12, 0, 6, -22, 0, -6),
    WPN_UPPER(0.18f, 0, 0, 0, 0, 0, 0, -20, 0, 8, -40, 0, -6),
};
static constexpr HotAnim::Keyframe kKnifeUnequip[] = {
    WPN_UPPER(0.00f, 0, 0, 0, 0, 0, 0, -20, 0, 8, -40, 0, -6),
    WPN_UPPER(0.18f, 7, 0, 0, 5, 0, 0, -12, 0, 6, -22, 0, -6),
};
static const ToolAnimPhaseV1 kKnifePhases[] = {
    WPN_PH(TOOL_PHASE_IDLE, 1.00f, 1, HotAnim::MaskArms, kKnifeIdle),
    WPN_PH(TOOL_PHASE_SHOOT, 0.25f, 0, HotAnim::MaskUpper, kKnifeShoot),
    WPN_PH(TOOL_PHASE_SLASH, 0.25f, 0, HotAnim::MaskUpper, kKnifeSlash),
    WPN_PH(TOOL_PHASE_EQUIP, 0.18f, 0, HotAnim::MaskUpper, kKnifeEquip),
    WPN_PH(TOOL_PHASE_UNEQUIP, 0.18f, 0, HotAnim::MaskUpper, kKnifeUnequip),
};

constexpr std::uint32_t kAllGameplayFields =
    GAME_TOOL_FIELD_DAMAGE | GAME_TOOL_FIELD_BEHAVIOR | GAME_TOOL_FIELD_TIMING |
    GAME_TOOL_FIELD_AMMO | GAME_TOOL_FIELD_PROJECTILE | GAME_TOOL_FIELD_MODEL |
    GAME_TOOL_FIELD_SOUNDS | GAME_TOOL_FIELD_SLOT;

void setDefinition(ToolVisualRecipeV1& r, WeaponBehaviorType behavior,
                   WeaponFireMode fireMode, WeaponNetworkMode networkMode,
                   bool hitscan, std::uint32_t slot, float damage,
                   float headshot, float fireDelay, float reloadTime, float spread,
                   float recoil, int mag, int reserve, int pellets,
                   float projSpeed, float projRadius, float projLife,
                   float equipTime, float unequipTime,
                   const ToolAnimPhaseV1* phases, std::uint32_t phaseCount)
{
    r.definition.presentMask = kAllGameplayFields;
    r.definition.behaviorType = static_cast<std::uint32_t>(behavior);
    r.definition.fireMode = static_cast<std::uint32_t>(fireMode);
    r.definition.networkMode = static_cast<std::uint32_t>(networkMode);
    r.definition.hitscan = hitscan ? 1u : 0u;
    r.definition.slot = slot;
    r.definition.damage = damage;
    r.definition.headshotMultiplier = headshot;
    r.definition.fireDelay = fireDelay;
    r.definition.reloadTime = reloadTime;
    r.definition.spread = spread;
    r.definition.recoil = recoil;
    r.definition.magazineSize = mag;
    r.definition.reserveAmmo = reserve;
    r.definition.pelletCount = pellets;
    r.definition.projectileSpeed = projSpeed;
    r.definition.projectileRadius = projRadius;
    r.definition.projectileLifetime = projLife;
    r.definition.equipPoseTime = equipTime;
    r.definition.unequipPoseTime = unequipTime;
    r.definition.phases = phases;
    r.definition.phaseCount = phaseCount;
}

void setIdentity(ToolVisualRecipeV1& r, const char* id, const char* displayName)
{
    r.definition.id = id;
    r.definition.displayName = displayName;
}

// Behavior family + presentation set identity. Behavior id selects the shared
// hot execution function; animation/effect set ids are the keys the animation
// and effects agents resolve from tool action events.
void setBehavior(ToolVisualRecipeV1& r, std::uint64_t behaviorId,
                 std::uint64_t effectSetId, std::uint32_t networkPolicy,
                 std::uint32_t collisionPolicy)
{
    r.definition.behaviorId = behaviorId;
    r.definition.animationSetId = r.toolKey;
    r.definition.effectSetId = effectSetId;
    r.definition.networkPolicy = networkPolicy;
    r.definition.collisionPolicy = collisionPolicy;
}

// Phase-0 opt-in: this definition is ready to own its execution hot-side. Until
// set, the cold attack path stays authoritative even though a behavior exists.
void setOwnsExecution(ToolVisualRecipeV1& r)
{
    r.definition.toolFlags |= TOOL_FLAG_OWNS_EXECUTION;
}

// Effect set ids (stable; the effects agent maps these to computed visuals).
constexpr std::uint64_t kEffectSetBallistic = gameHash("tool.effect.ballistic");
constexpr std::uint64_t kEffectSetProjectile = gameHash("tool.effect.projectile");
constexpr std::uint64_t kEffectSetMelee = gameHash("tool.effect.melee");

// Only the revolver supplies customParams through the hot definition today
// (values mirror config/weapons.json). Other weapons leave params empty so their
// JSON customParams remain; a brand-new hot weapon can supply its own.
static const ToolParamV1 kRevParams[] = {
    {"distanceFalloffStart", 60.0f},
    {"minDamageFraction", 0.02f},
    {"falloffExponent", 2.0f},
    {"limbDamageMultiplier", 0.75f},
    {"minAngleFactor", 0.45f},
    {"shootPoseTime", 0.12f},
    {"equipPoseTime", 0.18f},
};

} // namespace

ToolVisualRecipeV1 makeRevolverVisual()
{
    ToolVisualRecipeV1 r = makeBaseVisual(
        gameHash("revolver"),
        "assets/objects/weapons/mimita-revolver-v1.glb",
        gameHash("mesh.tool.revolver"));
    r.worldPosition[0] = -0.2f;
    setScale(r.viewScaleXYZ, 1.0f, 1.0f, 1.0f);
    addFirearmMuzzle(r, 1.0f, 0.9f, 0.55f, 0.16f, 1.0f, 0.85f, 0.5f, 3.0f, 6.0f);
    r.sounds.fire = "revolvershoot";
    setDefinition(r, WeaponBehaviorType::Hitscan, WeaponFireMode::SemiAuto,
                  WeaponNetworkMode::Normal, true, 1u, 9.0f, 3.0f, 0.008f, 1.0f, 0.0f,
                  0.0f, 6, 1337, 1, 0.0f, 0.0f, 0.0f, 0.12f, 0.18f, kRevPhases,
                  (std::uint32_t)(sizeof(kRevPhases) / sizeof(ToolAnimPhaseV1)));
    r.sounds.reload = "revolverreload";
    r.sounds.equip = "weapon/revolver/revolverequip";
    r.sounds.unequip = "weapon/revolver/revolverequip";
    setIdentity(r, "revolver", "Revolver");
    setBehavior(r, TOOL_BEHAVIOR_HITSCAN, kEffectSetBallistic,
                TOOL_NETWORK_NORMAL, TOOL_COLLISION_HITSCAN);
    setOwnsExecution(r);
    r.definition.soundHit = "player_hurt";
    r.definition.soundDryFire = "ui/click";
    r.definition.params = kRevParams;
    r.definition.paramCount =
        static_cast<std::uint32_t>(sizeof(kRevParams) / sizeof(ToolParamV1));
    return r;
}

ToolVisualRecipeV1 makeShotgunVisual()
{
    ToolVisualRecipeV1 r = makeBaseVisual(
        gameHash("shotgun"),
        "assets/objects/weapons/mimita-shotgun-v1.glb",
        gameHash("mesh.tool.shotgun"));
    r.viewPosition[2] = 0.5f;
    r.worldPosition[1] = 0.05f; r.worldPosition[2] = -1.5f;
    setEulerDegrees(r.worldRotation, 0.0f, -5.0f, -90.0f);
    addFirearmMuzzle(r, 1.0f, 0.82f, 0.45f, 0.2f, 1.0f, 0.78f, 0.42f, 3.2f, 7.0f);
    r.sounds.fire = "shotgunshoot";
    setDefinition(r, WeaponBehaviorType::Hitscan, WeaponFireMode::SemiAuto,
                  WeaponNetworkMode::Normal, true, 3u, 5.0f, 3.0f, 0.025f, 1.0f, 10.0f,
                  10.0f, 2, 1337, 15, 0.0f, 0.0f, 0.0f, 0.14f, 0.22f, kShotPhases,
                  (std::uint32_t)(sizeof(kShotPhases) / sizeof(ToolAnimPhaseV1)));
    r.sounds.reload = "shotgunreload";
    r.sounds.equip = "weapon/shotgun/shotgunequip";
    r.sounds.unequip = "weapon/shotgun/shotgunequip";
    setIdentity(r, "shotgun", "Shotgun");
    setBehavior(r, TOOL_BEHAVIOR_PELLET, kEffectSetBallistic,
                TOOL_NETWORK_NORMAL, TOOL_COLLISION_HITSCAN);
    setOwnsExecution(r);
    return r;
}

ToolVisualRecipeV1 makeRocketLauncherVisual()
{
    ToolVisualRecipeV1 r = makeBaseVisual(
        gameHash("rocket_launcher"),
        "assets/objects/weapons/mimita-rpg-v3.glb",
        gameHash("mesh.tool.rocket_launcher"));
    r.viewPosition[0] = 0.2f; r.viewPosition[2] = -1.5f;
    setEulerDegrees(r.viewRotation, 0.0f, 0.0f, -90.0f);
    setScale(r.viewScaleXYZ, 1.5f, 1.5f, 1.5f);
    addFirearmMuzzle(r, 1.0f, 0.7f, 0.3f, 0.3f, 1.0f, 0.6f, 0.25f, 4.0f, 9.0f);
    r.projectile.meshId = HOT_MESH_ROCKET;
    r.projectile.textureId = HOT_TEX_ROCKET;
    r.projectile.scale = 1.0f;
    r.projectile.color[0] = r.projectile.color[1] = r.projectile.color[2] = 1.0f;
    r.projectile.color[3] = 1.0f;
    r.projectile.length = 1.5f;
    r.projectile.radius = 0.18f;
    r.projectile.orientFromVelocity = 1u;
    r.sounds.fire = "rocketlauncher/rocketshoot";
    setDefinition(r, WeaponBehaviorType::RocketLauncher, WeaponFireMode::Automatic,
                  WeaponNetworkMode::Normal, false, 7u, 0.0f, 1.0f, 0.65f, 1.5f, 0.0f,
                  50.0f, 666, 1337, 1, 45.0f, 0.7f, 5.0f, 0.04f, 0.04f, kRocketPhases,
                  (std::uint32_t)(sizeof(kRocketPhases) / sizeof(ToolAnimPhaseV1)));
    r.sounds.reload = "rocketlauncher/rocketlauncherreload";
    setIdentity(r, "rocket_launcher", "Rocket Launcher");
    setBehavior(r, TOOL_BEHAVIOR_ROCKET, kEffectSetProjectile,
                TOOL_NETWORK_NORMAL, TOOL_COLLISION_PROJECTILE);
    setOwnsExecution(r);
    return r;
}

ToolVisualRecipeV1 makeGrenadeLauncherVisual()
{
    ToolVisualRecipeV1 r = makeBaseVisual(
        gameHash("grenade_launcher"),
        "assets/objects/weapons/mimita-nadelauncher-v1.glb",
        gameHash("mesh.tool.grenade_launcher"));
    r.viewPosition[0] = -1.0f; r.viewPosition[1] = 0.3f; r.viewPosition[2] = 0.6f;
    setEulerDegrees(r.viewRotation, -100.0f, -5.0f, 168.0f);
    setScale(r.viewScaleXYZ, 0.8f, 0.4f, 0.4f);
    addFirearmMuzzle(r, 1.0f, 0.85f, 0.5f, 0.18f, 1.0f, 0.8f, 0.45f, 3.0f, 7.0f);
    r.projectile.meshId = HOT_MESH_GRENADE;
    r.projectile.textureId = HOT_TEX_GRENADE;
    r.projectile.scale = 1.0f;
    r.projectile.color[0] = r.projectile.color[1] = r.projectile.color[2] = 1.0f;
    r.projectile.color[3] = 1.0f;
    r.projectile.radius = 0.28f;
    r.projectile.orientFromVelocity = 1u;
    r.sounds.fire = "grenadelauncher/grenadelaunchershoot";
    setDefinition(r, WeaponBehaviorType::GrenadeLauncher, WeaponFireMode::SemiAuto,
                  WeaponNetworkMode::Normal, false, 8u, 0.0f, 1.0f, 0.6f, 0.01f, 0.0f,
                  30.0f, 4, 1337, 1, 40.0f, 1.6f, 3.0f, 0.24f, 0.24f, kGrenadePhases,
                  (std::uint32_t)(sizeof(kGrenadePhases) / sizeof(ToolAnimPhaseV1)));
    r.sounds.reload = "grenadelauncher/grenadelauncherload";
    setIdentity(r, "grenade_launcher", "Grenade Launcher");
    setBehavior(r, TOOL_BEHAVIOR_GRENADE, kEffectSetProjectile,
                TOOL_NETWORK_NORMAL, TOOL_COLLISION_PROJECTILE);
    setOwnsExecution(r);
    return r;
}

ToolVisualRecipeV1 makeSpyknifeVisual()
{
    ToolVisualRecipeV1 r = makeBaseVisual(
        gameHash("spyknife"),
        "assets/objects/weapons/mimita-spy-knife-v2.glb",
        gameHash("mesh.tool.spyknife"));
    r.viewPosition[0] = -8.4f; r.viewPosition[1] = -3.0f; r.viewPosition[2] = 0.4f;
    setEulerDegrees(r.viewRotation, 0.0f, 30.0f, 0.0f);
    r.worldPosition[0] = 9.3f; r.worldPosition[1] = 2.9f; r.worldPosition[2] = -0.1f;
    setEulerDegrees(r.worldRotation, 0.0f, 0.0f, -5.0f);
    setScale(r.viewScaleXYZ, 1.0f, 1.0f, 2.0f);
    // Melee: no muzzle flash / light.
    r.muzzle.hasLight = 0u;
    r.muzzle.scale = 0.0f;
    r.muzzle.lifetime = 0.0f;
    r.sounds.fire = "weapon/hafs/hafsswing";
    setDefinition(r, WeaponBehaviorType::SpyKnife, WeaponFireMode::SemiAuto,
                  WeaponNetworkMode::Normal, false, 12u, 1.0f, 1.0f, 0.0f, 0.0f, 0.0f,
                  0.0f, 0, 0, 1, 0.0f, 0.0f, 0.0f, 0.18f, 0.18f, kKnifePhases,
                  (std::uint32_t)(sizeof(kKnifePhases) / sizeof(ToolAnimPhaseV1)));
    setIdentity(r, "spyknife", "Spy Knife");
    r.definition.soundHit = "player_hurt";
    setBehavior(r, TOOL_BEHAVIOR_MELEE, kEffectSetMelee,
                TOOL_NETWORK_NORMAL, TOOL_COLLISION_CONTACT);
    setOwnsExecution(r);
    return r;
}

ToolVisualRecipeV1 makeSwordswordVisual()
{
    ToolVisualRecipeV1 r = makeBaseVisual(
        gameHash("swordsword"),
        "assets/objects/weapons/mimita-hafs-v1.glb",
        gameHash("mesh.tool.swordsword"));
    r.viewPosition[0] = -9.0f; r.viewPosition[1] = 0.2f;
    setEulerDegrees(r.viewRotation, 100.0f, 0.0f, 0.0f);
    setScale(r.viewScaleXYZ, 0.6f, 0.3f, 0.3f);
    r.muzzle.hasLight = 0u;
    r.muzzle.scale = 0.0f;
    r.muzzle.lifetime = 0.0f;
    setDefinition(r, WeaponBehaviorType::Swordsword, WeaponFireMode::SemiAuto,
                  WeaponNetworkMode::Normal, false, 4u, 35.0f, 1.5f, 0.03f, 0.0f, 0.0f,
                  10.0f, 0, 0, 1, 0.0f, 0.0f, 0.0f, 0.20f, 0.20f, kSwordPhases,
                  (std::uint32_t)(sizeof(kSwordPhases) / sizeof(ToolAnimPhaseV1)));
    setIdentity(r, "swordsword", "Swordsword");
    setBehavior(r, TOOL_BEHAVIOR_MELEE, kEffectSetMelee,
                TOOL_NETWORK_NORMAL, TOOL_COLLISION_CONTACT);
    setOwnsExecution(r);
    return r;
}

// Katana: a brand-new hot tool that reuses the shared physical-melee behavior
// (TOOL_BEHAVIOR_MELEE) and the BlenderPhysical exact-pose clip
// `animation.katana_slash`. No one-off katana damage or renderer branch. The
// model is a placeholder until a katana mesh exists (shared melee collision),
// so the visible weapon matches the damaging contact shape once authored.
ToolVisualRecipeV1 makeKatanaVisual()
{
    ToolVisualRecipeV1 r = makeBaseVisual(
        gameHash("katana"),
        "assets/objects/weapons/mimita-hafs-v1.glb",
        gameHash("mesh.tool.katana"));
    r.viewPosition[0] = 0.35f; r.viewPosition[1] = 0.10f; r.viewPosition[2] = -0.45f;
    r.muzzleOffset[2] = 0.3f;
    r.muzzle.hasLight = 0u;
    r.muzzle.scale = 0.0f;
    r.muzzle.lifetime = 0.0f;
    setDefinition(r, WeaponBehaviorType::Swordsword, WeaponFireMode::SemiAuto,
                  WeaponNetworkMode::Normal, false, 18u, 30.0f, 1.5f, 0.03f, 0.0f,
                  0.0f, 10.0f, 0, 0, 1, 0.0f, 0.0f, 0.0f, 0.20f, 0.20f, kSwordPhases,
                  (std::uint32_t)(sizeof(kSwordPhases) / sizeof(ToolAnimPhaseV1)));
    setIdentity(r, "katana", "Katana");
    setBehavior(r, TOOL_BEHAVIOR_MELEE, kEffectSetMelee,
                TOOL_NETWORK_NORMAL, TOOL_COLLISION_CONTACT);
    setOwnsExecution(r);
    return r;
}

const ToolVisualRecipeV1* findToolVisualByNetworkId(std::uint64_t networkId)
{
    // NETWORK_WEAPON_* family ids (network/packets.h). Plain hot data: the
    // execution router uses this to find a definition's behavior/flag for numeric
    // keys without a cold switch. Re-edit live to add a family.
    switch (networkId) {
        case 1: return findToolVisual(gameHash("revolver"));
        case 2: return findToolVisual(gameHash("godball"));
        case 3: return findToolVisual(gameHash("shotgun"));
        case 4: return findToolVisual(gameHash("swordsword"));
        case 5: return findToolVisual(gameHash("rocket_launcher"));
        case 6: return findToolVisual(gameHash("hafs"));
        case 7: return findToolVisual(gameHash("grenade_launcher"));
        case 8: return findToolVisual(gameHash("aa12"));
        case 9: return findToolVisual(gameHash("spyknife"));
        default: return nullptr;
    }
}

const ToolVisualRecipeV1* findProjectileVisual(std::uint64_t projectileTypeId)
{
    // Network projectile ids (network/packets.h): rocket launcher = 5,
    // grenade launcher = 7. Kept as plain data so a new projectile only needs a
    // recipe, not a renderer branch.
    if (projectileTypeId == 5)
        return findToolVisual(gameHash("rocket_launcher"));
    if (projectileTypeId == 7)
        return findToolVisual(gameHash("grenade_launcher"));
    return nullptr;
}

// Hot-only test tool: does not exist as a cold builtin, so its presence in the
// cold registry proves a brand-new weapon can be added with no cold edit.
ToolVisualRecipeV1 makeHotSelftestGunVisual()
{
    ToolVisualRecipeV1 r = makeBaseVisual(
        gameHash("hot_selftest_gun"),
        "assets/objects/weapons/mimita-revolver-v1.glb",
        gameHash("mesh.tool.hot_selftest_gun"));
    r.worldPosition[0] = 0.30f;
    r.muzzleOffset[2] = 0.22f;
    r.sounds.fire = "revolvershoot";
    setDefinition(r, WeaponBehaviorType::Hitscan, WeaponFireMode::SemiAuto,
                  WeaponNetworkMode::Normal, true, 99u, 7.0f, 2.0f, 0.1f, 1.0f,
                  0.0f, 0.0f, 4, 100, 1, 0.0f, 0.0f, 0.0f, 0.15f, 0.15f, kRevPhases,
                  (std::uint32_t)(sizeof(kRevPhases) / sizeof(ToolAnimPhaseV1)));
    setIdentity(r, "hot_selftest_gun", "Hot Selftest Gun");
    setBehavior(r, TOOL_BEHAVIOR_HITSCAN, kEffectSetBallistic,
                TOOL_NETWORK_NORMAL, TOOL_COLLISION_HITSCAN);
    setOwnsExecution(r);
    return r;
}

// ── Migrated tools (values copied verbatim from config/weapons.json) ──
// Each names a shared behavior family; none adds a renderer or EXE branch.
ToolVisualRecipeV1 makeGodballVisual()
{
    ToolVisualRecipeV1 r = makeBaseVisual(gameHash("godball"), "",
                                          gameHash("mesh.tool.godball"));
    r.muzzle.hasLight = 0u;
    setDefinition(r, WeaponBehaviorType::Godball, WeaponFireMode::Automatic,
                  WeaponNetworkMode::Normal, false, 2u, 1.0f, 1.0f, 0.0f, 0.0f,
                  0.0f, 0.0f, 0, -1, 1, 0.0f, 0.5f, 0.0f, 0.0f, 0.0f, kSwordPhases,
                  (std::uint32_t)(sizeof(kSwordPhases) / sizeof(ToolAnimPhaseV1)));
    r.definition.soundHit = "godballhit";
    setIdentity(r, "godball", "Godball");
    setBehavior(r, TOOL_BEHAVIOR_CONTACT, kEffectSetMelee,
                TOOL_NETWORK_NORMAL, TOOL_COLLISION_CONTACT);
    setOwnsExecution(r);
    return r;
}

ToolVisualRecipeV1 makeOpRevolverVisual()
{
    ToolVisualRecipeV1 r = makeRevolverVisual();
    r.toolKey = gameHash("op_revolver");
    r.meshId = gameHash("mesh.tool.op_revolver");
    setDefinition(r, WeaponBehaviorType::Hitscan, WeaponFireMode::Automatic,
                  WeaponNetworkMode::Normal, true, 5u, 50.0f, 2.0f, 0.001f, 1.0f,
                  0.0f, 99.0f, 999, 1337, 1, 0.0f, 0.0f, 0.0f, 0.12f, 0.18f,
                  kRevPhases,
                  (std::uint32_t)(sizeof(kRevPhases) / sizeof(ToolAnimPhaseV1)));
    setIdentity(r, "op_revolver", "OP Revolver");
    setBehavior(r, TOOL_BEHAVIOR_HITSCAN, kEffectSetBallistic,
                TOOL_NETWORK_NORMAL, TOOL_COLLISION_HITSCAN);
    setOwnsExecution(r);
    return r;
}

ToolVisualRecipeV1 makeAa12Visual()
{
    ToolVisualRecipeV1 r = makeShotgunVisual();
    r.toolKey = gameHash("aa12");
    r.meshId = gameHash("mesh.tool.aa12");
    setDefinition(r, WeaponBehaviorType::Hitscan, WeaponFireMode::Automatic,
                  WeaponNetworkMode::Normal, true, 6u, 12.0f, 2.0f, 0.01f, 1.5f,
                  3.0f, 130.0f, 999, 1337, 15, 0.0f, 0.0f, 0.0f, 0.14f, 0.22f,
                  kShotPhases,
                  (std::uint32_t)(sizeof(kShotPhases) / sizeof(ToolAnimPhaseV1)));
    setIdentity(r, "aa12", "AA12");
    setBehavior(r, TOOL_BEHAVIOR_PELLET, kEffectSetBallistic,
                TOOL_NETWORK_NORMAL, TOOL_COLLISION_HITSCAN);
    setOwnsExecution(r);
    return r;
}

ToolVisualRecipeV1 makeAdminRevolverVisual()
{
    ToolVisualRecipeV1 r = makeRevolverVisual();
    r.toolKey = gameHash("admin_revolver");
    r.meshId = gameHash("mesh.tool.admin_revolver");
    setDefinition(r, WeaponBehaviorType::Hitscan, WeaponFireMode::Automatic,
                  WeaponNetworkMode::Normal, true, 9u, 50.0f, 2.0f, 0.0001f, 1.0f,
                  3.0f, 99.0f, 9999, 1337, 1, 0.0f, 0.0f, 0.0f, 0.12f, 0.18f,
                  kRevPhases,
                  (std::uint32_t)(sizeof(kRevPhases) / sizeof(ToolAnimPhaseV1)));
    setIdentity(r, "admin_revolver", "Admin Revolver");
    setBehavior(r, TOOL_BEHAVIOR_HITSCAN, kEffectSetBallistic,
                TOOL_NETWORK_NORMAL, TOOL_COLLISION_HITSCAN);
    setOwnsExecution(r);
    return r;
}

ToolVisualRecipeV1 makeHafsVisual()
{
    ToolVisualRecipeV1 r = makeBaseVisual(
        gameHash("hafs"), "assets/objects/weapons/mimita-hafs-v1.glb",
        gameHash("mesh.tool.hafs"));
    r.viewPosition[0] = -9.0f; r.viewPosition[1] = -3.0f; r.viewPosition[2] = 0.5f;
    r.worldPosition[0] = 0.25f; r.worldPosition[1] = -0.05f; r.worldPosition[2] = -0.25f;
    setEulerDegrees(r.worldRotation, 90.0f, 10.0f, -25.0f);
    setScale(r.viewScaleXYZ, 1.5f, 1.5f, 1.5f);
    r.muzzle.hasLight = 0u;
    r.sounds.fire = "weapon/hafs/hafsswing";
    r.sounds.equip = "weapon/hafs/hafsequip";
    setDefinition(r, WeaponBehaviorType::Hafs, WeaponFireMode::Automatic,
                  WeaponNetworkMode::Normal, false, 10u, 0.0f, 1.0f, 0.0f, 0.0f,
                  0.0f, 0.0f, 0, -1, 1, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, kSwordPhases,
                  (std::uint32_t)(sizeof(kSwordPhases) / sizeof(ToolAnimPhaseV1)));
    setIdentity(r, "hafs", "Hafs");
    setBehavior(r, TOOL_BEHAVIOR_MELEE, kEffectSetMelee,
                TOOL_NETWORK_NORMAL, TOOL_COLLISION_CONTACT);
    setOwnsExecution(r);
    return r;
}

ToolVisualRecipeV1 makeQuickHitVisual()
{
    ToolVisualRecipeV1 r = makeBaseVisual(gameHash("quick_hit"), "",
                                          gameHash("mesh.tool.quick_hit"));
    r.muzzle.hasLight = 0u;
    r.sounds.fire = "entity/falcon/falconhitquick";
    setDefinition(r, WeaponBehaviorType::QuickHit, WeaponFireMode::SemiAuto,
                  WeaponNetworkMode::Normal, false, 11u, 0.0f, 1.0f, 0.0f, 0.0f,
                  0.0f, 0.0f, 0, -1, 1, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, kKnifePhases,
                  (std::uint32_t)(sizeof(kKnifePhases) / sizeof(ToolAnimPhaseV1)));
    r.definition.soundHit = "player_hurt";
    setIdentity(r, "quick_hit", "Quick Hit");
    setBehavior(r, TOOL_BEHAVIOR_MELEE, kEffectSetMelee,
                TOOL_NETWORK_NORMAL, TOOL_COLLISION_CONTACT);
    setOwnsExecution(r);
    return r;
}

ToolVisualRecipeV1 makeThrownGrenadeVisual(const char* id, const char* name,
                                           std::uint32_t slot)
{
    ToolVisualRecipeV1 r = makeBaseVisual(gameHash(id), "",
                                          gameHash("mesh.tool.thrown"));
    r.muzzle.hasLight = 0u;
    r.sounds.fire = "grenadelauncher/grenadelaunchershoot";
    setDefinition(r, WeaponBehaviorType::Grenade, WeaponFireMode::SemiAuto,
                  WeaponNetworkMode::Normal, false, slot, 0.0f, 1.0f, 1.0f, 1.0f,
                  0.0f, 0.0f, 1, 0, 1, 18.0f, 0.12f, 3.0f, 0.0f, 0.0f,
                  kGrenadePhases,
                  (std::uint32_t)(sizeof(kGrenadePhases) /
                                  sizeof(ToolAnimPhaseV1)));
    r.definition.soundDryFire = "ui/click";
    setIdentity(r, id, name);
    setBehavior(r, TOOL_BEHAVIOR_THROWN, kEffectSetProjectile,
                TOOL_NETWORK_NORMAL, TOOL_COLLISION_PROJECTILE);
    setOwnsExecution(r);
    return r;
}

ToolVisualRecipeV1 makeAkVisual()
{
    ToolVisualRecipeV1 r = makeRevolverVisual();
    r.toolKey = gameHash("ak");
    r.meshId = gameHash("mesh.tool.ak");
    setDefinition(r, WeaponBehaviorType::Hitscan, WeaponFireMode::Automatic,
                  WeaponNetworkMode::Normal, true, 16u, 36.0f, 3.0f, 0.1f, 2.5f,
                  1.2f, 1.5f, 30, 1337, 1, 0.0f, 0.0f, 0.0f, 0.12f, 0.18f,
                  kRevPhases,
                  (std::uint32_t)(sizeof(kRevPhases) / sizeof(ToolAnimPhaseV1)));
    setIdentity(r, "ak", "AK");
    setBehavior(r, TOOL_BEHAVIOR_HITSCAN, kEffectSetBallistic,
                TOOL_NETWORK_NORMAL, TOOL_COLLISION_HITSCAN);
    setOwnsExecution(r);
    return r;
}

ToolVisualRecipeV1 makeSniperVisual()
{
    ToolVisualRecipeV1 r = makeShotgunVisual();
    r.toolKey = gameHash("sniper");
    r.meshId = gameHash("mesh.tool.sniper");
    setDefinition(r, WeaponBehaviorType::Hitscan, WeaponFireMode::SemiAuto,
                  WeaponNetworkMode::Normal, true, 17u, 115.0f, 4.0f, 1.2f, 3.0f,
                  0.0f, 4.0f, 5, 1337, 1, 0.0f, 0.0f, 0.0f, 0.12f, 0.18f,
                  kRevPhases,
                  (std::uint32_t)(sizeof(kRevPhases) / sizeof(ToolAnimPhaseV1)));
    setIdentity(r, "sniper", "Sniper");
    setBehavior(r, TOOL_BEHAVIOR_HITSCAN, kEffectSetBallistic,
                TOOL_NETWORK_NORMAL, TOOL_COLLISION_HITSCAN);
    setOwnsExecution(r);
    return r;
}

// The single tool registry. Enumerable so the cold weapon system can register a
// brand-new hot tool without a cold edit.
const ToolVisualRecipeV1* allToolVisuals(std::uint32_t& count)
{
    static ToolVisualRecipeV1 recipes[] = {
        makeRevolverVisual(),
        makeShotgunVisual(),
        makeRocketLauncherVisual(),
        makeGrenadeLauncherVisual(),
        makeSpyknifeVisual(),
        makeSwordswordVisual(),
        makeKatanaVisual(),
        makeHotSelftestGunVisual(),
        // Migrated built-ins: every current weapon resolves to one recipe.
        makeGodballVisual(),
        makeOpRevolverVisual(),
        makeAa12Visual(),
        makeAdminRevolverVisual(),
        makeHafsVisual(),
        makeQuickHitVisual(),
        makeThrownGrenadeVisual("grenade_smoke", "Smoke Grenade", 13u),
        makeThrownGrenadeVisual("grenade_frag", "Frag Grenade", 14u),
        makeThrownGrenadeVisual("grenade_fire", "Fire Grenade", 15u),
        makeAkVisual(),
        makeSniperVisual(),
    };
    count = static_cast<std::uint32_t>(sizeof(recipes) /
                                       sizeof(ToolVisualRecipeV1));
    // JSON is an authored presentation source, not a second renderer.  When
    // selected, copy the old weapon viewmodel/attachment values into the same
    // recipe consumed by the hot renderer.  C++ remains the rollback source.
    struct JsonPresentationCache {
        ToolVisualRecipeV1 baseline[32]{};
        ToolVisualRecipeV1 lastValid[32]{};
        std::string modelPaths[32];
        std::filesystem::file_time_type writeTime{};
        bool baselineReady = false;
        bool jsonReady = false;
    };
    static JsonPresentationCache cache;
    if (!cache.baselineReady) {
        for (std::uint32_t i = 0; i < count; ++i)
            cache.baseline[i] = cache.lastValid[i] = recipes[i];
        cache.baselineReady = true;
    }
    if (MimitaBehavior::weaponSource() == MimitaBehavior::Source::Cpp) {
        for (std::uint32_t i = 0; i < count; ++i)
            recipes[i] = cache.baseline[i];
    } else {
        static const std::filesystem::path path("config/weapons.json");
        std::error_code ec;
        const auto writeTime = std::filesystem::last_write_time(path, ec);
        if (!cache.jsonReady || (!ec && writeTime != cache.writeTime)) {
            nlohmann::json root;
            bool parsed = false;
            std::ifstream file(path);
            if (file) {
                try {
                    file >> root;
                    parsed = root.is_object();
                } catch (...) {
                    parsed = false;
                }
            }
            if (parsed) {
                for (std::uint32_t i = 0; i < count; ++i)
                    recipes[i] = cache.baseline[i];
                for (auto it = root.begin(); it != root.end(); ++it) {
                    if (!it.value().is_object() || !it.value().contains("id"))
                        continue;
                    const std::string id = it.value().value("id", std::string{});
                    if (id.empty())
                        continue;
                    ToolVisualRecipeV1* recipe = nullptr;
                    for (std::uint32_t i = 0; i < count; ++i) {
                        if (recipes[i].toolKey == gameHash(id.c_str())) {
                            recipe = &recipes[i];
                            break;
                        }
                    }
                    if (!recipe)
                        continue;
                    const auto& entry = it.value();
                    if (entry.contains("model") && entry["model"].is_object()) {
                        const std::string model = entry["model"].value(
                            "path", std::string{});
                        if (!model.empty()) {
                            const std::size_t index =
                                static_cast<std::size_t>(recipe - recipes);
                            cache.modelPaths[index] = model;
                            recipe->modelPath = cache.modelPaths[index].c_str();
                        }
                    }
                    if (!entry.contains("viewmodel") ||
                        !entry["viewmodel"].is_object())
                        continue;
                    const auto& vm = entry["viewmodel"];
                    auto readVec3 = [](const nlohmann::json& value, float out[3]) {
                        if (!value.is_array() || value.size() < 3)
                            return false;
                        for (int k = 0; k < 3; ++k)
                            out[k] = value[k].get<float>();
                        return true;
                    };
                    readVec3(vm.value("position", nlohmann::json{}),
                             recipe->viewPosition);
                    float degrees[3]{};
                    if (readVec3(vm.value("rotation_degrees", nlohmann::json{}),
                                 degrees)) {
                        glm::mat4 rotation(1.0f);
                        rotation = glm::rotate(rotation, glm::radians(degrees[0]),
                                               glm::vec3(1, 0, 0));
                        rotation = glm::rotate(rotation, glm::radians(degrees[1]),
                                               glm::vec3(0, 1, 0));
                        rotation = glm::rotate(rotation, glm::radians(degrees[2]),
                                               glm::vec3(0, 0, 1));
                        const glm::quat q = glm::normalize(glm::quat_cast(rotation));
                        recipe->viewRotation[0] = q.x;
                        recipe->viewRotation[1] = q.y;
                        recipe->viewRotation[2] = q.z;
                        recipe->viewRotation[3] = q.w;
                    }
                    if (readVec3(vm.value("scale", nlohmann::json{}),
                                 recipe->viewScaleXYZ))
                        recipe->viewScale = recipe->viewScaleXYZ[0];
                    if (vm.contains("attachment") && vm["attachment"].is_object()) {
                        const auto& attachment = vm["attachment"];
                        readVec3(attachment.value("position", nlohmann::json{}),
                                 recipe->worldPosition);
                        if (readVec3(attachment.value("rotation_degrees",
                                                        nlohmann::json{}),
                                     degrees)) {
                            glm::mat4 rotation(1.0f);
                            rotation = glm::rotate(rotation, glm::radians(degrees[0]),
                                                   glm::vec3(1, 0, 0));
                            rotation = glm::rotate(rotation, glm::radians(degrees[1]),
                                                   glm::vec3(0, 1, 0));
                            rotation = glm::rotate(rotation, glm::radians(degrees[2]),
                                                   glm::vec3(0, 0, 1));
                            const glm::quat q = glm::normalize(glm::quat_cast(rotation));
                            recipe->worldRotation[0] = q.x;
                            recipe->worldRotation[1] = q.y;
                            recipe->worldRotation[2] = q.z;
                            recipe->worldRotation[3] = q.w;
                        }
                    }
                }
                for (std::uint32_t i = 0; i < count; ++i)
                    cache.lastValid[i] = recipes[i];
                cache.jsonReady = true;
                cache.writeTime = writeTime;
            } else if (cache.jsonReady) {
                for (std::uint32_t i = 0; i < count; ++i)
                    recipes[i] = cache.lastValid[i];
            }
        } else {
            for (std::uint32_t i = 0; i < count; ++i)
                recipes[i] = cache.lastValid[i];
        }
    }
    refreshJsonToolAnimations(recipes, count);
    return recipes;
}

const ToolDefinitionV1* findToolDefinition(std::uint64_t toolKey)
{
    const ToolVisualRecipeV1* r = findToolVisual(toolKey);
    return r ? &r->definition : nullptr;
}

const ToolVisualRecipeV1* findToolVisual(std::uint64_t toolKey)
{
    if (toolKey == 0)
        return nullptr;
    std::uint32_t count = 0;
    const ToolVisualRecipeV1* recipes = allToolVisuals(count);
    for (std::uint32_t i = 0; i < count; ++i) {
        if (recipes[i].toolKey == toolKey)
            return &recipes[i];
    }
    return nullptr;
}

ServerDisagreementVisualV1 makeServerDisagreementVisual()
{
    ServerDisagreementVisualV1 r{};
    // Default = the shipping JSON values (uniform small, dark turquoise). Now
    // owned by hot C++; editing this changes the running client with no EXE
    // rebuild and no JSON dependency.
    r.pulseColor[0] = 0.0f; r.pulseColor[1] = 0.8f; r.pulseColor[2] = 0.8f;
    r.pulseColor[3] = 0.6f;
    r.pulseStartScale = 0.1f;
    r.pulseEndScale = 1.0f;
    r.pulseAlpha = 0.6f;
    r.pulseLifetime = 0.6f;
    r.beamHeight = 1.3f;
    r.beamThickness = 0.06f;
    r.beamLifetime = 0.36f;
    r.beamColor[0] = 0.0f; r.beamColor[1] = 0.8f; r.beamColor[2] = 0.8f;
    r.beamColor[3] = 0.4f;
    r.tracerThickness = 0.06f;
    r.tracerLifetime = 0.4f;
    r.tracerColor[0] = 0.0f; r.tracerColor[1] = 1.0f; r.tracerColor[2] = 1.0f;
    r.tracerColor[3] = 0.8f;
    r.textPrefix = "server disagree: ";
    r.textScale = 0.25f;
    r.textZOffset = 0.5f;
    r.textLifetimeExtra = 0.01f;
    r.particleCount = 8;
    r.particleMinSpeed = 1.0f;
    r.particleMaxSpeed = 3.0f;
    r.particleGravity = 2.0f;
    r.particleLifetime = 0.8f;
    r.particleScale = 0.54f;
    r.particleEndScale = 0.08f;
    r.particleAlpha = 0.7f;
    r.particleColor[0] = 0.0f; r.particleColor[1] = 0.8f; r.particleColor[2] = 0.8f;
    r.particleColor[3] = 0.7f;
    r.sound = "serverdisagree";
    r.soundVolumeMin = 0.5f;
    r.soundVolumeMax = 0.9f;
    r.soundRange = 150.0f;
    r.smallCorrectionMag = 0.25f;
    r.largeCorrectionMag = 1.0f;
    r.smallScale = 0.8f;
    r.largeScale = 3.0f;
    r.minEffectInterval = 60.0f / 60.0f;
    for (int i = 0; i < 10; ++i) {
        r.reasonColor[i][0] = 0.0f;
        r.reasonColor[i][1] = 0.8f;
        r.reasonColor[i][2] = 0.8f;
        r.reasonColor[i][3] = 1.0f;
    }
    r.reasonColor[1][0] = 0.0f; r.reasonColor[1][1] = 0.9f; r.reasonColor[1][2] = 0.9f;
    r.reasonColor[2][0] = 0.0f; r.reasonColor[2][1] = 0.7f; r.reasonColor[2][2] = 1.0f;
    r.reasonColor[3][0] = 0.0f; r.reasonColor[3][1] = 1.0f; r.reasonColor[3][2] = 0.8f;
    r.reasonColor[4][0] = 0.2f; r.reasonColor[4][1] = 0.8f; r.reasonColor[4][2] = 1.0f;
    r.reasonColor[5][0] = 0.3f; r.reasonColor[5][1] = 0.9f; r.reasonColor[5][2] = 0.9f;
    r.reasonColor[6][0] = 0.0f; r.reasonColor[6][1] = 0.8f; r.reasonColor[6][2] = 1.0f;
    r.reasonColor[7][0] = 0.2f; r.reasonColor[7][1] = 0.6f; r.reasonColor[7][2] = 0.9f;
    r.reasonColor[8][0] = 0.3f; r.reasonColor[8][1] = 0.7f; r.reasonColor[8][2] = 0.8f;
    r.reasonColor[9][0] = 0.1f; r.reasonColor[9][1] = 0.5f; r.reasonColor[9][2] = 0.8f;
    return r;
}

LocalDisagreementVisualV1 makeLocalDisagreementIndicatorVisual()
{
    LocalDisagreementVisualV1 r{};
    r.arrowColor[0] = 0.0f; r.arrowColor[1] = 0.6f; r.arrowColor[2] = 1.0f;
    r.arrowColor[3] = 0.9f;
    r.arrowLifetime = 1.2f;
    r.arrowThickness = 0.06f;
    r.arrowAlpha = 0.9f;
    r.textColor[0] = 0.0f; r.textColor[1] = 0.6f; r.textColor[2] = 1.0f;
    r.textColor[3] = 1.0f;
    r.textZOffset = 0.3f;
    r.textLifetime = 1.2f;
    r.textScale = 0.25f;
    r.textLabel = "CORRECTED";
    return r;
}

const float* serverDisagreementReasonColor(const ServerDisagreementVisualV1& recipe,
                                           std::uint32_t reason)
{
    if (reason < 10)
        return recipe.reasonColor[reason];
    return recipe.reasonColor[0];
}

namespace {

// tool.definition: map a hot recipe/definition to the plain-data POD the cold
// weapon system reads. No pointers/STL cross; strings are copied into fixed
// buffers. toolKey != 0 selects by key; toolKey == 0 enumerates by index. A
// missing tool returns found = 0 so the cold side keeps JSON/builtin.
bool MIMITA_GAME_CALL toolDefinitionQuery(void*, GameToolDefinitionV1* q)
{
    if (!q || q->structSize != sizeof(GameToolDefinitionV1))
        return false;
    q->found = 0;
    const ToolVisualRecipeV1* r = nullptr;
    if (q->toolKey != 0) {
        r = findToolVisual(q->toolKey);
    } else {
        std::uint32_t count = 0;
        const ToolVisualRecipeV1* recipes = allToolVisuals(count);
        const std::uint32_t index = q->enumerateIndex;
        if (index >= count)
            return true;  // end of list; found stays 0
        r = &recipes[index];
    }
    if (!r)
        return true;
    const ToolDefinitionV1& d = r->definition;
    q->found = 1;
    q->toolKey = r->toolKey;
    q->presentMask = static_cast<std::uint32_t>(d.presentMask);
    q->behaviorType = d.behaviorType;
    q->fireMode = d.fireMode;
    q->networkMode = d.networkMode;
    q->hitscan = d.hitscan;
    q->slot = d.slot;
    q->damage = d.damage;
    q->headshotMultiplier = d.headshotMultiplier;
    q->fireDelay = d.fireDelay;
    q->reloadTime = d.reloadTime;
    q->spread = d.spread;
    q->recoil = d.recoil;
    q->equipPoseTime = d.equipPoseTime;
    q->unequipPoseTime = d.unequipPoseTime;
    q->projectileSpeed = d.projectileSpeed;
    q->projectileRadius = d.projectileRadius;
    q->projectileLifetime = d.projectileLifetime;
    q->magazineSize = d.magazineSize;
    q->reserveAmmo = d.reserveAmmo;
    q->pelletCount = d.pelletCount;
    std::snprintf(q->id, sizeof(q->id), "%s", d.id ? d.id : "");
    std::snprintf(q->displayName, sizeof(q->displayName), "%s",
                  d.displayName ? d.displayName : "");
    std::snprintf(q->modelPath, sizeof(q->modelPath), "%s",
                  r->modelPath ? r->modelPath : "");
    q->socket = r->socket;
    for (int i = 0; i < 3; ++i) {
        q->attachmentPosition[i] = r->worldPosition[i];
        q->attachmentRotation[i] = 0.0f;
    }
    q->scale = r->worldScale;
    std::snprintf(q->soundShoot, sizeof(q->soundShoot), "%s",
                  r->sounds.fire ? r->sounds.fire : "");
    std::snprintf(q->soundReload, sizeof(q->soundReload), "%s",
                  r->sounds.reload ? r->sounds.reload : "");
    std::snprintf(q->soundEquip, sizeof(q->soundEquip), "%s",
                  r->sounds.equip ? r->sounds.equip : "");
    std::snprintf(q->soundUnequip, sizeof(q->soundUnequip), "%s",
                  r->sounds.unequip ? r->sounds.unequip : "");
    std::snprintf(q->soundHit, sizeof(q->soundHit), "%s",
                  d.soundHit ? d.soundHit : "");
    std::snprintf(q->soundDryFire, sizeof(q->soundDryFire), "%s",
                  d.soundDryFire ? d.soundDryFire : "");
    const std::uint32_t paramCount =
        d.paramCount < 8 ? d.paramCount : 8u;
    q->paramCount = paramCount;
    for (std::uint32_t i = 0; i < paramCount; ++i) {
        std::snprintf(q->params[i].key, sizeof(q->params[i].key), "%s",
                      d.params[i].key ? d.params[i].key : "");
        q->params[i].value = d.params[i].value;
    }
    return true;
}

const MimitaHotPackage::CapabilityRegistrar s_toolDefinitionProvider{
    {GAME_CAP_TOOL_DEFINITION, gameHash("sig.tool.definition.v1"), 0,
     reinterpret_cast<void*>(&toolDefinitionQuery), "tool.definition"}};

} // namespace

#endif
