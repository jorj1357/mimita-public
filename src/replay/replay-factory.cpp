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

std::vector<ReplayClipInfo> scanSavedClips()
{
    std::vector<ReplayClipInfo> result;
    std::error_code ec;
    const std::filesystem::path baseDir = std::filesystem::path("replays");
    if (!std::filesystem::exists(baseDir, ec))
        return result;

    std::vector<std::pair<std::filesystem::file_time_type, std::string>> found;
    for (const auto& entry : std::filesystem::recursive_directory_iterator(baseDir, ec)) {
        if (ec || !entry.is_regular_file())
            continue;
        const std::string name = entry.path().filename().string();
        if (name.size() > 11 && name.rfind(".mclip.json") == name.size() - 11)
            found.push_back({entry.last_write_time(ec), entry.path().string()});
        else if (name.size() > 5 && name.rfind(".json") == name.size() - 5 &&
                 name.find("-replay.json") != std::string::npos)
            found.push_back({entry.last_write_time(ec), entry.path().string()});
    }

    std::sort(found.begin(), found.end(),
              [](const auto& a, const auto& b) { return a.first > b.first; });

    for (const auto& item : found) {
        ReplayClipInfo info;
        if (loadClipInfo(item.second, info))
            result.push_back(std::move(info));
    }
    return result;
}

std::vector<ReplayEventItem> buildEventTimeline(
    const std::vector<ReplaySceneFrame>& frames,
    const std::vector<ReplaySoundEvent>& sounds)
{
    std::vector<ReplayEventItem> events;
    std::set<std::string> activeWeapons;

    for (const ReplaySceneFrame& frame : frames) {
        for (const ReplayActorState& actor : frame.actors) {
            if (actor.dead && actor.health <= 0) {
                ReplayEventItem e;
                e.type = ReplayEventType::Death;
                e.tick = (uint32_t)frame.tick;
                e.label = actor.name + " died";
                e.detail = actor.type;
                e.color = {1.0f, 0.2f, 0.2f, 1.0f};
                events.push_back(e);
            }

            if (!actor.weaponName.empty() && actor.weaponName != "none") {
                if (activeWeapons.find(actor.id + ":" + actor.weaponName) == activeWeapons.end()) {
                    activeWeapons.insert(actor.id + ":" + actor.weaponName);
                    ReplayEventItem e;
                    e.type = ReplayEventType::WeaponSwitch;
                    e.tick = (uint32_t)frame.tick;
                    e.label = actor.name + " switched to " + actor.weaponName;
                    e.color = {0.3f, 0.7f, 1.0f, 1.0f};
                    events.push_back(e);
                }
            }
        }

        for (const ReplayEffectEvent& effect : frame.effects) {
            if (effect.type == "blood_spurt_emitter" && !effect.sourceActorId.empty()) {
                ReplayEventItem e;
                e.type = ReplayEventType::Kill;
                e.tick = (uint32_t)frame.tick;
                e.label = effect.sourceActorId + " hit " + effect.targetActorId;
                e.color = {1.0f, 0.6f, 0.1f, 1.0f};
                events.push_back(e);
            }
        }
    }

    for (const ReplaySoundEvent& sound : sounds) {
        const std::string& path = sound.soundPath;
        if (path.find("jump") != std::string::npos || path.find("doublejump") != std::string::npos) {
            ReplayEventItem e;
            e.type = ReplayEventType::Jump;
            e.tick = (uint32_t)sound.tick;
            e.label = "jump";
            e.color = {0.3f, 1.0f, 0.3f, 0.6f};
            events.push_back(e);
        } else if (path.find("dash") != std::string::npos) {
            ReplayEventItem e;
            e.type = ReplayEventType::Dash;
            e.tick = (uint32_t)sound.tick;
            e.label = "dash";
            e.color = {0.3f, 0.3f, 1.0f, 0.6f};
            events.push_back(e);
        } else if (path.find("land") != std::string::npos) {
            ReplayEventItem e;
            e.type = ReplayEventType::Land;
            e.tick = (uint32_t)sound.tick;
            e.label = "land";
            e.color = {0.5f, 0.5f, 0.5f, 0.6f};
            events.push_back(e);
        } else if (path.find("freezebegin") != std::string::npos) {
            ReplayEventItem e;
            e.type = ReplayEventType::FreezeBegin;
            e.tick = (uint32_t)sound.tick;
            e.label = "freeze";
            e.color = {0.0f, 1.0f, 1.0f, 0.6f};
            events.push_back(e);
        }
    }

    std::sort(events.begin(), events.end(),
              [](const ReplayEventItem& a, const ReplayEventItem& b) {
                  return a.tick < b.tick;
              });

    return events;
}

void ReplayBrowser::refresh()
{
    mClips = scanSavedClips();
    mSelectedIndex = -1;
}

