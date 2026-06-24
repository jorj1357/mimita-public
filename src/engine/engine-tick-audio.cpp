#include "engine/engine-tick-audio.h"
#include "terminal/terminal-state.h"
#include "perf/perf.h"
#include "audio/audio.h"
#include "audio/music-manager.h"

void engineTickAudio(float dt)
{
    { Perf::ScopedTimer _aud("Audio");
    audioUpdate(dt);
    MusicManager::instance().update(dt);
    }
}
