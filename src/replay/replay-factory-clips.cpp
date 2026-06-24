#include "replay-factory.h"

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <ctime>

#include <nlohmann/json.hpp>

#include "combat/weapon-types.h"

using json = nlohmann::json;

HighlightType classifyHighlight(const KillContext& ctx)
{
    if (ctx.roundWinning)
        return HighlightType::RoundWinningKill;

    if (ctx.victimWasAirborne && ctx.killerWasAirborne)
        return HighlightType::AirKill;

    if (ctx.distance > 30.0f)
        return HighlightType::LongRangeKill;

    if (ctx.killCountLast5Sec >= 2)
        return HighlightType::MultiKill;

    if (ctx.weaponId.find("shotgun") != std::string::npos && ctx.distance > 15.0f)
        return HighlightType::ShotgunOneShot;

    return HighlightType::Kill;
}

std::string ReplayFactory::generateClipFilename()
{
    const std::time_t now = std::time(nullptr);
    std::tm localTime{};
#ifdef _WIN32
    localtime_s(&localTime, &now);
#else
    localtime_r(&now, &localTime);
#endif
    char fileName[64];
    std::strftime(fileName, sizeof(fileName), "%Y-%m-%d_%H-%M-%S", &localTime);
    return std::string(fileName) + ".mclip.json";
}

void ReplayFactory::finalizeAndSave(PendingClip& pending)
{
    const uint32_t killTick = pending.killTick;
    const uint32_t preSeconds = 5;
    const uint32_t postSeconds = 3;
    const uint32_t startTick = killTick > preSeconds * ReplayRingBuffer::TickRate
        ? killTick - preSeconds * ReplayRingBuffer::TickRate
        : 0u;
    const uint32_t endTick = killTick + postSeconds * ReplayRingBuffer::TickRate;

    ReplayClip clip = mRing.makeClip(startTick, endTick, killTick,
                                      pending.killerId, pending.victimId);
    if (clip.sceneFrames.empty()) {
        printf("[REPLAY FACTORY] empty clip, not saving\n");
        return;
    }

    // Classify the highlight
    KillContext ctx;
    ctx.killerId = pending.killerId;
    ctx.victimId = pending.victimId;
    ctx.weaponId = pending.weaponId;
    ctx.distance = pending.distance;
    ctx.killerWasAirborne = pending.killerWasAirborne;
    ctx.victimWasAirborne = pending.victimWasAirborne;
    ctx.killCountLast5Sec = pending.killCountInWindow;
    ctx.ticksSinceLastKill = pending.ticksSinceLastKill;
    ctx.roundWinning = pending.roundWinning;
    HighlightType type = classifyHighlight(ctx);

    // Build info
    ReplayClipInfo info;
    info.mapName = std::string(clip.header.mapName);
    info.killerName = pending.killerId;
    info.victimName = pending.victimId;
    info.weaponName = pending.weaponId;
    info.highlightType = type;
    info.durationTicks = clip.header.tickCount;
    info.tickRate = clip.header.tickRate;
    info.killTick = killTick;
    info.distance = pending.distance;
    info.roundWinning = pending.roundWinning;

    // Generate filename with highlight type
    std::time_t now = std::time(nullptr);
    std::tm localTime{};
#ifdef _WIN32
    localtime_s(&localTime, &now);
#else
    localtime_r(&now, &localTime);
#endif
    char fileName[128];
    std::strftime(fileName, sizeof(fileName), "%Y-%m-%d_%H-%M-%S", &localTime);

    std::string typeStr = highlightTypeName(type);
    std::string clipName = std::string(fileName) + "_" + typeStr + ".mclip.json";
    std::string path = (std::filesystem::path("replays") / "clips" / clipName).string();

    // Save
    std::error_code ec;
    std::filesystem::create_directories(std::filesystem::path("replays") / "clips", ec);
    if (!clip.save(path)) {
        printf("[REPLAY FACTORY] failed to save clip: %s\n", path.c_str());
        return;
    }

    info.path = path;
    info.filename = clipName;
    info.timestamp = fileName;

    pending.info = info;
    printf("[REPLAY FACTORY] saved clip: %s  type=%s killer=%s victim=%s weapon=%s dist=%.1f\n",
           path.c_str(), typeStr.c_str(),
           pending.killerId.c_str(), pending.victimId.c_str(),
           pending.weaponId.c_str(), pending.distance);
}