void ReplayBrowser::draw()
{
    if (!mOpen) return;

    const float screenW = uiScreenW();
    const float screenH = uiScreenH();
    const float panelW = std::min(800.0f, screenW * 0.85f);
    const float panelH = std::min(600.0f, screenH * 0.85f);
    const float panelX = (screenW - panelW) * 0.5f;
    const float panelY = (screenH - panelH) * 0.5f;

    uiDrawRect({panelX, panelY, panelW, panelH}, {0.05f, 0.05f, 0.08f, 0.95f}, "browser-bg");
    uiDrawRectOutline({panelX, panelY, panelW, panelH}, {0.3f, 0.3f, 0.4f, 1.0f}, "browser-border");

    uiDrawText("REPLAY BROWSER", panelX + 16.0f, panelY + 12.0f, 0.50f, {1.0f, 0.9f, 0.3f, 1.0f});

    UIRect closeBtn = {panelX + panelW - 40.0f, panelY + 8.0f, 32.0f, 28.0f};
    uiDrawRect(closeBtn, {0.3f, 0.05f, 0.05f, 1.0f}, "browser-close");
    uiDrawText("X", closeBtn.x + 8.0f, closeBtn.y + 4.0f, 0.40f, {1.0f, 1.0f, 1.0f, 1.0f});

    UIRect refreshBtn = {panelX + panelW - 90.0f, panelY + 8.0f, 44.0f, 28.0f};
    uiDrawRect(refreshBtn, {0.15f, 0.15f, 0.2f, 1.0f}, "browser-refresh");
    uiDrawText("REFRESH", refreshBtn.x + 2.0f, refreshBtn.y + 4.0f, 0.24f, {1.0f, 1.0f, 1.0f, 1.0f});

    if (mClips.empty()) {
        uiDrawText("No replays found. Press F8 during gameplay to save a clip.",
                   panelX + 20.0f, panelY + panelH * 0.5f - 10.0f, 0.30f, {0.6f, 0.6f, 0.6f, 1.0f});
        return;
    }

    const float listX = panelX + 12.0f;
    const float listW = panelW - 24.0f;
    const float listTopY = panelY + 48.0f;
    const float listBottomY = panelY + panelH - 16.0f;
    const float cardH = 70.0f;
    const float visibleH = listBottomY - listTopY;

    float totalH = (float)mClips.size() * cardH;
    float maxScroll = std::max(0.0f, totalH - visibleH);

    float y = listTopY - mScrollY;
    int drawn = 0;
    for (size_t i = 0; i < mClips.size(); ++i) {
        if (y + cardH < listTopY) { y += cardH; continue; }
        if (y > listBottomY) break;

        bool selected = (int)i == mSelectedIndex;
        glm::vec4 cardColor = selected
            ? glm::vec4{0.2f, 0.25f, 0.35f, 1.0f}
            : (drawn % 2 == 0 ? glm::vec4{0.08f, 0.08f, 0.12f, 1.0f}
                              : glm::vec4{0.1f, 0.1f, 0.14f, 1.0f});
        uiDrawRect({listX, y, listW, cardH - 4.0f}, cardColor, "clip-card");

        const ReplayClipInfo& clip = mClips[i];

        uiDrawRect({listX + 6.0f, y + 6.0f, 100.0f, 56.0f},
                   clip.highlightType == HighlightType::RoundWinningKill
                       ? glm::vec4{0.9f, 0.7f, 0.1f, 1.0f}
                       : glm::vec4{0.15f, 0.15f, 0.2f, 1.0f},
                   "clip-thumb");

        char title[128];
        snprintf(title, sizeof(title), "%s  |  %s",
                 clip.killerName.c_str(), clip.weaponName.c_str());
        uiDrawText(title, listX + 114.0f, y + 6.0f, 0.32f, {1.0f, 1.0f, 1.0f, 1.0f});

        char victimText[128];
        snprintf(victimText, sizeof(victimText), "Victim: %s",
                 clip.victimName.c_str());
        uiDrawText(victimText, listX + 114.0f, y + 26.0f, 0.24f, {0.7f, 0.7f, 0.7f, 1.0f});

        char metaText[128];
        snprintf(metaText, sizeof(metaText), "%.1fs  |  %s  |  %s",
                 clip.durationSeconds(),
                 highlightTypeName(clip.highlightType),
                 clip.timestamp.c_str());
        uiDrawText(metaText, listX + 114.0f, y + 42.0f, 0.22f, {0.5f, 0.5f, 0.6f, 1.0f});

        if (selected) {
            float actionY = y + 12.0f;
            float actionX = listX + listW - 280.0f;

            UIRect playBtn = {actionX, actionY, 60.0f, 22.0f};
            uiDrawRect(playBtn, {0.15f, 0.6f, 0.2f, 1.0f}, "clip-play");
            uiDrawText("PLAY", actionX + 8.0f, actionY + 2.0f, 0.24f, {1.0f, 1.0f, 1.0f, 1.0f});

            UIRect renameBtn = {actionX + 66.0f, actionY, 60.0f, 22.0f};
            uiDrawRect(renameBtn, {0.2f, 0.2f, 0.3f, 1.0f}, "clip-rename");
            uiDrawText("RENAME", renameBtn.x + 2.0f, actionY + 2.0f, 0.24f, {1.0f, 1.0f, 1.0f, 1.0f});

            UIRect delBtn = {actionX + 132.0f, actionY, 60.0f, 22.0f};
            uiDrawRect(delBtn, {0.5f, 0.1f, 0.1f, 1.0f}, "clip-delete");
            uiDrawText("DELETE", delBtn.x + 2.0f, actionY + 2.0f, 0.24f, {1.0f, 1.0f, 1.0f, 1.0f});
        }

        y += cardH;
        drawn++;
    }

    if (maxScroll > 0) {
        float barH = visibleH * (visibleH / totalH);
        float barY = listTopY + (mScrollY / maxScroll) * (visibleH - barH);
        uiDrawRect({panelX + panelW - 6.0f, barY, 4.0f, barH}, {0.4f, 0.4f, 0.5f, 0.8f}, "scroll-bar");
    }

    char stats[64];
    snprintf(stats, sizeof(stats), "%zu clips", mClips.size());
    uiDrawText(stats, panelX + 16.0f, panelY + panelH - 14.0f, 0.24f, {0.4f, 0.4f, 0.5f, 1.0f});
}

