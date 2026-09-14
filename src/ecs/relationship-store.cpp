// 09 14 2026
/* purpose
* Implements generic relationship storage.
* Does NOT own gameplay policy, rendering, or replication.
*/
#include "ecs/relationship-store.h"

#include <algorithm>

namespace MimitaRuntime {

RelationshipStore& RelationshipStore::instance()
{
    static RelationshipStore store;
    return store;
}

void RelationshipStore::clear()
{
    edges_.clear();
}

bool RelationshipStore::add(std::uint64_t typeId, EntityId from, EntityId to,
                            std::uint64_t value)
{
    if (typeId == 0 || from == kInvalidEntityId || to == kInvalidEntityId)
        return false;
    auto& list = edges_[typeId];
    for (Edge& edge : list) {
        if (edge.from == from && edge.to == to) {
            edge.value = value;
            return true;
        }
    }
    list.push_back(Edge{from, to, value});
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
    return list.size() != before;
}

bool RelationshipStore::has(std::uint64_t typeId, EntityId from, EntityId to) const
{
    auto it = edges_.find(typeId);
    if (it == edges_.end())
        return false;
    for (const Edge& edge : it->second) {
        if (edge.from == from && edge.to == to)
            return true;
    }
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
    for (const Edge& edge : it->second) {
        if (edge.from == from)
            matches.push_back(&edge);
    }
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
    for (const Edge& edge : it->second) {
        if (edge.to == to)
            matches.push_back(edge.from);
    }
    std::sort(matches.begin(), matches.end());

    const std::size_t count = std::min(maxOut, matches.size());
    for (std::size_t i = 0; i < count; ++i)
        outFrom[i] = matches[i];
    return count;
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
