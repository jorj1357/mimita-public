#include "replay.h"
#include "replay-io.h"

#include <cstdio>

// Global state definitions
ReplayRecorder* gActiveReplayRecorder = nullptr;
ReplayClipSaver* gActiveReplayClipSaver = nullptr;
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

void setActiveReplayClipSaver(ReplayClipSaver* saver)
{
    gActiveReplayClipSaver = saver;
}

void setReplayFactoryNotifyFn(ReplayFactoryNotifyFn fn)
{
    gReplayFactoryNotifyFn = fn;
}

void notifyReplayKill(const std::string& killerId,
                      const std::string& victimId,
                      bool roundWinning)
{
    if (gActiveReplayClipSaver)
        gActiveReplayClipSaver->notifyKill(killerId, victimId, roundWinning);
    if (gReplayFactoryNotifyFn)
        gReplayFactoryNotifyFn(killerId, victimId, false, false, roundWinning);
}

std::string saveInstantReplay(ReplayRingBuffer& ring, uint32_t durationSeconds)
{
    if (!ring.isRecording() || ring.currentTick() == 0)
        return {};

    const uint32_t tickRate = ReplayRingBuffer::TickRate;
    const uint32_t desiredTicks = durationSeconds * tickRate;
    const uint32_t currentTick = ring.currentTick();
    const uint32_t startTick = currentTick > desiredTicks
        ? currentTick - desiredTicks
        : 0;

    ReplayClip clip = ring.makeClip(startTick, currentTick, 0, "", "");
    if (clip.sceneFrames.empty() && clip.frames.empty())
        return {};

    const std::string path = generateReplayExportPath();
    if (clip.save(path))
        return path;
    return {};
}
