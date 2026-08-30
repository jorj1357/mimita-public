#include "network/disagreement-visuals.h"
#include "effects/effect-part.h"
#include "audio/audio.h"
#include "debug/debug-log.h"
#include "debug/structured-log.h"
#include "config/networking-config.h"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <glm/glm.hpp>
#include <nlohmann/json.hpp>

namespace MimitaNet {

bool gDisagreementDebug = false;

// ── Disagreement config (hot-reloadable from config/serverdisagree.json) ─

static glm::vec3 readVec3(const nlohmann::json& j)
{
    if (j.is_array() && j.size() >= 3)
        return glm::vec3((float)j[0], (float)j[1], (float)j[2]);
    return glm::vec3(1.0f);
}

struct DisagreementSoundConfig {
    std::string sample = "serverdisagree";
    float volumeMin = 0.5f;
    float volumeMax = 0.9f;
    float volumeRadiusDivisor = 3.0f;
    float pitchVariation = 0.05f;
    float range = 150.0f;
};

struct DisagreementPulseConfig {
    bool enabled = true;
    float startScale = 0.1f;
    float endScale = 1.0f;
    float alpha = 0.6f;
};

struct DisagreementBeamConfig {
    bool enabled = true;
    float heightMultiplier = 1.3f;
    float lifetimeMultiplier = 0.6f;
    float startScale = 0.08f;
    float endScale = 0.02f;
    float alpha = 0.4f;
};

struct DisagreementTracerConfig {
    bool enabled = true;
    float minCorrectionLength = 0.01f;
    float lifetime = 0.4f;
    float startScale = 0.06f;
    float endScale = 0.0f;
    float thickness = 0.06f;
    float endThickness = 0.0f;
    float alpha = 0.8f;
    glm::vec3 color{0.0f, 1.0f, 1.0f};
};

struct DisagreementTextConfig {
    bool enabled = true;
    float zOffset = 0.5f;
    float lifetimeExtra = 0.3f;
    float scale = 0.03f;
    std::string prefix = "SERVER REJECTED HIT: ";
};

struct DisagreementParticleConfig {
    bool enabled = true;
    int count = 8;
    float minSpeed = 1.0f;
    float maxSpeed = 3.0f;
    float minLifetime = 0.5f;
    float maxLifetime = 0.8f;
    float startScale = 0.04f;
    float endScale = 0.08f;
    float alpha = 0.7f;
    float gravity = 2.0f;
};

struct DisagreementSeverityConfig {
    float smallCorrectionMag = 0.25f;
    float largeCorrectionMag = 1.0f;
    float smallRadius = 0.8f;
    float mediumRadius = 1.8f;
    float largeRadius = 3.0f;
    float hitRejectionMinRadius = 4.0f;
    float smallLifetime = 0.6f;
    float mediumLifetime = 1.0f;
    float largeLifetime = 1.5f;
    float hitRejectionMinLifetime = 1.5f;
};

struct DisagreementLocalIndicatorConfig {
    float arrowLifetime = 1.2f;
    float arrowStartScale = 0.04f;
    float arrowEndScale = 0.0f;
    float arrowThickness = 0.04f;
    float arrowEndThickness = 0.0f;
    float arrowAlpha = 0.9f;
    float textZOffset = 0.3f;
    float textLifetime = 1.2f;
    float textScale = 0.25f;
    std::string textLabel = "CORRECTED";
    glm::vec3 color{0.0f, 0.6f, 1.0f};
};

struct DisagreementReasonColorConfig {
    glm::vec3 occludedShot{0.0f, 0.9f, 0.9f};
    glm::vec3 invalidDamage{0.0f, 0.7f, 1.0f};
    glm::vec3 positionCorrection{0.0f, 1.0f, 0.8f};
    glm::vec3 invalidMovement{0.2f, 0.8f, 1.0f};
    glm::vec3 invalidState{0.3f, 0.9f, 0.9f};
    glm::vec3 rewindMiss{0.0f, 0.8f, 1.0f};
    glm::vec3 targetNotFound{0.2f, 0.6f, 0.9f};
    glm::vec3 targetDead{0.3f, 0.7f, 0.8f};
    glm::vec3 selfTarget{0.1f, 0.5f, 0.8f};
    glm::vec3 defaultColor{0.0f, 0.8f, 0.8f};
};

struct DisagreementConfig {
    bool hotReload = true;
    bool rejectAllHits = false;
    int minTicksBetweenEffects = 60;
    DisagreementSoundConfig sound;
    DisagreementPulseConfig pulse;
    DisagreementBeamConfig beam;
    DisagreementTracerConfig tracer;
    DisagreementTextConfig text;
    DisagreementParticleConfig particles;
    DisagreementSeverityConfig severity;
    DisagreementLocalIndicatorConfig localIndicator;
    DisagreementReasonColorConfig reasonColors;
};

static DisagreementConfig gDisagreeConfig;
static std::filesystem::file_time_type gDisagreeConfigLastWrite;
static std::string gDisagreeConfigPath = "config/serverdisagree.json";

static bool loadDisagreementConfig(const std::string& path)
{
    try
    {
        std::ifstream f(path);
        if (!f.is_open())
        {
            Debug::warn(Debug::Category::Networking,
                "[DISAGREE CFG] Cannot open %s\n", path.c_str());
            return false;
        }
        nlohmann::json j;
        f >> j;

        auto loadSound = [&](const nlohmann::json& jc, DisagreementSoundConfig& cfg) {
            if (jc.contains("sample")) cfg.sample = jc["sample"];
            if (jc.contains("volumeMin")) cfg.volumeMin = jc["volumeMin"];
            if (jc.contains("volumeMax")) cfg.volumeMax = jc["volumeMax"];
            if (jc.contains("volumeRadiusDivisor")) cfg.volumeRadiusDivisor = jc["volumeRadiusDivisor"];
            if (jc.contains("pitchVariation")) cfg.pitchVariation = jc["pitchVariation"];
            if (jc.contains("range")) cfg.range = jc["range"];
        };
        auto loadPulse = [&](const nlohmann::json& jc, DisagreementPulseConfig& cfg) {
            if (jc.contains("enabled")) cfg.enabled = jc["enabled"];
            if (jc.contains("startScale")) cfg.startScale = jc["startScale"];
            if (jc.contains("endScale")) cfg.endScale = jc["endScale"];
            if (jc.contains("alpha")) cfg.alpha = jc["alpha"];
        };
        auto loadBeam = [&](const nlohmann::json& jc, DisagreementBeamConfig& cfg) {
            if (jc.contains("enabled")) cfg.enabled = jc["enabled"];
            if (jc.contains("heightMultiplier")) cfg.heightMultiplier = jc["heightMultiplier"];
            if (jc.contains("lifetimeMultiplier")) cfg.lifetimeMultiplier = jc["lifetimeMultiplier"];
            if (jc.contains("startScale")) cfg.startScale = jc["startScale"];
            if (jc.contains("endScale")) cfg.endScale = jc["endScale"];
            if (jc.contains("alpha")) cfg.alpha = jc["alpha"];
        };
        auto loadTracer = [&](const nlohmann::json& jc, DisagreementTracerConfig& cfg) {
            if (jc.contains("enabled")) cfg.enabled = jc["enabled"];
            if (jc.contains("minCorrectionLength")) cfg.minCorrectionLength = jc["minCorrectionLength"];
            if (jc.contains("lifetime")) cfg.lifetime = jc["lifetime"];
            if (jc.contains("startScale")) cfg.startScale = jc["startScale"];
            if (jc.contains("endScale")) cfg.endScale = jc["endScale"];
            if (jc.contains("thickness")) cfg.thickness = jc["thickness"];
            if (jc.contains("endThickness")) cfg.endThickness = jc["endThickness"];
            if (jc.contains("alpha")) cfg.alpha = jc["alpha"];
            if (jc.contains("color")) cfg.color = readVec3(jc["color"]);
        };
        auto loadText = [&](const nlohmann::json& jc, DisagreementTextConfig& cfg) {
            if (jc.contains("enabled")) cfg.enabled = jc["enabled"];
            if (jc.contains("zOffset")) cfg.zOffset = jc["zOffset"];
            if (jc.contains("lifetimeExtra")) cfg.lifetimeExtra = jc["lifetimeExtra"];
            if (jc.contains("scale")) cfg.scale = jc["scale"];
            if (jc.contains("prefix")) cfg.prefix = jc["prefix"];
        };
        auto loadParticles = [&](const nlohmann::json& jc, DisagreementParticleConfig& cfg) {
            if (jc.contains("enabled")) cfg.enabled = jc["enabled"];
            if (jc.contains("count")) cfg.count = jc["count"];
            if (jc.contains("minSpeed")) cfg.minSpeed = jc["minSpeed"];
            if (jc.contains("maxSpeed")) cfg.maxSpeed = jc["maxSpeed"];
            if (jc.contains("minLifetime")) cfg.minLifetime = jc["minLifetime"];
            if (jc.contains("maxLifetime")) cfg.maxLifetime = jc["maxLifetime"];
            if (jc.contains("startScale")) cfg.startScale = jc["startScale"];
            if (jc.contains("endScale")) cfg.endScale = jc["endScale"];
            if (jc.contains("alpha")) cfg.alpha = jc["alpha"];
            if (jc.contains("gravity")) cfg.gravity = jc["gravity"];
        };
        auto loadSeverity = [&](const nlohmann::json& jc, DisagreementSeverityConfig& cfg) {
            if (jc.contains("smallCorrectionMag")) cfg.smallCorrectionMag = jc["smallCorrectionMag"];
            if (jc.contains("largeCorrectionMag")) cfg.largeCorrectionMag = jc["largeCorrectionMag"];
            if (jc.contains("smallRadius")) cfg.smallRadius = jc["smallRadius"];
            if (jc.contains("mediumRadius")) cfg.mediumRadius = jc["mediumRadius"];
            if (jc.contains("largeRadius")) cfg.largeRadius = jc["largeRadius"];
            if (jc.contains("hitRejectionMinRadius")) cfg.hitRejectionMinRadius = jc["hitRejectionMinRadius"];
            if (jc.contains("smallLifetime")) cfg.smallLifetime = jc["smallLifetime"];
            if (jc.contains("mediumLifetime")) cfg.mediumLifetime = jc["mediumLifetime"];
            if (jc.contains("largeLifetime")) cfg.largeLifetime = jc["largeLifetime"];
            if (jc.contains("hitRejectionMinLifetime")) cfg.hitRejectionMinLifetime = jc["hitRejectionMinLifetime"];
        };
        auto loadLocalIndicator = [&](const nlohmann::json& jc, DisagreementLocalIndicatorConfig& cfg) {
            if (jc.contains("arrowLifetime")) cfg.arrowLifetime = jc["arrowLifetime"];
            if (jc.contains("arrowStartScale")) cfg.arrowStartScale = jc["arrowStartScale"];
            if (jc.contains("arrowEndScale")) cfg.arrowEndScale = jc["arrowEndScale"];
            if (jc.contains("arrowThickness")) cfg.arrowThickness = jc["arrowThickness"];
            if (jc.contains("arrowEndThickness")) cfg.arrowEndThickness = jc["arrowEndThickness"];
            if (jc.contains("arrowAlpha")) cfg.arrowAlpha = jc["arrowAlpha"];
            if (jc.contains("textZOffset")) cfg.textZOffset = jc["textZOffset"];
            if (jc.contains("textLifetime")) cfg.textLifetime = jc["textLifetime"];
            if (jc.contains("textScale")) cfg.textScale = jc["textScale"];
            if (jc.contains("textLabel")) cfg.textLabel = jc["textLabel"];
            if (jc.contains("color")) cfg.color = readVec3(jc["color"]);
        };
        auto loadColors = [&](const nlohmann::json& jc, DisagreementReasonColorConfig& cfg) {
            if (jc.contains("OCCLUDED_SHOT")) cfg.occludedShot = readVec3(jc["OCCLUDED_SHOT"]);
            if (jc.contains("INVALID_DAMAGE")) cfg.invalidDamage = readVec3(jc["INVALID_DAMAGE"]);
            if (jc.contains("POSITION_CORRECTION")) cfg.positionCorrection = readVec3(jc["POSITION_CORRECTION"]);
            if (jc.contains("INVALID_MOVEMENT")) cfg.invalidMovement = readVec3(jc["INVALID_MOVEMENT"]);
            if (jc.contains("INVALID_STATE")) cfg.invalidState = readVec3(jc["INVALID_STATE"]);
            if (jc.contains("REWIND_MISS")) cfg.rewindMiss = readVec3(jc["REWIND_MISS"]);
            if (jc.contains("TARGET_NOT_FOUND")) cfg.targetNotFound = readVec3(jc["TARGET_NOT_FOUND"]);
            if (jc.contains("TARGET_DEAD")) cfg.targetDead = readVec3(jc["TARGET_DEAD"]);
            if (jc.contains("SELF_TARGET")) cfg.selfTarget = readVec3(jc["SELF_TARGET"]);
            if (jc.contains("DEFAULT")) cfg.defaultColor = readVec3(jc["DEFAULT"]);
        };

        if (j.contains("hotReload")) gDisagreeConfig.hotReload = j["hotReload"];
        if (j.contains("rejectAllHits")) gDisagreeConfig.rejectAllHits = j["rejectAllHits"];
        if (j.contains("minTicksBetweenEffects"))
        {
            const int minTicks = j["minTicksBetweenEffects"];
            gDisagreeConfig.minTicksBetweenEffects = std::max(1, minTicks);
        }
        if (j.contains("sound")) loadSound(j["sound"], gDisagreeConfig.sound);
        if (j.contains("pulse")) loadPulse(j["pulse"], gDisagreeConfig.pulse);
        if (j.contains("beam")) loadBeam(j["beam"], gDisagreeConfig.beam);
        if (j.contains("tracer")) loadTracer(j["tracer"], gDisagreeConfig.tracer);
        if (j.contains("text")) loadText(j["text"], gDisagreeConfig.text);
        if (j.contains("particles")) loadParticles(j["particles"], gDisagreeConfig.particles);
        if (j.contains("severity")) loadSeverity(j["severity"], gDisagreeConfig.severity);
        if (j.contains("localIndicator")) loadLocalIndicator(j["localIndicator"], gDisagreeConfig.localIndicator);
        if (j.contains("reasonColors")) loadColors(j["reasonColors"], gDisagreeConfig.reasonColors);

        std::error_code ec;
        gDisagreeConfigLastWrite = std::filesystem::last_write_time(path, ec);
        Debug::warn(Debug::Category::Networking, "[DISAGREE CFG] Loaded: %s\n", path.c_str());
        return true;
    }
    catch (const std::exception& e)
    {
        Debug::warn(Debug::Category::Networking,
            "[DISAGREE CFG] Parse error in %s: %s\n", path.c_str(), e.what());
        return false;
    }
}

void pollDisagreementReload()
{
    static bool initialLoad = false;
    if (!initialLoad) {
        initialLoad = true;
        loadDisagreementConfig(gDisagreeConfigPath);
    }
    if (!gDisagreeConfig.hotReload || gDisagreeConfigPath.empty()) return;
    using Clock = std::chrono::steady_clock;
    static Clock::time_point nextCheck;
    const auto now = Clock::now();
    if (now < nextCheck) return;
    nextCheck = now + std::chrono::milliseconds(100);

    std::error_code ec;
    auto wt = std::filesystem::last_write_time(gDisagreeConfigPath, ec);
    if (ec) return;
    if (wt != gDisagreeConfigLastWrite) {
        gDisagreeConfigLastWrite = wt;
        Debug::warn(Debug::Category::Networking, "[DISAGREE CFG] Detected change. Reloading...\n");
        loadDisagreementConfig(gDisagreeConfigPath);
    }
}

// ── Static helpers ─────────────────────────────────────────────────────

static const char* reasonName(DisagreementReason reason)
{
    switch (reason)
    {
        case DISAGREEMENT_OCCLUDED_SHOT:  return "occluded shot";
        case DISAGREEMENT_INVALID_DAMAGE: return "invalid damage";
        case DISAGREEMENT_POSITION_CORRECTION: return "position correction";
        case DISAGREEMENT_INVALID_MOVEMENT: return "invalid movement";
        case DISAGREEMENT_INVALID_STATE: return "invalid state";
        case DISAGREEMENT_REWIND_MISS:   return "rewind miss";
        case DISAGREEMENT_TARGET_NOT_FOUND: return "target not found";
        case DISAGREEMENT_TARGET_DEAD:   return "target dead";
        case DISAGREEMENT_SELF_TARGET:   return "self target";
        default: return "unknown";
    }
}

static const char* reasonLabel(DisagreementReason reason)
{
    switch (reason)
    {
        case DISAGREEMENT_OCCLUDED_SHOT:  return "OCCLUDED SHOT";
        case DISAGREEMENT_INVALID_DAMAGE: return "INVALID DAMAGE";
        case DISAGREEMENT_POSITION_CORRECTION: return "POSITION CORRECTION";
        case DISAGREEMENT_INVALID_MOVEMENT: return "INVALID MOVEMENT";
        case DISAGREEMENT_INVALID_STATE: return "INVALID STATE";
        case DISAGREEMENT_REWIND_MISS:   return "REWIND MISS";
        case DISAGREEMENT_TARGET_NOT_FOUND: return "TARGET NOT FOUND";
        case DISAGREEMENT_TARGET_DEAD:   return "TARGET DEAD";
        case DISAGREEMENT_SELF_TARGET:   return "SELF TARGET";
        default: return "SERVER CORRECTION";
    }
}

// ── Public API ─────────────────────────────────────────────────────────

void spawnDisagreementEffect(const DisagreementEvent& event)
{
    if (!NetworkingConfig::instance().data().disagreement.enabled)
        return;
    const auto& cfg = gDisagreeConfig;
    // Uniform presentation: every disagreement uses the same small size,
    // lifetime, and dark-turquoise color. Only the text popup varies.
    const glm::vec3 color = cfg.reasonColors.defaultColor;
    const float radius = cfg.severity.smallRadius;
    const float lifetime = cfg.severity.smallLifetime;
    const glm::vec3 pos = event.position;

    // Sound
    {
        const auto& s = cfg.sound;
        float vol = s.volumeMin + (s.volumeMax - s.volumeMin) * (radius / s.volumeRadiusDivisor);
        unsigned int h = (unsigned int)(pos.x * 73856093)
            ^ (unsigned int)(pos.y * 19349663)
            ^ (unsigned int)(pos.z * 83492791);
        float pitch = 1.0f + ((float)(h % 101) - 50.0f) / 1000.0f * s.pitchVariation;
        playWorldSound(s.sample, pos, vol, pitch, s.range);
    }

    // Expanding sphere pulse
    if (cfg.pulse.enabled)
    {
        EffectPart e;
        e.position = pos;
        e.color = color;
        e.maxLifetime = lifetime;
        e.scale = cfg.pulse.startScale;
        e.endScale = radius * cfg.pulse.endScale;
        e.billboardText = false;
        e.alpha = cfg.pulse.alpha;
        e.replayType = "server_disagreement_pulse";
        EffectPartSystem::instance().spawn(e);
    }

    // Vertical beam
    if (cfg.beam.enabled)
    {
        EffectPart e;
        e.position = pos;
        e.endPosition = pos + glm::vec3(0, 0, radius * cfg.beam.heightMultiplier);
        e.color = color;
        e.maxLifetime = lifetime * cfg.beam.lifetimeMultiplier;
        e.scale = cfg.beam.startScale;
        e.endScale = cfg.beam.endScale;
        e.alpha = cfg.beam.alpha;
        e.beam = true;
        e.replayType = "server_disagreement_beam";
        EffectPartSystem::instance().spawn(e);
    }

    // Correction tracer
    if (cfg.tracer.enabled && glm::length(event.correction) > cfg.tracer.minCorrectionLength)
    {
        EffectPart e;
        e.position = pos;
        e.endPosition = pos + event.correction;
        e.color = color;
        e.maxLifetime = cfg.tracer.lifetime;
        e.scale = cfg.tracer.startScale;
        e.endScale = cfg.tracer.endScale;
        e.thickness = cfg.tracer.thickness;
        e.endThickness = cfg.tracer.endThickness;
        e.alpha = cfg.tracer.alpha;
        e.beam = true;
        e.replayType = "server_disagreement_tracer";
        EffectPartSystem::instance().spawn(e);
    }

    // Damage-number-style text popup
    if (cfg.text.enabled)
    {
        EffectPart e;
        e.position = pos + glm::vec3(0, 0, cfg.text.zOffset);
        e.color = color;
        e.maxLifetime = lifetime + cfg.text.lifetimeExtra;
        if (event.descriptionIsFinalLabel)
            e.label = event.description;
        else if (!event.description.empty())
            e.label = cfg.text.prefix + event.description;
        else
            e.label = cfg.text.prefix + reasonLabel(event.reason);
        e.billboardText = true;
        e.scale = cfg.text.scale;
        e.replayType = "server_disagreement_text";
        EffectPartSystem::instance().spawn(e);
    }

    // Particle burst
    if (cfg.particles.enabled)
    {
        unsigned int h = (unsigned int)(pos.x * 73856093)
            ^ (unsigned int)(pos.y * 19349663)
            ^ (unsigned int)(pos.z * 83492791);
        const auto& pcfg = cfg.particles;
        for (int i = 0; i < std::max(pcfg.count, 0); ++i)
        {
            unsigned int si = h + (unsigned int)i * 769u;
            float angle = (float)(si % 6283) / 1000.0f;
            float elevation = ((float)((si * 311u) % 1000) / 1000.0f - 0.5f) * 3.14f;
            float speed = pcfg.minSpeed + ((float)((si * 503u) % 500) / 500.0f) * (pcfg.maxSpeed - pcfg.minSpeed);
            glm::vec3 dir(
                std::cos(angle) * std::cos(elevation),
                std::sin(angle) * std::cos(elevation),
                std::sin(elevation)
            );
            EffectPart e;
            e.position = pos;
            e.velocity = dir * speed;
            e.color = color;
            e.maxLifetime = pcfg.minLifetime + ((float)((si * 211u) % 200) / 200.0f) * (pcfg.maxLifetime - pcfg.minLifetime);
            e.scale = pcfg.startScale;
            e.endScale = pcfg.endScale;
            e.billboardText = false;
            e.alpha = pcfg.alpha;
            e.gravity = pcfg.gravity;
            e.affectedByGravity = true;
            e.replayType = "server_disagreement_particle";
            EffectPartSystem::instance().spawn(e);
        }
    }

    Debug::warn(Debug::Category::Networking,
        "[SERVER DISAGREEMENT] eventId=%u shotSerial=%u shooter=%u target=%u "
        "reason=%s radius=%.1f lifetime=%.1f sound=serverdisagree "
        "pos=(%.1f,%.1f,%.1f) correction=(%.2f,%.2f,%.2f) mag=%.2f desc=\"%s\"\n",
        event.eventId, event.relatedSerial, event.sourcePlayerId, event.targetPlayerId,
        reasonName(event.reason),
        radius, lifetime,
        pos.x, pos.y, pos.z,
        event.correction.x, event.correction.y, event.correction.z,
        glm::length(event.correction),
        event.description.c_str());
}

void logDisagreement(const DisagreementEvent& event)
{
    if (gDisagreementDebug)
    {
        Debug::warn(Debug::Category::Networking,
            "[NET DISAGREEMENT] %s at (%.1f,%.1f,%.1f) correction=(%.1f,%.1f,%.1f) %s\n",
            reasonName(event.reason),
            event.position.x, event.position.y, event.position.z,
            event.correction.x, event.correction.y, event.correction.z,
            event.description.c_str());
    }

    // Always-on structured log entry: surfaces "where I was/shots I fired vs
    // where the server thinks they were" with the exact numbers into the
    // Network log file, gated by debuglogger.json "network" level.
    if (::StructuredLogger::instance().shouldLog(
            ::StructuredCategory::Network, ::StructuredLevel::Verbose))
    {
        char buf[512];
        snprintf(buf, sizeof(buf),
            "reason=%s source=%u target=%u pos=(%.1f,%.1f,%.1f) "
            "correction=(%.1f,%.1f,%.1f) %s",
            reasonName(event.reason), event.sourcePlayerId, event.targetPlayerId,
            event.position.x, event.position.y, event.position.z,
            event.correction.x, event.correction.y, event.correction.z,
            event.description.c_str());
        ::StructuredLogger::Entry e;
        e.category = ::StructuredCategory::Network;
        e.level = ::StructuredLevel::Verbose;
        e.eventId = "disagreement";
        e.correlationId = std::to_string(event.eventId);
        e.reason = reasonName(event.reason);
        e.sourceFile = __FILE__;
        e.sourceLine = __LINE__;
        e.functionName = __FUNCTION__;
        e.message = buf;
        ::StructuredLogger::instance().write(e);
    }
}

void spawnLocalDisagreementIndicator(const DisagreementEvent& event)
{
    if (!NetworkingConfig::instance().data().disagreement.enabled)
        return;
    const auto& li = gDisagreeConfig.localIndicator;

    // Correction arrow from predicted to corrected
    EffectPart tracer;
    tracer.position = event.position;
    tracer.endPosition = event.position + event.correction;
    tracer.color = li.color;
    tracer.maxLifetime = li.arrowLifetime;
    tracer.scale = li.arrowStartScale;
    tracer.endScale = li.arrowEndScale;
    tracer.thickness = li.arrowThickness;
    tracer.endThickness = li.arrowEndThickness;
    tracer.alpha = li.arrowAlpha;
    tracer.beam = true;
    tracer.replayType = "local_disagreement_arrow";
    EffectPartSystem::instance().spawn(tracer);

    // Local correction text (only visible to corrected player)
    EffectPart text;
    text.position = event.position + glm::vec3(0, 0, li.textZOffset);
    text.color = li.color;
    text.maxLifetime = li.textLifetime;
    text.label = li.textLabel;
    text.billboardText = true;
    text.scale = li.textScale;
    text.replayType = "local_disagreement_text";
    EffectPartSystem::instance().spawn(text);

    Debug::warn(Debug::Category::Networking,
        "[LOCAL CORRECTION] corrected=(%.1f,%.1f,%.1f) offset=(%.1f,%.1f,%.1f)\n",
        event.position.x + event.correction.x,
        event.position.y + event.correction.y,
        event.position.z + event.correction.z,
        event.correction.x, event.correction.y, event.correction.z);
}

bool isRejectAllHitsEnabled()
{
    return gDisagreeConfig.rejectAllHits;
}

int disagreementMinTicks()
{
    return gDisagreeConfig.minTicksBetweenEffects;
}

} // namespace MimitaNet
