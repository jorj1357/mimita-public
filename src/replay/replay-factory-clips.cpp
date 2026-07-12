#include "replay-factory.h"
#include "replay-factory-worker.h"

#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <ctime>
#include <vector>
#include <thread>

#include <nlohmann/json.hpp>

#include "combat/weapon-types.h"

using json = nlohmann::json;

// ── Prune helper — runs on worker thread ──────────────────────────
static void enforceClipStorageLimit()
{
    namespace fs = std::filesystem;
    constexpr uint64_t MAX_BYTES = 100ULL * 1024 * 1024;
    fs::path clipsDir = fs::path("replays") / "clips";
    std::error_code ec;

    if (!fs::exists(clipsDir, ec)) return;

    struct ClipEntry {
        fs::path path;
        fs::file_time_type time;
        uint64_t size;
    };
    std::vector<ClipEntry> clips;
    uint64_t totalBytes = 0;

    for (const auto& entry : fs::directory_iterator(clipsDir, ec)) {
        if (!entry.is_regular_file(ec)) continue;
        auto ext = entry.path().extension().string();
        if (ext != ".json") continue;
        uint64_t sz = entry.file_size(ec);
        totalBytes += sz;
        clips.push_back({entry.path(), entry.last_write_time(ec), sz});
    }

    if (totalBytes <= MAX_BYTES) return;

    std::sort(clips.begin(), clips.end(),
        [](const ClipEntry& a, const ClipEntry& b) { return a.time < b.time; });

    printf("[REPLAY CLIP] clips directory = %llu bytes (limit %llu), pruning %zu files\n",
           (unsigned long long)totalBytes, (unsigned long long)MAX_BYTES, clips.size());
    for (const auto& clip : clips) {
        if (totalBytes <= MAX_BYTES) break;
        fs::remove(clip.path, ec);
        totalBytes -= clip.size;
        printf("[REPLAY CLIP] deleted: %s\n", clip.path.filename().string().c_str());
    }
    printf("[REPLAY CLIP] pruning done, clips directory = %llu bytes\n",
           (unsigned long long)totalBytes);
}

// ── Worker job: serialize + write + prune ─────────────────────────
static void saveClipJob(ReplayClip clip, std::string path)
{
    printf("[REPLAY WORKER] executing save job thread=%zx\n",
           std::hash<std::thread::id>{}(std::this_thread::get_id()));

    // Create directories
    std::error_code ec;
    std::filesystem::create_directories(std::filesystem::path("replays") / "clips", ec);

    // Serialize and write
    if (!clip.save(path)) {
        printf("[REPLAY FACTORY] failed to save clip: %s\n", path.c_str());
        return;
    }

    printf("[REPLAY FACTORY] saved clip: %s\n", path.c_str());

    // Prune
    enforceClipStorageLimit();
}

// ── Highlight classification ──────────────────────────────────────
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

    // Build info (for caller reference — not used by worker)
    pending.info.mapName = std::string(clip.header.mapName);
    pending.info.killerName = pending.killerId;
    pending.info.victimName = pending.victimId;
    pending.info.weaponName = pending.weaponId;
    pending.info.highlightType = type;
    pending.info.durationTicks = clip.header.tickCount;
    pending.info.tickRate = clip.header.tickRate;
    pending.info.killTick = killTick;
    pending.info.distance = pending.distance;
    pending.info.roundWinning = pending.roundWinning;

    // Generate filename with timestamp and killTick to avoid collisions
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
    std::string clipName = std::string(fileName) + "_" + std::to_string(killTick) + "_" + typeStr + ".mclip.json";
    std::string path = (std::filesystem::path("replays") / "clips" / clipName).string();

    pending.info.path = path;
    pending.info.filename = clipName;
    pending.info.timestamp = fileName;

    printf("[REPLAY FACTORY] enqueuing clip save: %s  type=%s killer=%s victim=%s weapon=%s dist=%.1f\n",
           path.c_str(), typeStr.c_str(),
           pending.killerId.c_str(), pending.victimId.c_str(),
           pending.weaponId.c_str(), pending.distance);

    // ── Offload everything to background worker ──────────────────
    if (!mWorker) {
        printf("[REPLAY FACTORY] ERROR: worker missing, refusing synchronous replay save\n");
        return;
    }

    mWorker->enqueue(
        [clip = std::move(clip), path = std::move(path)]() mutable
        {
            saveClipJob(std::move(clip), std::move(path));
        });
}
