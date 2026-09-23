// 09 23 2026
/* purpose
* Hot policy for choosing one avatar for one actor life.
* The kernel supplies validated candidate names; this module owns the choice.
* Does NOT read files, access AvatarSystem, or mutate EXE/entity storage.
*/
#if defined(MIMITA_GAME_DLL)

#include "hot-reload/game-api.h"
#include "hot-reload/hot-package.h"

#include <cstring>

namespace {

std::uint64_t mix(std::uint64_t value)
{
    value ^= value >> 30;
    value *= 0xbf58476d1ce4e5b9ULL;
    value ^= value >> 27;
    value *= 0x94d049bb133111ebULL;
    return value ^ (value >> 31);
}

void MIMITA_GAME_CALL onActorAvatarPolicy(void* /*host*/, const GameEventV1* event)
{
    auto* p = event ? static_cast<ActorAvatarPolicyV1*>(event->payload) : nullptr;
    if (!p || p->candidateCount == 0)
        return;

    const std::uint32_t count = p->candidateCount > ACTOR_AVATAR_POLICY_MAX_CANDIDATES
        ? ACTOR_AVATAR_POLICY_MAX_CANDIDATES : p->candidateCount;
    const std::uint32_t index = static_cast<std::uint32_t>(
        mix(p->entityId ^ (static_cast<std::uint64_t>(p->lifeGeneration) << 32)) % count);
    p->selectedIndex = index;
    std::memcpy(p->selectedAvatar, p->candidates[index],
                sizeof(p->selectedAvatar));
    p->selectedAvatar[sizeof(p->selectedAvatar) - 1] = '\0';
    p->handled = 1u;
}

} // namespace

const MimitaHotPackage::EventRegistrar s_actorAvatarPolicyRegistration{
    {GAME_EVENT_ACTOR_AVATAR_POLICY, 0, 0, onActorAvatarPolicy,
     "actor.avatar-policy"}};

#endif
