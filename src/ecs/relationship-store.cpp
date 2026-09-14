// 09 14 2026
/* purpose
* Implements generic relationship storage, per-type replication policy, and
* generic edge change markers.
* Does NOT own gameplay policy or rendering.
*/
#include "ecs/relationship-store.h"

#include <algorithm>

#include "hot-reload/game-api.h"

namespace MimitaRuntime {

RelationshipStore& RelationshipStore::instance()
{
    static RelationshipStore store;
    return store;
}

void RelationshipStore::clear()
{
    edges_.clear();
    policies_.clear();
    revision_ = 0;
}

bool RelationshipStore::add(std::uint64_t typeId, EntityId from, EntityId to,
                            std::uint64_t value)
{
    if (typeId == 0 || from == kInvalidEntityId || to == kInvalidEntityId)
        return false;
    // First use of a type makes it replicable by default; explicit policy wins.
    if (policies_.find(typeId) == policies_.end())
        policies_[typeId] = GAME_NET_ALL;
    const std::uint32_t changeVersion = ++revision_;
    auto& list = edges_[typeId];
    for (Edge& edge : list) {
        if (edge.from == from && edge.to == to) {
            edge.value = value;
            edge.changeVersion = changeVersion;
            return true;
        }
    }
    list.push_back(Edge{from, to, value, changeVersion});
    return true;
}

bool RelationshipStore::addVersioned(std::uint64_t typeId, EntityId from,
                                     EntityId to, std::uint64_t value,
                                     std::uint32_t changeVersion)
{
    if (typeId == 0 || from == kInvalidEntityId || to == kInvalidEntityId)
        return false;
    if (policies_.find(typeId) == policies_.end())
        policies_[typeId] = GAME_NET_ALL;
    auto& list = edges_[typeId];
    for (Edge& edge : list) {
        if (edge.from == from && edge.to == to) {
            if (changeVersion != 0 && changeVersion <= edge.changeVersion)
                return false;  // stale replicated record
            edge.value = value;
            edge.changeVersion = changeVersion != 0 ? changeVersion : ++revision_;
            return true;
        }
    }
    list.push_back(Edge{from, to, value,
                        changeVersion != 0 ? changeVersion : ++revision_});
    return true;
}

bool RelationshipStore::remove(std::uint64_t typeId, EntityId from, EntityId to)
{
    auto it = edges_.find(typeId);
    if (it == edges_.end())
        return false;
    auto& list = it->second;
    const auto before = list.size();
    list.erase(std::remove_if(list.begin(), list.end(),
                              [&](const Edge& edge) {
                                  return edge.from == from && edge.to == to;
                              }),
               list.end());
    if (list.size() != before)
        ++revision_;
    return list.size() != before;
}

bool RelationshipStore::has(std::uint64_t typeId, EntityId from, EntityId to) const
{
    auto it = edges_.find(typeId);
    if (it == edges_.end())
        return false;
    for (const Edge& edge : it->second)
        if (edge.from == from && edge.to == to)
            return true;
    return false;
}

std::size_t RelationshipStore::query(std::uint64_t typeId, EntityId from,
                                     EntityId* outTo, std::uint64_t* outValue,
                                     std::size_t maxOut) const
{
    if (!outTo || maxOut == 0)
        return 0;
    auto it = edges_.find(typeId);
    if (it == edges_.end())
        return 0;
    std::vector<const Edge*> matches;
    for (const Edge& edge : it->second)
        if (edge.from == from)
            matches.push_back(&edge);
    std::sort(matches.begin(), matches.end(),
              [](const Edge* a, const Edge* b) { return a->to < b->to; });
    const std::size_t count = std::min(maxOut, matches.size());
    for (std::size_t i = 0; i < count; ++i) {
        outTo[i] = matches[i]->to;
        if (outValue)
            outValue[i] = matches[i]->value;
    }
    return count;
}

std::size_t RelationshipStore::queryReverse(std::uint64_t typeId, EntityId to,
                                            EntityId* outFrom, std::size_t maxOut) const
{
    if (!outFrom || maxOut == 0)
        return 0;
    auto it = edges_.find(typeId);
    if (it == edges_.end())
        return 0;
    std::vector<EntityId> matches;
    for (const Edge& edge : it->second)
        if (edge.to == to)
            matches.push_back(edge.from);
    std::sort(matches.begin(), matches.end());
    const std::size_t count = std::min(maxOut, matches.size());
    for (std::size_t i = 0; i < count; ++i)
        outFrom[i] = matches[i];
    return count;
}

std::size_t RelationshipStore::edgesOfType(std::uint64_t typeId,
                                           RelationshipEdge* out,
                                           std::size_t maxOut) const
{
    if (!out || maxOut == 0)
        return 0;
    auto it = edges_.find(typeId);
    if (it == edges_.end())
        return 0;
    std::vector<const Edge*> matches;
    matches.reserve(it->second.size());
    for (const Edge& edge : it->second)
        matches.push_back(&edge);
    std::sort(matches.begin(), matches.end(),
              [](const Edge* a, const Edge* b) {
                  if (a->from != b->from)
                      return a->from < b->from;
                  return a->to < b->to;
              });
    const std::size_t count = std::min(maxOut, matches.size());
    for (std::size_t i = 0; i < count; ++i) {
        out[i].from = matches[i]->from;
        out[i].to = matches[i]->to;
        out[i].value = matches[i]->value;
        out[i].changeVersion = matches[i]->changeVersion;
    }
    return count;
}

std::uint32_t RelationshipStore::changeVersionOf(std::uint64_t typeId,
                                                 EntityId from, EntityId to) const
{
    auto it = edges_.find(typeId);
    if (it == edges_.end())
        return 0;
    for (const Edge& edge : it->second)
        if (edge.from == from && edge.to == to)
            return edge.changeVersion;
    return 0;
}

void RelationshipStore::setNetworkPolicy(std::uint64_t typeId, std::uint32_t policy)
{
    if (typeId != 0)
        policies_[typeId] = policy;
}

std::uint32_t RelationshipStore::networkPolicy(std::uint64_t typeId) const
{
    auto it = policies_.find(typeId);
    return it == policies_.end() ? GAME_NET_NONE : it->second;
}

std::vector<std::uint64_t> RelationshipStore::typeIds() const
{
    std::vector<std::uint64_t> ids;
    ids.reserve(edges_.size());
    for (const auto& entry : edges_)
        ids.push_back(entry.first);
    std::sort(ids.begin(), ids.end());
    return ids;
}

void RelationshipStore::eraseEntity(EntityId entity)
{
    for (auto& entry : edges_) {
        auto& list = entry.second;
        list.erase(std::remove_if(list.begin(), list.end(),
                                  [&](const Edge& edge) {
                                      return edge.from == entity || edge.to == entity;
                                  }),
                   list.end());
    }
}

std::size_t RelationshipStore::edgeCount() const
{
    std::size_t count = 0;
    for (const auto& entry : edges_)
        count += entry.second.size();
    return count;
}

} // namespace MimitaRuntime
