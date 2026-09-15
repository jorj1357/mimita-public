// 09 14 2026
/* purpose
* Implements the generic predicted -> authoritative entity association.
* Does NOT own prediction/interpolation math, networking, or presentation.
*/
#include "ecs/prediction-registry.h"

#include "ecs/entity-registry.h"

namespace Ecs {
namespace {

bool alive(EntityId entity)
{
    return entity != kInvalidEntityId && EntityRegistry::instance().alive(entity);
}

void retire(EntityId entity)
{
    if (alive(entity))
        EntityRegistry::instance().destroy(entity);
}

} // namespace

PredictionRegistry& PredictionRegistry::instance()
{
    static PredictionRegistry registry;
    return registry;
}

void PredictionRegistry::clear()
{
    associations_.clear();
}

EntityId PredictionRegistry::registerProvisional(std::uint64_t key,
                                                 EntityId provisional,
                                                 std::uint64_t tick)
{
    if (key == 0)
        return provisional;
    PredictionAssociation& assoc = associations_[key];
    assoc.key = key;
    assoc.tick = tick;

    // Authority already present: the provisional is redundant.
    if (alive(assoc.authoritative)) {
        if (provisional != assoc.authoritative)
            retire(provisional);
        assoc.status = PREDICTION_ASSOCIATED;
        return assoc.authoritative;
    }

    if (alive(assoc.provisional) && assoc.provisional != provisional)
        retire(assoc.provisional);
    assoc.provisional = provisional;
    assoc.status = PREDICTION_PENDING;
    return provisional;
}

EntityId PredictionRegistry::associate(std::uint64_t key, EntityId authoritative,
                                       std::uint64_t tick)
{
    if (key == 0 || authoritative == kInvalidEntityId)
        return authoritative;
    PredictionAssociation& assoc = associations_[key];
    assoc.key = key;
    assoc.tick = tick;

    // Duplicate/late association: keep the newest authoritative, retire any
    // provisional, and never resurrect a retired provisional.
    if (assoc.authoritative != kInvalidEntityId &&
        assoc.authoritative != authoritative)
        retire(assoc.authoritative);
    assoc.authoritative = authoritative;
    if (alive(assoc.provisional)) {
        retire(assoc.provisional);
        assoc.provisional = kInvalidEntityId;
    }
    assoc.status = PREDICTION_ASSOCIATED;
    return authoritative;
}

EntityId PredictionRegistry::canonical(std::uint64_t key) const
{
    auto it = associations_.find(key);
    if (it == associations_.end())
        return kInvalidEntityId;
    if (alive(it->second.authoritative))
        return it->second.authoritative;
    if (alive(it->second.provisional))
        return it->second.provisional;
    return kInvalidEntityId;
}

EntityId PredictionRegistry::provisionalOf(std::uint64_t key) const
{
    auto it = associations_.find(key);
    return it == associations_.end() ? kInvalidEntityId : it->second.provisional;
}

EntityId PredictionRegistry::authoritativeOf(std::uint64_t key) const
{
    auto it = associations_.find(key);
    return it == associations_.end() ? kInvalidEntityId : it->second.authoritative;
}

PredictionStatus PredictionRegistry::statusOf(std::uint64_t key) const
{
    auto it = associations_.find(key);
    return it == associations_.end() ? PREDICTION_PENDING : (PredictionStatus)it->second.status;
}

void PredictionRegistry::onEntityDestroyed(EntityId entity)
{
    for (auto it = associations_.begin(); it != associations_.end();) {
        PredictionAssociation& assoc = it->second;
        if (assoc.provisional == entity)
            assoc.provisional = kInvalidEntityId;
        if (assoc.authoritative == entity)
            assoc.authoritative = kInvalidEntityId;

        const bool noProvisional = assoc.provisional == kInvalidEntityId;
        const bool noAuthority = assoc.authoritative == kInvalidEntityId;
        if (noProvisional && noAuthority) {
            it = associations_.erase(it);
            continue;
        }
        if (noAuthority)
            assoc.status = PREDICTION_PENDING;   // provisional continues
        else if (noProvisional)
            assoc.status = PREDICTION_ASSOCIATED; // authority continues
        ++it;
    }
}

void PredictionRegistry::pruneOlderThan(std::uint64_t tick)
{
    for (auto it = associations_.begin(); it != associations_.end();) {
        PredictionAssociation& assoc = it->second;
        const bool pending = assoc.authoritative == kInvalidEntityId;
        if (pending && assoc.tick < tick) {
            retire(assoc.provisional);
            it = associations_.erase(it);
            continue;
        }
        ++it;
    }
}

} // namespace Ecs
