// 09 16 2026
/* purpose
* Hot remote-ragdoll presentation. The cold presenter calls the
* `ragdoll.presentation` capability per remote owner; this module buffers the
* snapshots (read via `ragdoll.snapshot`), interpolates them to a delayed render
* time, maps limbs with the `ragdoll.bind` template, and writes the typed remote
* actor via `actor.skeleton.write`, then reports handled so the cold presenter
* yields. Fully live-editable: edit and save, no EXE rebuild.
* Does NOT link into the EXE; only into the replaceable game DLL.
*/
#if defined(MIMITA_GAME_DLL)

#include "hot-reload/game-api.h"
#include "hot-reload/hot-package.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <deque>
#include <unordered_map>

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace {

using SnapshotFn = bool (MIMITA_GAME_CALL *)(void*, GameRagdollSnapshotV1*);
using BindFn = bool (MIMITA_GAME_CALL *)(void*, GameRagdollTemplateV1*);
using WriteFn = bool (MIMITA_GAME_CALL *)(void*, GameActorSkeletonWriteV1*);

constexpr double kTickHz = 60.0;
constexpr double kStaleSeconds = 0.5;
constexpr double kMaxExtrapolationSeconds = 0.1;
constexpr std::size_t kMaxBufferedFrames = 64;

struct Frame {
    std::uint32_t tick = 0;
    std::uint64_t receivedMs = 0;
    std::uint32_t limbCount = 0;
    glm::vec3 pos[GAME_MAX_RAGDOLL_SNAPSHOT_LIMBS];
    glm::quat rot[GAME_MAX_RAGDOLL_SNAPSHOT_LIMBS];
};
struct Owner {
    std::deque<Frame> buffer;
    std::uint32_t firstTick = 0;
    std::uint64_t firstReceivedMs = 0;
    bool clockInit = false;
};

std::unordered_map<std::uint32_t, Owner> g_owners;
GameRagdollTemplateV1 g_template{};

std::uint64_t nowMs()
{
    return (std::uint64_t)std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
}

void pushFrame(Owner& owner, const GameRagdollSnapshotV1& snap)
{
    Frame f;
    f.tick = (std::uint32_t)snap.tick;
    f.receivedMs = nowMs();
    f.limbCount = std::min(snap.limbCount,
                           (std::uint32_t)GAME_MAX_RAGDOLL_SNAPSHOT_LIMBS);
    for (std::uint32_t i = 0; i < f.limbCount; ++i) {
        f.pos[i] = glm::vec3(snap.limbs[i].position[0], snap.limbs[i].position[1],
                             snap.limbs[i].position[2]);
        f.rot[i] = glm::quat(snap.limbs[i].rotation[0], snap.limbs[i].rotation[1],
                             snap.limbs[i].rotation[2], snap.limbs[i].rotation[3]);
    }
    if (owner.buffer.empty()) {
        owner.firstReceivedMs = f.receivedMs;
        owner.firstTick = f.tick;
    }
    if (owner.buffer.empty() || f.tick >= owner.buffer.back().tick) {
        if (!owner.buffer.empty() && f.tick == owner.buffer.back().tick)
            owner.buffer.back() = f;
        else
            owner.buffer.push_back(f);
    } else {
        auto it = owner.buffer.begin();
        while (it != owner.buffer.end() && it->tick < f.tick) ++it;
        if (it != owner.buffer.end() && it->tick == f.tick)
            *it = f;
        else
            owner.buffer.insert(it, f);
    }
    while (owner.buffer.size() > kMaxBufferedFrames)
        owner.buffer.pop_front();
}

// Interpolate the buffer to the delayed render tick into outPos/outRot.
bool interpolate(Owner& owner, double delaySeconds,
                 glm::vec3* outPos, glm::quat* outRot,
                 const GameRagdollTemplateV1& tpl)
{
    if (owner.buffer.size() < 2)
        return false;
    const std::uint64_t now = nowMs();
    if (now - owner.buffer.back().receivedMs >
        (std::uint64_t)(kStaleSeconds * 1000.0))
        return false;
    if (!owner.clockInit) {
        owner.firstTick = owner.buffer.front().tick;
        owner.firstReceivedMs = owner.buffer.front().receivedMs;
        owner.clockInit = true;
    }
    const double renderTick = (double)owner.firstTick +
        ((double)now - (double)owner.firstReceivedMs) / 1000.0 * kTickHz -
        delaySeconds * kTickHz;

    const std::size_t partCount = tpl.partCount;
    const Frame& oldest = owner.buffer.front();
    const Frame& newest = owner.buffer.back();
    const std::size_t n = std::min({partCount, (std::size_t)oldest.limbCount,
                                    (std::size_t)newest.limbCount});

    if ((double)renderTick <= (double)oldest.tick) {
        for (std::size_t i = 0; i < n; ++i) {
            outPos[i] = oldest.pos[i];
            outRot[i] = oldest.rot[i];
        }
        return n > 0;
    }
    if ((double)renderTick >= (double)newest.tick) {
        const Frame& a = owner.buffer[owner.buffer.size() - 2];
        const double tickDelta = (double)newest.tick - (double)a.tick;
        double dt = (renderTick - (double)newest.tick) / kTickHz;
        dt = std::clamp(dt, 0.0, kMaxExtrapolationSeconds);
        const std::size_t m = std::min({partCount, (std::size_t)a.limbCount,
                                        (std::size_t)newest.limbCount});
        for (std::size_t i = 0; i < m; ++i) {
            glm::vec3 velocity(0.0f);
            if (tickDelta > 0.0)
                velocity = (newest.pos[i] - a.pos[i]) * (float)(kTickHz / tickDelta);
            outPos[i] = newest.pos[i] + velocity * (float)dt;
            outRot[i] = newest.rot[i];
        }
        return m > 0;
    }
    std::size_t index = 0;
    for (std::size_t i = 0; i + 1 < owner.buffer.size(); ++i) {
        if ((double)owner.buffer[i].tick <= renderTick &&
            renderTick <= (double)owner.buffer[i + 1].tick) {
            index = i;
            break;
        }
    }
    const Frame& a = owner.buffer[index];
    const Frame& b = owner.buffer[index + 1];
    const double span = (double)b.tick - (double)a.tick;
    const float alpha = span > 0.0
        ? (float)std::clamp((renderTick - (double)a.tick) / span, 0.0, 1.0)
        : 0.0f;
    const std::size_t m = std::min({partCount, (std::size_t)a.limbCount,
                                    (std::size_t)b.limbCount});
    for (std::size_t i = 0; i < m; ++i) {
        outPos[i] = glm::mix(a.pos[i], b.pos[i], alpha);
        outRot[i] = glm::normalize(glm::slerp(a.rot[i], b.rot[i], alpha));
    }
    return m > 0;
}

void MIMITA_GAME_CALL ragdollPresentProvider(void* host, GameRagdollPresentV1* req)
{
    if (!req)
        return;
    req->handled = 0;
    GameplayContextV1* ctx = static_cast<GameplayContextV1*>(host);
    if (!ctx || !ctx->resolveCapability || req->ownerActorId == 0)
        return;
    auto snapFn = reinterpret_cast<SnapshotFn>(
        ctx->resolveCapability(ctx->host, GAME_CAP_RAGDOLL_SNAPSHOT));
    auto bindFn = reinterpret_cast<BindFn>(
        ctx->resolveCapability(ctx->host, GAME_CAP_RAGDOLL_BIND));
    auto writeFn = reinterpret_cast<WriteFn>(
        ctx->resolveCapability(ctx->host, GAME_CAP_ACTOR_SKELETON_WRITE));
    if (!snapFn || !bindFn || !writeFn)
        return;

    // Fetch the body template once (same model for remote owners).
    if (!g_template.valid) {
        GameRagdollTemplateV1 t{};
        t.actorEntity = 0;
        if (!bindFn(ctx->host, &t) || !t.valid)
            return;
        g_template = t;
    }

    GameRagdollSnapshotV1 snap{};
    snap.op = 0;  // read
    snap.ownerActorId = req->ownerActorId;
    if (!snapFn(ctx->host, &snap))
        return;

    Owner& owner = g_owners[req->ownerActorId];
    pushFrame(owner, snap);

    glm::vec3 pos[GAME_MAX_RAGDOLL_SNAPSHOT_LIMBS];
    glm::quat rot[GAME_MAX_RAGDOLL_SNAPSHOT_LIMBS];
    if (!interpolate(owner, req->delaySeconds, pos, rot, g_template))
        return;

    const int torsoIndex = g_template.torsoIndex;
    if (torsoIndex < 0 || (std::uint32_t)torsoIndex >= g_template.partCount)
        return;

    const glm::mat4 torsoWorld =
        glm::translate(glm::mat4(1.0f), pos[torsoIndex]) *
        glm::mat4_cast(rot[torsoIndex]);
    const glm::vec3 root = glm::vec3(torsoWorld * glm::vec4(
        g_template.rootOffsetLocal[0], g_template.rootOffsetLocal[1],
        g_template.rootOffsetLocal[2], 1.0f));

    GameActorSkeletonWriteV1 w{};
    w.ownerActorId = req->ownerActorId;
    w.isNpc = req->isNpc;
    w.actorEntity = req->actorEntity;
    w.rootPosition[0] = root.x; w.rootPosition[1] = root.y; w.rootPosition[2] = root.z;
    w.rootRotation[0] = rot[torsoIndex].w; w.rootRotation[1] = rot[torsoIndex].x;
    w.rootRotation[2] = rot[torsoIndex].y; w.rootRotation[3] = rot[torsoIndex].z;
    w.rootRotationActive = 1u;
    const glm::mat4 rootWorld = glm::translate(glm::mat4(1.0f), root) *
                                glm::mat4_cast(rot[torsoIndex]);
    const std::uint32_t anc = std::min(g_template.ancestorNodeCount, 8u);
    w.ancestorCount = anc;
    for (std::uint32_t i = 0; i < anc; ++i)
        w.ancestorNodes[i] = g_template.ancestorNodes[i];

    for (std::uint32_t i = 0; i < g_template.partCount &&
                             w.nodeCount < GAME_MAX_SKELETON_NODES; ++i) {
        const GameRagdollPartV1& part = g_template.parts[i];
        if (part.nodeIndex < 0)
            continue;
        glm::mat4 meshLocal;
        for (int c = 0; c < 4; ++c)
            for (int r = 0; r < 4; ++r)
                meshLocal[c][r] = part.meshLocal[c * 4 + r];
        const glm::mat4 childWorld =
            glm::translate(glm::mat4(1.0f), pos[i]) * glm::mat4_cast(rot[i]) * meshLocal;
        glm::mat4 parentWorld = rootWorld;
        if (part.skeletonParentPart >= 0 &&
            (std::uint32_t)part.skeletonParentPart < g_template.partCount) {
            const std::uint32_t pp = (std::uint32_t)part.skeletonParentPart;
            glm::mat4 parentMeshLocal;
            for (int c = 0; c < 4; ++c)
                for (int r = 0; r < 4; ++r)
                    parentMeshLocal[c][r] = g_template.parts[pp].meshLocal[c * 4 + r];
            parentWorld = glm::translate(glm::mat4(1.0f), pos[pp]) *
                          glm::mat4_cast(rot[pp]) * parentMeshLocal;
        }
        const glm::mat4 local = glm::inverse(parentWorld) * childWorld;
        GameActorSkeletonNodeV1& node = w.nodes[w.nodeCount++];
        node.nodeIndex = part.nodeIndex;
        for (int c = 0; c < 4; ++c)
            for (int r = 0; r < 4; ++r)
                node.local[c * 4 + r] = local[c][r];
    }

    if (w.nodeCount == 0)
        return;
    if (writeFn(ctx->host, &w) && w.applied)
        req->handled = 1;
}

const MimitaHotPackage::CapabilityRegistrar s_ragdollPresentProvider{
    {GAME_CAP_RAGDOLL_PRESENT, gameHash("sig.ragdoll.presentation.v1"), 0,
     reinterpret_cast<void*>(&ragdollPresentProvider), "ragdoll.presentation"}};

} // namespace

#endif
