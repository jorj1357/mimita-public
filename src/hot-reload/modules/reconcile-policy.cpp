// 09 15 2026
/* purpose
* net.reconcile: the ONE hot reconciliation policy. Given predicted vs
* authoritative facts, it owns the error metric, thresholds, and the
* snap/smooth/hard-reset decision. The cold client mechanism still gathers the
* facts and applies the chosen correction. Generation mismatch (predicted vs
* authoritative produced by different hot behavior generations) forces a
* conservative hard reset instead of treating it as normal simulation drift.
* Does NOT link into the EXE; only into the replaceable game DLL.
*/
#if defined(MIMITA_GAME_DLL)

#include "hot-reload/game-api.h"
#include "hot-reload/hot-package.h"
#include "hot-reload/hot-reconciliation.h"

namespace {

void MIMITA_GAME_CALL onReconcile(void* /*host*/, const GameEventV1* event)
{
    auto* r = event ? static_cast<GameReconcileV1*>(event->payload) : nullptr;
    if (!r)
        return;

    r->shouldCorrect = 0u;
    r->correctionMode = 0u;
    r->hardReset = 0u;
    r->replayInputs = 0u;

    // Generation mismatch: never treat cross-generation error as normal drift.
    if (r->predictedGeneration != r->authoritativeGeneration) {
        r->shouldCorrect = 1u;
        r->correctionMode = 4u;  // hard reset
        r->hardReset = 1u;
        r->handled = 1u;
        return;
    }

    const float e = r->positionError;
    if (e <= r->smallDistance) {
        r->correctionMode = 0u;  // ignore
    } else if (e <= r->mediumDistance) {
        r->correctionMode = 1u;  // small / smooth
        r->shouldCorrect = 1u;
    } else if (e <= r->majorDistance) {
        r->correctionMode = 2u;  // medium
        r->shouldCorrect = 1u;
    } else {
        r->correctionMode = 3u;  // major / snap
        r->shouldCorrect = 1u;
    }

    r->handled = 1u;
}

} // namespace

const MimitaHotPackage::EventRegistrar s_reconcileRegistration{
    {GAME_EVENT_RECONCILE, 0, 0, onReconcile, "net.reconcile"}};

#endif
