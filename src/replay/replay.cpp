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