void ReplayTimeline::setFrames(const std::vector<ReplaySceneFrame>& frames,
                                const std::vector<ReplaySoundEvent>& sounds)
{
    mEvents = buildEventTimeline(frames, sounds);
}

void ReplayTimeline::draw(uint32_t currentTick, uint32_t totalTicks)
{
    if (totalTicks == 0) return;

    const float screenW = uiScreenW();
    const float screenH = uiScreenH();

    const float barX = 60.0f;
    const float barY = screenH - 50.0f;
    const float barW = screenW - 120.0f;
    const float barH = 24.0f;
    mTimelineWidth = barW;

    uiDrawRect({barX - 10.0f, barY - 20.0f, barW + 20.0f, barH + 60.0f},
               {0.0f, 0.0f, 0.0f, 0.7f}, "timeline-bg");
    uiDrawRect({barX, barY, barW, barH}, {0.15f, 0.15f, 0.2f, 1.0f}, "timeline-track");

    float progress = (float)currentTick / (float)totalTicks;
    uiDrawRect({barX, barY, barW * progress, barH}, {0.4f, 0.6f, 1.0f, 1.0f}, "timeline-fill");

    int markerInterval = std::max(1, (int)totalTicks / 20);
    for (uint32_t t = 0; t <= totalTicks; t += markerInterval) {
        float mx = barX + ((float)t / (float)totalTicks) * barW;
        uiDrawRect({mx, barY, 1.0f, barH * 0.5f}, {0.4f, 0.4f, 0.4f, 1.0f}, "tick-marker");
    }

    for (const ReplayEventItem& event : mEvents) {
        if (event.tick > totalTicks) continue;
        float ex = barX + ((float)event.tick / (float)totalTicks) * barW;
        uiDrawRect({ex - 1.0f, barY - 6.0f, 3.0f, 10.0f}, event.color, "event-marker");

        float distToCurrent = std::abs((float)event.tick - (float)currentTick);
        if (distToCurrent < (float)totalTicks * 0.05f) {
            float labelW = uiMeasureText(event.label.c_str(), 0.20f);
            uiDrawText(event.label.c_str(), ex - labelW * 0.5f, barY - 20.0f,
                       0.20f, event.color);
        }
    }

    float cx = barX + progress * barW;
    uiDrawRect({cx - 4.0f, barY - 8.0f, 8.0f, barH + 16.0f},
               {1.0f, 1.0f, 1.0f, 0.9f}, "timeline-cursor");

    char timeLabel[32];
    float totalTime = (float)totalTicks / 60.0f;
    float currentTime = (float)currentTick / 60.0f;
    snprintf(timeLabel, sizeof(timeLabel), "%.1fs / %.1fs", currentTime, totalTime);
    float labelW = uiMeasureText(timeLabel, 0.28f);
    uiDrawText(timeLabel, cx - labelW * 0.5f, barY + barH + 6.0f, 0.28f, {0.8f, 0.8f, 0.9f, 1.0f});

    char tickLabel[32];
    snprintf(tickLabel, sizeof(tickLabel), "Tick %u / %u", currentTick, totalTicks);
    uiDrawText(tickLabel, barX, barY + barH + 28.0f, 0.22f, {0.5f, 0.5f, 0.6f, 1.0f});

    uiDrawText("[SPACE] pause/resume  [<- ->] step  [R] restart",
               barX + barW - 300.0f, barY + barH + 28.0f, 0.20f, {0.4f, 0.4f, 0.5f, 1.0f});
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
    for (const ReplaySceneFrame& frame : mRing.sceneFrames()) {
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
