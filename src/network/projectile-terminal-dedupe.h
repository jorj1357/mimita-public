// 07 19 2026, 12 05
/* purpose
* Track authoritative projectile terminal events by projectile ID.
* Provide bounded deduplication and tombstones for client reconciliation.
* Keep late spawn/state packets from resurrecting terminated projectiles.
* Does NOT simulate projectiles, own packet transport, or play effects.
* Does NOT infer identity from owner, fire serial, weapon type, or spawn generation.
* Does NOT replace server projectile authority.
*/

#pragma once

#include <cstddef>
#include <cstdint>
#include <deque>
#include <unordered_set>

namespace MimitaNet {

class ProjectileTerminalDedupe
{
public:
    explicit ProjectileTerminalDedupe(size_t limit = 256) : mLimit(limit) {}

    bool has(uint32_t projectileId) const;
    bool record(uint32_t projectileId);
    void clear();
    size_t size() const { return mSeen.size(); }

private:
    size_t mLimit = 256;
    std::deque<uint32_t> mOrder;
    std::unordered_set<uint32_t> mSeen;
};

} // namespace MimitaNet
