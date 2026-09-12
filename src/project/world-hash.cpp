// 09 12 2026
/* purpose
* Implements deterministic world-state hashing.
* Does NOT mutate state.
*/
#include "project/world-hash.h"

#include "ecs/components.h"
#include "ecs/entity-registry.h"
#include "live-code/code-hash.h"

#include <algorithm>
#include <string>

namespace Project {

namespace {

void appendFloat(std::string& out, float value)
{
    // Fixed precision keeps the hash stable across formatting differences.
    char buffer[32];
    std::snprintf(buffer, sizeof(buffer), "%.6f", value);
    out += buffer;
}

void appendVec3(std::string& out, const float value[3])
{
    for (int i = 0; i < 3; ++i) {
        appendFloat(out, value[i]);
        out += ',';
    }
}

} // namespace

std::string WorldHash::hashEntities(const EntityRegistry& registry,
                                    const std::vector<EntityId>& ids)
{
    std::vector<EntityId> ordered = ids;
    if (ordered.empty())
        ordered = registry.all();
    std::sort(ordered.begin(), ordered.end());

    std::string canonical;
    for (EntityId id : ordered) {
        if (!registry.alive(id))
            continue;
        canonical += "E";
        canonical += std::to_string((unsigned long long)id);
        canonical += ';';

        if (const auto* transform = registry.tryGet<TransformComponent>(id)) {
            canonical += "T";
            appendVec3(canonical, reinterpret_cast<const float*>(&transform->position));
            appendFloat(canonical, transform->yaw);
            canonical += ';';
        }
        if (const auto* velocity = registry.tryGet<VelocityComponent>(id)) {
            canonical += "V";
            appendVec3(canonical, reinterpret_cast<const float*>(&velocity->linear));
            canonical += ';';
        }
        if (const auto* health = registry.tryGet<HealthComponent>(id)) {
            canonical += "H";
            canonical += std::to_string(health->current);
            canonical += ':';
            canonical += std::to_string(health->max);
            canonical += ':';
            canonical += health->dead ? '1' : '0';
            canonical += ';';
        }
        if (const auto* body = registry.tryGet<BodyComponent>(id)) {
            canonical += "B";
            appendFloat(canonical, body->radius);
            appendFloat(canonical, body->height);
            canonical += ';';
        }
        if (const auto* projectile = registry.tryGet<ProjectileComponent>(id)) {
            canonical += "P";
            canonical += std::to_string(projectile->fireSerial);
            canonical += ':';
            canonical += std::to_string(projectile->weaponDefNetworkId);
            canonical += ';';
        }
        if (const auto* owner = registry.tryGet<OwnerComponent>(id)) {
            canonical += "O";
            canonical += std::to_string((unsigned long long)owner->owner);
            canonical += ';';
        }
        canonical += '\n';
    }
    return LiveCodeHash::sha256Bytes(canonical.data(), canonical.size());
}

} // namespace Project
