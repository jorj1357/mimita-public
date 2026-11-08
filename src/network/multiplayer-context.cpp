#include "network/multiplayer-context.h"
#include "network/packets.h"
#include "analytics/analytics-manager.h"
#include "avatar/avatar.h"
#include "config/player-settings.h"
#include "combat/weapon-registry.h"
#include "effects/effect-part.h"
#include "debug/structured-log.h"
#include "gamemode/map-config.h"
#include "map/map-authored-geometry.h"
#include "world/world.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>

namespace MimitaNet {

bool gNetDamageDebug = false;
bool gNetHitDebug = false;

namespace {

bool isSameAddress(const sockaddr_in& a, const sockaddr_in& b)
{
    return a.sin_family == b.sin_family &&
        a.sin_port == b.sin_port &&
        a.sin_addr.s_addr == b.sin_addr.s_addr;
}

void copyName(char (&dst)[MAX_NAME_BYTES], const std::string& name)
{
    std::memset(dst, 0, sizeof(dst));
    std::strncpy(dst, name.c_str(), sizeof(dst) - 1);
}

} // namespace

static const char* disconnectReasonStr(MultiplayerContext& ctx)
{
    if (!ctx.active) return "inactive";
    if (ctx.connectFailed) return "connection-timeout";
    if (!ctx.connected) return "not-connected";
    if (ctx.sock == INVALID_SOCKET && !ctx.transport) return "no-transport";
    return "unknown";
}

bool mpApplyPendingMapConfig(MultiplayerContext& ctx, World& world,
                             const std::string& activeMapPath)
{
    if (!ctx.pendingMapConfigSnapshot.has_value())
        return false;

    const std::string activeMapId =
        std::filesystem::path(activeMapPath).stem().string();
    if (activeMapId.empty() ||
        activeMapId != ctx.pendingMapConfigSnapshot->mapId)
        return false;

    auto pending = std::move(*ctx.pendingMapConfigSnapshot);
    ctx.pendingMapConfigSnapshot.reset();
    if (MapConfigRegistry::instance().mapId() == pending.mapId &&
        ctx.appliedGeometrySignature == pending.geometrySignature) {
        StructuredLogger::instance().writeEvent(
            StructuredCategory::World, StructuredLevel::Verbose,
            "map.geometry.snapshot.skipped", pending.mapId,
            "authoritative config snapshot already applied", ctx.tick,
            {{"map", pending.mapId},
             {"geometry_revision", pending.geometryRevision},
             {"geometry_signature", pending.geometrySignature}});
        return false;
    }

    const auto start = std::chrono::steady_clock::now();
    if (!MapConfigRegistry::instance().loadFromJsonString(
            pending.mapId, pending.json, "server-snapshot:" + pending.mapId)) {
        StructuredLogger::instance().writeEvent(
            StructuredCategory::World, StructuredLevel::Errors,
            "map.geometry.snapshot.rejected", pending.mapId,
            "authoritative map config JSON was rejected; last client world kept",
            ctx.tick,
            {{"map", pending.mapId},
             {"config_revision", pending.configRevision},
             {"geometry_revision", pending.geometryRevision},
             {"geometry_signature", pending.geometrySignature},
             {"bytes", pending.json.size()}});
        return false;
    }

    AuthoredGeometryOptions options;
    options.rebuildCollision = true;
    options.staticFaceRemoval = true;
    if (!applyAuthoredMapGeometry(world, pending.mapId, options)) {
        StructuredLogger::instance().writeEvent(
            StructuredCategory::World, StructuredLevel::Errors,
            "map.geometry.snapshot.apply-failed", pending.mapId,
            "authoritative config loaded but authored geometry apply failed",
            ctx.tick,
            {{"map", pending.mapId},
             {"geometry_revision", pending.geometryRevision},
             {"geometry_signature", pending.geometrySignature}});
        return false;
    }

    ctx.appliedGeometryRevision = pending.geometryRevision;
    ctx.appliedGeometrySignature = pending.geometrySignature;
    ctx.geometryTransitionStartMs = nowMs();
    const double durationMs = std::chrono::duration<double, std::milli>(
        std::chrono::steady_clock::now() - start).count();
    StructuredLogger::instance().writeEvent(
        StructuredCategory::World, StructuredLevel::Important,
        "map.geometry.snapshot.applied", pending.mapId,
        "remote client applied authoritative authored map geometry", ctx.tick,
        {{"map", pending.mapId},
         {"config_revision", pending.configRevision},
         {"geometry_revision", pending.geometryRevision},
         {"geometry_signature", pending.geometrySignature},
         {"bytes", pending.json.size()},
         {"render_revision", world.renderRevision},
         {"duration_ms", durationMs}});
    return true;
}

} // namespace MimitaNet

