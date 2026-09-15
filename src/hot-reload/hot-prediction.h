// 09 14 2026
/* purpose
* Shared hot prediction-link state. One generic component links an
* authoritative entity back to the prediction key that created it, so a client
* predicted provisional entity can be associated with its authoritative
* counterpart and retired. Type-agnostic: projectiles, predicted items,
* authoritative effects. Hot-only header: not a GameAPI context field.
* Does NOT link into the EXE.
*/
#pragma once

#include <cstdint>

#include "hot-reload/game-api.h"

static constexpr std::uint64_t HOT_PREDICTION_LINK_COMPONENT =
    gameHash("PredictionLink");

struct HotPredictionLinkV1 {
    std::uint64_t predictionKey;
    std::uint32_t status;   // reserved
    std::uint32_t reserved;
};
