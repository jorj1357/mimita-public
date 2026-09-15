// 09 15 2026
/* purpose
* net.relevance: the hot snapshot/relevance policy. Given a viewer position and
* generic candidate entities (Transform + optional ReplicationPolicy), it decides
* which candidates replicate, a priority tier, and the low-tier cadence. Always-
* relevant state (match/objective) is included regardless of distance; near
* actors replicate every tick; far entities use a lower cadence. It works for any
* entity with generic Transform state (no player/NPC/entity-type switch).
* Transport/framing/socket stay cold and mechanical.
* Does NOT link into the EXE; only into the replaceable game DLL.
*/
#if defined(MIMITA_GAME_DLL)

#include "hot-reload/game-api.h"
#include "hot-reload/hot-package.h"
#include "network/relevance.h"

namespace {

using MimitaNet::GameRelevanceQueryV1;

// Hot-editable policy values.
constexpr float kNearRadius = 30.0f;
constexpr std::uint32_t kFarCadence = 6;

void MIMITA_GAME_CALL onRelevance(void* /*host*/, const GameEventV1* event)
{
    auto* q = event ? static_cast<GameRelevanceQueryV1*>(event->payload) : nullptr;
    if (!q)
        return;
    const float near2 = kNearRadius * kNearRadius;
    for (std::uint32_t i = 0; i < q->candidateCount &&
                             i < MimitaNet::GAME_MAX_RELEVANCE_CANDIDATES; ++i) {
        const auto& c = q->candidates[i];
        const bool always = (c.flags & 1u) != 0;
        const float dx = c.position[0] - q->viewerPosition[0];
        const float dy = c.position[1] - q->viewerPosition[1];
        const float dz = c.position[2] - q->viewerPosition[2];
        const bool near = (dx * dx + dy * dy + dz * dz) <= near2;
        q->outInclude[i] = 1u;
        q->outTier[i] = (always || near) ? 0u : 1u;
    }
    q->outLowTierEveryNTicks = kFarCadence;
    q->handled = 1u;
}

} // namespace

const MimitaHotPackage::SchemaRegistrar s_replicationPolicySchema{
    {MimitaNet::GAME_COMPONENT_REPLICATION_POLICY,
     gameHash("ReplicationPolicy.v1"), 4, 4, GAME_COPY_AUTHORING,
     GAME_NET_SERVER_ONLY, "ReplicationPolicy", 1, 0}};
const MimitaHotPackage::EventRegistrar s_relevanceRegistration{
    {MimitaNet::GAME_EVENT_NET_RELEVANCE, 0, 0, onRelevance, "net.relevance"}};

#endif
