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
    r->correctionMode = GAME_RECONCILE_MODE_NONE;
    r->hardReset = GAME_RECONCILE_HARD_RESET_NONE;
    r->replayInputs = 0u;
    r->reserved = GAME_RECONCILE_APPLY_NONE;

    // Generation mismatch: a hot-code sync/bootstrap condition, never a position
    // correction. Signal bootstrap (no position change) and return without
    // snapping so the client does not enter a repeated/fighting correction loop.
    // A zero/unknown generation on either side is not treated as a mismatch.
    if (r->predictedGeneration != 0 && r->authoritativeGeneration != 0 &&
        r->predictedGeneration != r->authoritativeGeneration) {
        r->hardReset = GAME_RECONCILE_HARD_RESET_BOOTSTRAP;
        r->handled = 1u;
        return;
    }

    const float e = r->positionError;
    // No correction at all for zero/tiny error (and for non-finite values).
    if (!(e > 0.25f) || e < r->smallDistance) {
        r->correctionMode = GAME_RECONCILE_MODE_NONE;
        r->reserved = GAME_RECONCILE_APPLY_NONE;
    } else if (e < r->majorDistance) {
        r->correctionMode = GAME_RECONCILE_MODE_SMOOTH;
        r->shouldCorrect = 1u;
        // Apply the server state once (cold rate-limits to avoid a float loop).
        r->reserved = GAME_RECONCILE_APPLY_SMOOTH_ONCE;
    } else {
        r->correctionMode = GAME_RECONCILE_MODE_SNAP;
        r->shouldCorrect = 1u;
        r->reserved = GAME_RECONCILE_APPLY_SNAP;
    }

    r->handled = 1u;
}

} // namespace

const MimitaHotPackage::EventRegistrar s_reconcileRegistration{
    {GAME_EVENT_RECONCILE, 0, 0, onReconcile, "net.reconcile"}};

#endif
