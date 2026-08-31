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
#include "debug/debug-log.h"

using json = nlohmann::json;

// ── Prune helper — runs on worker thread ──────────────────────────
static void enforceClipStorageLimit()
{
    namespace fs = std::filesystem;
    constexpr uint64_t MAX_BYTES = 100ULL * 1024 * 1024;
    fs::path clipsDir = fs::path("replays") / "clips";

    struct ClipEntry {
        fs::path path;
        fs::file_time_type time;
        uint64_t size;
    };
    std::vector<ClipEntry> clips;
    uint64_t totalBytes = 0;

    try {
        std::error_code ec;
        if (!fs::exists(clipsDir, ec)) return;

        fs::directory_iterator it(clipsDir, ec);
        const fs::directory_iterator endIt;
        for (; !ec && it != endIt; it.increment(ec)) {
            std::error_code fec;
            if (!it->is_regular_file(fec) || fec) continue;
            auto ext = it->path().extension().string();
            if (ext != ".json") continue;
            std::error_code sec;
            uint64_t sz = it->file_size(sec);
            if (sec) continue;
            std::error_code tec;
            fs::file_time_type ft = it->last_write_time(tec);
            if (tec) continue;
            totalBytes += sz;
            clips.push_back({it->path(), ft, sz});
        }

        if (totalBytes <= MAX_BYTES) return;

        std::sort(clips.begin(), clips.end(),
            [](const ClipEntry& a, const ClipEntry& b) { return a.time < b.time; });

        printf("[REPLAY CLIP] clips directory = %llu bytes (limit %llu), pruning %zu files\n",
               (unsigned long long)totalBytes, (unsigned long long)MAX_BYTES, clips.size());
        for (const auto& clip : clips) {
            if (totalBytes <= MAX_BYTES) break;
            std::error_code rec;
            if (fs::remove(clip.path, rec) && !rec) {
                totalBytes -= clip.size;
                printf("[REPLAY CLIP] deleted: %s\n", clip.path.filename().string().c_str());
            }
        }
        printf("[REPLAY CLIP] pruning done, clips directory = %llu bytes\n",
               (unsigned long long)totalBytes);
    } catch (...) {
        printf("[REPLAY CLIP] prune aborted after error\n");
    }
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

    if (!mWorker) {
        printf("[REPLAY FACTORY] ERROR: worker missing, refusing synchronous replay save\n");
        return;
    }

    // Snapshot the ring buffer on the owning gameplay thread. The worker must
    // never read the live ring while the recorder is writing the next tick.
    ReplayClip clip = mRing.makeClip(startTick, endTick, killTick,
                                     pending.killerId, pending.victimId);
    if (clip.sceneFrames.empty()) {
        printf("[REPLAY FACTORY] empty clip, not saving\n");
        return;
    }
    clip.weaponId = pending.weaponId;
    clip.killDistance = pending.distance;

    // Copy only immutable data needed by the worker for classification + naming.
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

    const HighlightType type = classifyHighlight(ctx);
    std::time_t now = std::time(nullptr);
    std::tm localTime{};
#ifdef _WIN32
    localtime_s(&localTime, &now);
#else
    localtime_r(&now, &localTime);
#endif
    char fileName[128];
    std::strftime(fileName, sizeof(fileName), "%Y-%m-%d_%H-%M-%S", &localTime);
    const std::string clipName = std::string(fileName) + "_" + std::to_string(killTick)
        + "_" + highlightTypeName(type) + ".mclip.json";
    const std::string path = (std::filesystem::path("replays") / "clips" / clipName).string();
    const bool queued = enqueueClipSave(std::move(clip), path);
    if (!queued)
        printf("[REPLAY FACTORY] save queue full; replay save dropped\n");
}

bool ReplayFactory::enqueueClipSave(ReplayClip clip, const std::string& path)
{
    if (!mWorker)
        return false;
    const bool queued = mWorker->enqueue(
        [clip = std::move(clip), path]() mutable {
            saveClipJob(std::move(clip), path);
        });
    Debug::log(queued ? Debug::Category::Replay : Debug::Category::General,
        "[PERF][REPLAY_SAVE] queued=%d queueDepth=%zu path=%s\n",
        queued ? 1 : 0, mWorker->queueDepth(), path.c_str());
    return queued;
}
