#include "replay.h"
#include "replay-io.h"
#include "debug/debug-log.h"

#include <cstdio>

// Global state definitions
ReplayRecorder* gActiveReplayRecorder = nullptr;
bool gReplayCaptureEnabled = true;
ReplayFactoryNotifyFn gReplayFactoryNotifyFn = nullptr;

void setActiveReplayRecorder(ReplayRecorder* recorder)
{
    gActiveReplayRecorder = recorder;
}

void setReplayCaptureEnabled(bool enabled)
{
    gReplayCaptureEnabled = enabled;
}

void setReplayFactoryNotifyFn(ReplayFactoryNotifyFn fn)
{
    gReplayFactoryNotifyFn = fn;
}

void notifyReplayKill(const std::string& killerId,
                      const std::string& victimId,
                      bool roundWinning)
{
    if (gReplayFactoryNotifyFn)
        gReplayFactoryNotifyFn(killerId, victimId, false, false, roundWinning);
}

std::string saveInstantReplay(ReplayRingBuffer& ring, uint32_t durationSeconds)
{
    if (!ring.isRecording() || ring.currentTick() == 0) {
        Debug::warn(Debug::Category::Replay,
            "[SAVE-CLIP] FAILED: not recording=%d currentTick=%u\n",
            (int)ring.isRecording(), ring.currentTick());
        return {};
    }

    const uint32_t tickRate = ReplayRingBuffer::TickRate;
    const uint32_t desiredTicks = durationSeconds * tickRate;
    const uint32_t currentTick = ring.currentTick();
    const uint32_t startTick = currentTick > desiredTicks
        ? currentTick - desiredTicks
        : 0;

    Debug::warn(Debug::Category::Replay,
        "[SAVE-CLIP] creating clip: startTick=%u currentTick=%u desiredTicks=%u tickRate=%u\n",
        startTick, currentTick, desiredTicks, tickRate);

    ReplayClip clip = ring.makeClip(startTick, currentTick, 0, "", "");
    if (clip.sceneFrames.empty() && clip.frames.empty()) {
        Debug::warn(Debug::Category::Replay,
            "[SAVE-CLIP] FAILED: empty clip (sceneFrames=%zu frames=%zu)\n",
            clip.sceneFrames.size(), clip.frames.size());
        return {};
    }

    Debug::warn(Debug::Category::Replay,
        "[SAVE-CLIP] clip created: sceneFrames=%zu frames=%zu soundEvents=%zu\n",
        clip.sceneFrames.size(), clip.frames.size(), clip.soundEvents.size());

    const std::string path = generateReplayExportPath();
    Debug::warn(Debug::Category::Replay,
        "[SAVE-CLIP] saving clip to: %s\n", path.c_str());

    if (clip.save(path)) {
        Debug::warn(Debug::Category::Replay,
            "[SAVE-CLIP] clip saved OK: %s\n", path.c_str());
        return path;
    }
    Debug::warn(Debug::Category::Replay,
        "[SAVE-CLIP] FAILED: clip.save() returned false for %s\n", path.c_str());
    return {};
}
