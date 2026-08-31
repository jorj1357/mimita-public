#include "replay-factory.h"

#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <algorithm>
#include <ctime>
#include <set>
#include <chrono>

#include <nlohmann/json.hpp>

#include "combat/weapon-types.h"
#include "gui/ui-system.h"

using json = nlohmann::json;

const char* highlightTypeName(HighlightType t)
{
    switch (t) {
        case HighlightType::Kill: return "Kill";
        case HighlightType::RoundWinningKill: return "Round Winner";
        case HighlightType::MultiKill: return "Multi Kill";
        case HighlightType::AirKill: return "Air Kill";
        case HighlightType::LongRangeKill: return "Long Range";
        case HighlightType::ShotgunOneShot: return "Shotgun One-Shot";
        case HighlightType::RevengeKill: return "Revenge";
        case HighlightType::FirstKill: return "First Blood";
        case HighlightType::LastKill: return "Last Kill";
    }
    return "Kill";
}

bool loadClipInfo(const std::string& path, ReplayClipInfo& info)
{
    std::ifstream file(path);
    if (!file.is_open()) return false;

    try {
        json root;
        file >> root;

        info.path = path;
        info.filename = std::filesystem::path(path).filename().string();

        const json metadata = root.value("metadata", json::object());
        info.mapName = metadata.value("mapPath", "");
        info.killerName = metadata.value("killerId", "");
        info.victimName = metadata.value("victimId", "");
        info.killTick = metadata.value("killTick", 0u);

        const json header = root.value("header", json::object());
        info.tickRate = header.value("tickRate", 60u);
        info.durationTicks = header.value("tickCount", 0u);
        std::string mapName = header.value("mapName", "");
        if (info.mapName.empty()) info.mapName = mapName;

        std::error_code ec;
        auto ft = std::filesystem::last_write_time(path, ec);
        if (!ec) {
            auto sysNow = std::chrono::system_clock::now();
            std::time_t timeT = std::chrono::system_clock::to_time_t(sysNow);
            std::tm tm{};
#ifdef _WIN32
            localtime_s(&tm, &timeT);
#else
            localtime_r(&timeT, &tm);
#endif
            char buf[64];
            std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M", &tm);
            info.timestamp = buf;
        }

        file.close();
        return true;
    } catch (...) {
        return false;
    }
}

ReplayFactory::ReplayFactory(ReplayRingBuffer& ring)
    : mRing(ring)
{
}

void ReplayFactory::update()
{
    if (mKillWindowTimer > 0.0f) {
        mKillWindowTimer -= 1.0f / 60.0f;
        if (mKillWindowTimer <= 0.0f) {
            mKillWindowTimer = 0.0f;
            mKillsLast5Sec = 0;
        }
    }

    if (mLastClip && !mLastClip->roundWinning) {
        if (mRing.currentTick() >= mLastClip->killTick + 3u * ReplayRingBuffer::TickRate) {
            finalizeAndSave(*mLastClip);
            mLastClip.reset();
        }
    }
}

void ReplayFactory::notifyKill(const std::string& killerId,
                                const std::string& victimId,
                                bool killerAirborne,
                                bool victimAirborne,
                                bool roundWinning)
{
    mKillsLast5Sec++;
    mKillWindowTimer = 5.0f;
    uint32_t ticksSinceLast = mLastKillTick > 0
        ? mRing.currentTick() - mLastKillTick
        : 9999;
    mLastKillTick = mRing.currentTick();

    std::string weaponId;
    float distance = 0.0f;
    glm::vec3 killerPos, victimPos;
    bool foundKiller = false, foundVictim = false;
    for (uint32_t i = 0; i < mRing.sceneFrameCount(); ++i) {
        const ReplaySceneFrame& frame = mRing.sceneFrameAt(i);
        if ((uint32_t)frame.tick != mRing.currentTick() &&
            (uint32_t)frame.tick < mRing.currentTick() + 2u)
            continue;
        if ((uint32_t)frame.tick > mRing.currentTick() + 5u) break;
        for (const ReplayActorState& actor : frame.actors) {
            if (actor.id == killerId) {
                if (weaponId.empty() && !actor.weaponName.empty() && actor.weaponName != "none")
                    weaponId = actor.weaponName;
                killerPos = actor.position;
                foundKiller = true;
            }
            if (actor.id == victimId) {
                victimPos = actor.position;
                foundVictim = true;
            }
        }
        if (foundKiller && foundVictim) break;
    }
    if (foundKiller && foundVictim)
        distance = glm::length(killerPos - victimPos);

    mLastWeaponId = weaponId;

    PendingClip pending;
    pending.killTick = mRing.currentTick();
    pending.killerId = killerId;
    pending.victimId = victimId;
    pending.weaponId = weaponId;
    pending.distance = distance;
    pending.killerWasAirborne = killerAirborne;
    pending.victimWasAirborne = victimAirborne;
    pending.roundWinning = roundWinning;
    pending.killCountInWindow = mKillsLast5Sec;
    pending.ticksSinceLastKill = ticksSinceLast;

    if (roundWinning) {
        finalizeAndSave(pending);
    } else {
        mLastClip = std::move(pending);
    }
}

bool ReplayFactory::saveLastKill(std::string* savedPath)
{
    (void)savedPath;
    if (!mLastClip) return false;

    const uint32_t requestedEnd = mLastClip->killTick + 3u * ReplayRingBuffer::TickRate;
    if (mRing.currentTick() < requestedEnd) {
        printf("[REPLAY FACTORY] clip save queued until tick=%u\n", requestedEnd);
        return false;
    }

    finalizeAndSave(*mLastClip);
    mLastClip.reset();
    return true;
}

bool ReplayFactory::enqueueInstantReplay(uint32_t durationSeconds, std::string* queuedPath)
{
    if (!mRing.isRecording() || mRing.currentTick() == 0)
        return false;
    const uint32_t desiredTicks = durationSeconds * ReplayRingBuffer::TickRate;
    const uint32_t currentTick = mRing.currentTick();
    const uint32_t startTick = currentTick > desiredTicks ? currentTick - desiredTicks : 0;
    ReplayClip clip = mRing.makeClip(startTick, currentTick, 0, "", "");
    if (clip.sceneFrames.empty() && clip.frames.empty())
        return false;
    const std::string path = generateReplayExportPath();
    if (!enqueueClipSave(std::move(clip), path))
        return false;
    if (queuedPath)
        *queuedPath = path;
    return true;
}
