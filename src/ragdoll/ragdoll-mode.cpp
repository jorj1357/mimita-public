#include "ragdoll/ragdoll-mode.h"
#include "ragdoll/ragdoll-mode-config.h"
#include "ragdoll/ragdoll-body.h"
#include "ragdoll/ragdoll-entities.h"
#include "hot-reload/game-api.h"
#include "hot-reload/generic-runtime.h"
#include "live-code/live-behavior.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <unordered_map>
#include <utility>

#include <glad/glad.h>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/quaternion.hpp>

#include "config.h"
#include "config/ragdoll-death-config.h"
#include "camera.h"
#include "effects/effect-part.h"
#include "effects/hit-effects.h"
#include "entities/player.h"
#include "render/render-player.h"
#include "input/input-state.h"
#include "physics/physics-types.h"
#include "physics/movement/physics-collision.h"
#include "physics/movement/physics-collision-shared.h"
#include "physics/ray-utils.h"
#include "sim/domain-scheduler.h"
#include "telemetry/telemetry.h"
#include "renderer/renderer.h"
#include "world/texture-store.h"
#include "world/world.h"
#include "debug/debug-visuals.h"
#include "debug/debug-log.h"
#include "debug/gl-debug.h"
#include "debug/structured-log.h"

extern Renderer* gRenderer;
extern TextureStore gTextures;

// Hot ragdoll solver seam. Fills the POD snapshot and calls the hot
// `ragdoll.solve` provider; returns true when the hot side owned the solve and
// its results were applied. Returns false (cold solver runs) when no provider is
// loaded or the provider declines (handled == 0) — the temporary fallback.
static bool hotRagdollSolve(RagdollBody& b,
                            const Ragdoll::SolveParams& params, float dt)
{
    auto fn = reinterpret_cast<GameRagdollSolveFn>(
        MimitaRuntime::GenericRuntime::instance().capability(
            GAME_CAP_RAGDOLL_SOLVE));
    if (!fn)
        return false;
    GameRagdollSolveV1 s{};
    s.structSize = sizeof(GameRagdollSolveV1);
    const std::uint32_t n = static_cast<std::uint32_t>(
        std::min<std::size_t>(b.parts.size(), GAME_MAX_RAGDOLL_LIMBS));
    s.limbCount = n;
    s.dt = dt;
    s.gravityScale = params.gravityScale;
    s.stiffness = params.stiffness;
    s.damping = params.damping;
    s.iterations = params.iterations > 0 ? (std::uint32_t)params.iterations : 1u;
    for (std::uint32_t i = 0; i < n; ++i) {
        const RagdollModePart& p = b.parts[i];
        GameRagdollLimbStateV1& d = s.limbs[i];
        d.position[0] = p.body.position.x; d.position[1] = p.body.position.y; d.position[2] = p.body.position.z;
        d.orientation[0] = p.body.orientation.w; d.orientation[1] = p.body.orientation.x;
        d.orientation[2] = p.body.orientation.y; d.orientation[3] = p.body.orientation.z;
        d.linearVelocity[0] = p.body.linearVelocity.x; d.linearVelocity[1] = p.body.linearVelocity.y; d.linearVelocity[2] = p.body.linearVelocity.z;
        d.angularVelocity[0] = p.body.angularVelocity.x; d.angularVelocity[1] = p.body.angularVelocity.y; d.angularVelocity[2] = p.body.angularVelocity.z;
        GameRagdollLimbStaticV1& st = s.statics[i];
        st.parentIndex = p.parentIndex < 0 ? 0xffffffffu : (std::uint32_t)p.parentIndex;
        st.hasRotationLimits = p.hasRotationLimits ? 1u : 0u;
        st.inverseMass = p.body.invMass;
        st.radius = p.body.capsuleRadius;
        st.halfHeight = p.body.capsuleHalfHeight;
        st.parentLocalAnchor[0] = p.parentLocalAnchor.x; st.parentLocalAnchor[1] = p.parentLocalAnchor.y; st.parentLocalAnchor[2] = p.parentLocalAnchor.z;
        st.childLocalAnchor[0] = p.childLocalAnchor.x; st.childLocalAnchor[1] = p.childLocalAnchor.y; st.childLocalAnchor[2] = p.childLocalAnchor.z;
        st.restLength = p.restLength;
        st.maxStretch = p.maxStretch;
        st.bindRotation[0] = p.bindRelativeRotation.w; st.bindRotation[1] = p.bindRelativeRotation.x;
        st.bindRotation[2] = p.bindRelativeRotation.y; st.bindRotation[3] = p.bindRelativeRotation.z;
        st.rotMinDeg[0] = p.rotMinDeg.x; st.rotMinDeg[1] = p.rotMinDeg.y; st.rotMinDeg[2] = p.rotMinDeg.z;
        st.rotMaxDeg[0] = p.rotMaxDeg.x; st.rotMaxDeg[1] = p.rotMaxDeg.y; st.rotMaxDeg[2] = p.rotMaxDeg.z;
    }
    auto fillGrab = [](GameRagdollGrabV1& g, const RagdollGrabState& src) {
        g.active = src.active ? 1u : 0u;
        g.limbIndex = src.partIndex < 0 ? 0xffffffffu : (std::uint32_t)src.partIndex;
        g.targetLimb = src.targetPart;
        g.grabPoint[0] = src.grabPoint.x; g.grabPoint[1] = src.grabPoint.y; g.grabPoint[2] = src.grabPoint.z;
        g.grabNormal[0] = src.grabNormal.x; g.grabNormal[1] = src.grabNormal.y; g.grabNormal[2] = src.grabNormal.z;
        g.handLocalAnchor[0] = src.handLocalAnchor.x; g.handLocalAnchor[1] = src.handLocalAnchor.y; g.handLocalAnchor[2] = src.handLocalAnchor.z;
        g.targetLocalAnchor[0] = src.targetLocalAnchor.x; g.targetLocalAnchor[1] = src.targetLocalAnchor.y; g.targetLocalAnchor[2] = src.targetLocalAnchor.z;
        g.strength = src.strength;
    };
    fillGrab(s.grabLeft, b.leftGrab);
    fillGrab(s.grabRight, b.rightGrab);
    fn(LiveBehavior::hostContext(0), &s);
    if (!s.handled)
        return false;
    for (std::uint32_t i = 0; i < n; ++i) {
        RagdollModePart& p = b.parts[i];
        const GameRagdollLimbStateV1& d = s.limbs[i];
        p.body.position = glm::vec3(d.position[0], d.position[1], d.position[2]);
        p.body.orientation = glm::quat(d.orientation[0], d.orientation[1], d.orientation[2], d.orientation[3]);
        p.body.linearVelocity = glm::vec3(d.linearVelocity[0], d.linearVelocity[1], d.linearVelocity[2]);
        p.body.angularVelocity = glm::vec3(d.angularVelocity[0], d.angularVelocity[1], d.angularVelocity[2]);
    }
    return true;
}

// Hot alive-ragdoll aim seam. Fills the POD snapshot (head/torso indices, camera
// look basis, per-limb orientation/angular velocity/aim offset) and calls the
// hot `ragdoll.aim` provider; returns true when the hot side owned the aim and
// its angular velocities were applied. Returns false (cold applyControls runs)
// when no provider is loaded or the provider declines (handled == 0).
static bool hotRagdollAim(RagdollBody& b, const Camera& camera, float dt)
{
    auto fn = reinterpret_cast<GameRagdollAimFn>(
        MimitaRuntime::GenericRuntime::instance().capability(
            GAME_CAP_RAGDOLL_AIM));
    if (!fn)
        return false;
    const auto& cfg = RagdollModeConfig::instance().data();
    GameRagdollAimV1 a{};
    a.structSize = sizeof(GameRagdollAimV1);
    a.headIndex = b.headIndex < 0 ? 0xffffffffu : (std::uint32_t)b.headIndex;
    a.torsoIndex = b.torsoIndex < 0 ? 0xffffffffu : (std::uint32_t)b.torsoIndex;
    a.cameraFront[0] = camera.front.x; a.cameraFront[1] = camera.front.y; a.cameraFront[2] = camera.front.z;
    a.cameraUp[0] = camera.up.x; a.cameraUp[1] = camera.up.y; a.cameraUp[2] = camera.up.z;
    a.headStrength = cfg.headRotationStrength;
    a.headMaxSpeed = cfg.headRotationSpeed;
    a.torsoStrength = cfg.torsoLookSpring;
    a.torsoMaxSpeed = cfg.torsoMaxAngularStep;
    a.lookDamping = cfg.lookDamping;
    a.dt = dt;
    const std::uint32_t n = static_cast<std::uint32_t>(
        std::min<std::size_t>(b.parts.size(), GAME_MAX_RAGDOLL_LIMBS));
    a.limbCount = n;
    for (std::uint32_t i = 0; i < n; ++i) {
        const RagdollModePart& p = b.parts[i];
        GameRagdollAimLimbV1& d = a.limbs[i];
        d.orientation[0] = p.body.orientation.w; d.orientation[1] = p.body.orientation.x;
        d.orientation[2] = p.body.orientation.y; d.orientation[3] = p.body.orientation.z;
        d.angularVelocity[0] = p.body.angularVelocity.x; d.angularVelocity[1] = p.body.angularVelocity.y; d.angularVelocity[2] = p.body.angularVelocity.z;
        d.aimOffset[0] = p.aimOffset.w; d.aimOffset[1] = p.aimOffset.x;
        d.aimOffset[2] = p.aimOffset.y; d.aimOffset[3] = p.aimOffset.z;
    }
    fn(LiveBehavior::hostContext(0), &a);
    if (!a.handled)
        return false;
    for (std::uint32_t i = 0; i < n; ++i) {
        const GameRagdollAimLimbV1& d = a.limbs[i];
        b.parts[i].body.angularVelocity =
            glm::vec3(d.angularVelocity[0], d.angularVelocity[1], d.angularVelocity[2]);
    }
    return true;
}

// Deterministic corpse seed: same world seed, owner, death tick, and event id
// produce the same corpse on every client (FNV-1a over the identity, finalized
// with a splitmix64 step so small id changes fully decorrelate the spawn).
static std::uint64_t corpseSeedFor(const std::string& actorId, std::uint32_t ownerId,
                                   std::uint32_t deathTick, std::uint32_t deathEventId,
                                   std::uint64_t worldSeed)
{
    std::uint64_t h = 1469598103934665603ull ^ worldSeed;
    auto mix = [&h](std::uint64_t v) {
        h ^= v;
        h *= 1099511628211ull;
    };
    for (unsigned char c : actorId)
        mix(c);
    mix(ownerId);
    mix(deathTick);
    mix(deathEventId);
    h += 0x9E3779B97F4A7C15ull;
    h = (h ^ (h >> 30)) * 0xBF58476D1CE4E5B9ull;
    h = (h ^ (h >> 27)) * 0x94D049BB133111EBull;
    return h ^ (h >> 31);
}

// xorshift64* stream so a corpse's random tumble is reproducible from its seed.
static float seededSigned(std::uint64_t& state)
{
    state ^= state >> 12;
    state ^= state << 25;
    state ^= state >> 27;
    const std::uint64_t v = state * 2685821657736338717ull;
    return ((float)((v >> 11) % 2001) / 1000.0f - 1.0f) * 2.0f;
}


using Ragdoll::rigidWorld;

// The hand is the capsule endpoint farther from the shoulder joint anchor.
static glm::vec3 partHandWorld(const RagdollModePart& part)
{
    Capsule cap = capsuleOf(part.body);
    glm::vec3 shoulder = part.body.position;
    if (part.parentIndex >= 0)
        shoulder = part.body.position + part.body.orientation * part.childLocalAnchor;
    return (glm::length(cap.a - shoulder) > glm::length(cap.b - shoulder)) ? cap.a : cap.b;
}

static bool raycastWorld(const World& world, const glm::vec3& origin,
                          const glm::vec3& dir, float maxDist, float radius,
                          glm::vec3& hitPoint, glm::vec3& hitNormal)
{
    float closestDist = maxDist;
    bool hit = false;

    AABB queryBounds;
    queryBounds.min = glm::min(origin, origin + dir * maxDist) - glm::vec3(1.0f);
    queryBounds.max = glm::max(origin, origin + dir * maxDist) + glm::vec3(1.0f);
    std::vector<int> candidates;
    appendChunkTrianglesForAABB(const_cast<World&>(world), queryBounds, 0.1f, candidates, "ragdollModeGrab");

    for (int ti : candidates) {
        if (ti < 0 || ti >= (int)world.collisionMesh.triangles.size())
            continue;
        const CollisionTriangle& tri = world.collisionMesh.triangles[ti];
        float dist = 0.0f;
        glm::vec3 normal, point;
        if (sweptSphereTriangle(origin, dir, radius, tri, maxDist, dist, normal, point)) {
            if (dist < closestDist) {
                closestDist = dist;
                hitPoint = point;
                hitNormal = normal;
                hit = true;
            }
        }
    }
    return hit;
}

// Build an orientation quaternion from a look direction using the ragdoll
// convention: local +Y = forward, local +Z = up, local +X = right.
static glm::quat lookRotation(glm::vec3 forward, glm::vec3 up)
{
    forward = glm::normalize(forward);
    up = glm::normalize(up);

    glm::vec3 right = glm::cross(forward, up);
    if (glm::length(right) < 0.001f)
        right = glm::cross(forward, glm::vec3(1.0f, 0.0f, 0.0f));
    right = glm::normalize(right);

    glm::vec3 trueUp = glm::normalize(glm::cross(right, forward));
    glm::mat3 basis(right, forward, trueUp);
    return glm::normalize(glm::quat_cast(basis));
}

RagdollModeSystem& RagdollModeSystem::instance()
{
    static RagdollModeSystem sys;
    return sys;
}

void RagdollModeSystem::activate(Player& player)
{
    const auto& cfg = RagdollModeConfig::instance().data();
    if (!cfg.enabled) return;

    mActive = true;
    mAlive = RagdollBody{};
    mCameraSmoothInit = false;

    // Reset the solver domain accumulator so activation never bursts through
    // the configured catch-up with stale time.
    if (Sim::SimulationDomain* domain = Sim::DomainScheduler::instance().find("ragdoll.solver"))
        domain->accumulator = 0.0;

    // Bind from the model's rest pose, not the currently animated pose.
    // Otherwise a limb's current animation rotation (for example an arm swung
    // about the shoulder mid-walk) is baked into meshLocal, and the visible
    // limb stays stuck at that angle relative to its ragdoll capsule. Resetting
    // the skeleton first snaps every limb to the capsule's canonical bind
    // frame; physics then owns it.
    {
        const size_t n = std::min(player.perfectPoseSkeleton.nodes.size(),
                                  player.perfectPoseSkeleton.restLocalTransforms.size());
        for (size_t i = 0; i < n; ++i)
            player.perfectPoseSkeleton.nodes[i].localTransform =
                player.perfectPoseSkeleton.restLocalTransforms[i];
    }
    player.updateModelWorldTransforms();
    mAlive.torsoPosition = player.pos;
    mAlive.rootWorldPosition = player.pos;
    initParts(player, mAlive);
    mAppliedConfigGeneration = cfg.generation;

    // The entity component store is the canonical alive-ragdoll home. Drop any
    // binding from a previous activation so the next update binds this fresh
    // body as the new source of truth.
    Ragdoll::RagdollEntities::instance().unbind(mOwnerActorId);

    Debug::warn(Debug::Category::Ragdoll,
        "[RAGDOLL MODE] Activated — %zu parts\n", mAlive.parts.size());

    if (StructuredLogger::instance().shouldLog(StructuredCategory::Ragdoll, StructuredLevel::Trace)) {
        char msg[1024];
        snprintf(msg, sizeof(msg),
            "[RAGDOLLDEBUG] RAGDOLL_ENTER player=%s pos=(%.3f,%.3f,%.3f) "
            "config=config/ragdoll.json grounded=%d yaw=%.3f tick=%u parts=%zu",
            player.username.c_str(),
            player.pos.x, player.pos.y, player.pos.z,
            (int)player.ground.onGround,
            player.yaw,
            (uint32_t)player.movementSimulationTick,
            mAlive.parts.size());
        StructuredLogger::Entry e;
        e.category = StructuredCategory::Ragdoll;
        e.level = StructuredLevel::Trace;
        e.eventId = "RAGDOLL_ENTER";
        e.correlationId = player.username;
        e.reason = "Player entered ragdoll mode";
        e.sourceFile = __FILE__;
        e.sourceLine = __LINE__;
        e.functionName = __FUNCTION__;
        e.tick = (uint32_t)player.movementSimulationTick;
        e.message = msg;
        StructuredLogger::instance().write(e);
    }
}

void RagdollModeSystem::deactivate(Player& player)
{
    RagdollBody& b = mAlive;
    if (b.torsoIndex >= 0 && b.torsoIndex < (int)b.parts.size()) {
        const RigidBody& torso = mAlive.parts[mAlive.torsoIndex].body;
        glm::mat4 torsoWorld = rigidWorld(torso);
        // Return the authoritative root to the same body-relative point it was
        // anchored at, so repeated toggles do not drift.
        player.pos = glm::vec3(torsoWorld * glm::vec4(mAlive.rootOffsetLocal, 1.0f));
        const auto& cfg = RagdollModeConfig::instance().data();
        player.vel = cfg.exitPreserveVelocity ? torso.linearVelocity : glm::vec3(0.0f);
        player.vel.z += cfg.exitHopVelocity;
    }

    // Restore any skeleton nodes the ragdoll neutralized, so the normal
    // animation root is valid again on the next frame.
    for (int anc : mAlive.rootAncestorNodes) {
        if (anc >= 0 && anc < (int)player.perfectPoseSkeleton.nodes.size())
            player.perfectPoseSkeleton.nodes[anc].localTransform =
                player.perfectPoseSkeleton.restLocalTransforms[anc];
    }

    player.modelRootRotationActive = false;
    player.modelRootRotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);

    mActive = false;
    mAlive = RagdollBody{};
    mCameraSmoothInit = false;

    // Release the canonical alive-ragdoll state for this owner.
    Ragdoll::RagdollEntities::instance().unbind(mOwnerActorId);

    Debug::warn(Debug::Category::Ragdoll,
        "[RAGDOLL MODE] Deactivated\n");
}

void RagdollModeSystem::initParts(const Player& player, RagdollBody& b)
{
    Ragdoll::buildBody(player, RagdollModeConfig::instance().data(), b);
}

// Re-derives capsule and attachment geometry from config while keeping the
// current body poses and velocities. Used for live ragdoll.json tuning.
void RagdollModeSystem::reinitPreservingState(Player& player, RagdollBody& b)
{
    struct Saved {
        glm::vec3 position;
        glm::quat orientation;
        glm::vec3 linearVelocity;
        glm::vec3 angularVelocity;
    };
    std::vector<std::pair<std::string, Saved>> saved;
    saved.reserve(b.parts.size());
    for (const auto& part : b.parts) {
        saved.push_back({part.name,
            {part.body.position, part.body.orientation,
             part.body.linearVelocity, part.body.angularVelocity}});
    }

    // Re-bind from the rest pose (not the live ragdoll pose) so meshLocal stays
    // the true body-to-mesh bind offset; the saved body poses below restore the
    // current ragdoll configuration on top of it.
    {
        const size_t n = std::min(player.perfectPoseSkeleton.nodes.size(),
                                  player.perfectPoseSkeleton.restLocalTransforms.size());
        for (size_t i = 0; i < n; ++i)
            player.perfectPoseSkeleton.nodes[i].localTransform =
                player.perfectPoseSkeleton.restLocalTransforms[i];
    }
    player.updateModelWorldTransforms();
    initParts(player, b);

    for (auto& part : b.parts) {
        for (const auto& s : saved) {
            if (s.first != part.name) continue;
            part.body.position = s.second.position;
            part.body.orientation = s.second.orientation;
            part.body.linearVelocity = s.second.linearVelocity;
            part.body.angularVelocity = s.second.angularVelocity;
            break;
        }
    }

    if (b.torsoIndex >= 0) {
        glm::mat4 torsoWorld = rigidWorld(b.parts[b.torsoIndex].body);
        b.rootOffsetLocal = glm::vec3(glm::inverse(torsoWorld) * glm::vec4(b.rootWorldPosition, 1.0f));
    }

    Debug::log(Debug::Category::Ragdoll,
        "[RAGDOLL MODE] Geometry re-initialized (config generation %llu, parts=%zu)\n",
        (unsigned long long)mAppliedConfigGeneration, b.parts.size());
}

void RagdollModeSystem::update(float dt, const World& world, Player& player,
                                const InputState& input, const Camera& camera)
{
    if (!mActive) return;
    RagdollBody& b = mAlive;

    const auto& cfg = RagdollModeConfig::instance().data();

    Ragdoll::RagdollEntities& entities = Ragdoll::RagdollEntities::instance();
    const std::uint32_t owner = mOwnerActorId;

    // Live tuning: apply capsule/attachment geometry changes immediately while
    // preserving the current pose and momentum.
    if (cfg.generation != mAppliedConfigGeneration) {
        mAppliedConfigGeneration = cfg.generation;
        if (!b.parts.empty())
            reinitPreservingState(player, b);
        // Geometry changed: rebuild the canonical limb set from the re-derived
        // body on the next step below.
        entities.unbind(owner);
    }

    // The entity component store is the canonical, hot-reload-durable home for
    // the alive ragdoll. Rehydrate the working body from it so the pose and
    // momentum survive a generation switch instead of trusting the in-memory
    // copy; bind fresh (component store -> body template) when unbound.
    if (entities.bound(owner))
        entities.syncToBody(owner, b);
    else
        entities.bind(owner, b);

    b.activationTime += dt;

    // Step 1: Inputs and physical controls (head aim, arm extension, grabs).
    if (!hotRagdollAim(b, camera, dt))
        applyControls(dt, input, camera, b);
    processGrab(input, camera, world, b);
    processExtend(input, camera, dt, b);

    // Publish the alive ragdoll into the entity components so the solver domain
    // reads and writes the canonical limb state.
    entities.syncFromBody(owner, b);
    entities.setGrab(owner, true, b.leftGrab);
    entities.setGrab(owner, false, b.rightGrab);

    // Base solver policy comes from config; a hot gameplay module may override.
    Ragdoll::SolveParams base;
    base.stiffness = 1.0f;
    base.damping = 1.0f;
    base.iterations = cfg.solverIterations;
    base.gravityScale = cfg.gravityScale;
    entities.setSolveParams(owner, base);
    const Ragdoll::SolveParams params = entities.solveParams(owner);

    // Advance the editable solver domain. At solver_hz the solver substeps more
    // than once per 60 Hz gameplay tick; input and look motors are applied once
    // above. The 60 Hz gameplay tick itself is untouched.
    Sim::DomainScheduler& scheduler = Sim::DomainScheduler::instance();
    scheduler.add("ragdoll.solver", (double)std::max(1.0f, cfg.solverHz));
    scheduler.advance(dt, [&](const Sim::SimulationDomain& domain) {
        if (domain.name != "ragdoll.solver") return;
        MIMITA_TELEMETRY_SCOPE("RagdollSolverSubstep");
        entities.syncToBody(owner, b);
        const float subDt = (float)(1.0 / domain.tickRateHz);
        hotRagdollSolve(b, params, subDt);
        entities.syncFromBody(owner, b);
        Telemetry::EntityCounters counters;
        counters.updates = b.parts.size();
        counters.physicsContacts = 1;
        counters.lastTouchedTick = domain.tick;
        Telemetry::Registry::instance().addEntityCounter(owner, counters);
    });

    // Step 9: Write the authoritative root and skeleton transforms.
    syncToPlayer(player, b);

    if (StructuredLogger::instance().shouldLog(StructuredCategory::Ragdoll, StructuredLevel::Trace)) {
        float totalKE = 0.0f;
        for (const auto& part : b.parts) {
            float spd = glm::length(part.body.linearVelocity);
            totalKE += 0.5f * part.body.mass * spd * spd;
        }
        char msg[1024];
        snprintf(msg, sizeof(msg),
            "[RAGDOLLDEBUG] RAGDOLL_TICK player=%s time=%.3f grounded=%d "
            "yaw=%.3f tick=%u grab_left=%d grab_right=%d kinetic_energy=%.3f parts=%zu",
            player.username.c_str(), b.activationTime,
            (int)player.ground.onGround, player.yaw,
            (uint32_t)player.movementSimulationTick,
            (int)b.leftGrab.active, (int)b.rightGrab.active, totalKE, b.parts.size());
        StructuredLogger::Entry e;
        e.category = StructuredCategory::Ragdoll;
        e.level = StructuredLevel::Trace;
        e.eventId = "RAGDOLL_TICK";
        e.correlationId = player.username;
        e.reason = "Ragdoll mode tick";
        e.sourceFile = __FILE__;
        e.sourceLine = __LINE__;
        e.functionName = __FUNCTION__;
        e.tick = (uint32_t)player.movementSimulationTick;
        e.message = msg;
        StructuredLogger::instance().write(e);
    }
}

bool RagdollModeSystem::grabLimb(bool left, int targetLimbIndex, float strength)
{
    if (!mActive) return false;
    RagdollBody& b = mAlive;
    const int armIndex = left ? b.leftArmIndex : b.rightArmIndex;
    if (armIndex < 0 || armIndex >= (int)b.parts.size()) return false;
    if (targetLimbIndex < 0 || targetLimbIndex >= (int)b.parts.size()) return false;
    if (targetLimbIndex == armIndex) return false;

    RagdollGrabState& grab = left ? b.leftGrab : b.rightGrab;
    RagdollModePart& arm = b.parts[armIndex];
    RagdollModePart& target = b.parts[targetLimbIndex];
    grab.active = true;
    grab.partIndex = armIndex;
    grab.targetPart = targetLimbIndex;
    grab.strength = glm::clamp(strength, 0.0f, 1.0f);
    grab.handPosition = partHandWorld(arm);
    grab.handLocalAnchor = glm::inverse(arm.body.orientation)
        * (grab.handPosition - arm.body.position);
    grab.targetLocalAnchor = glm::inverse(target.body.orientation)
        * (grab.handPosition - target.body.position);
    grab.grabPoint = grab.handPosition;
    return true;
}

void RagdollModeSystem::releaseGrab(bool left)
{
    RagdollGrabState& grab = left ? mAlive.leftGrab : mAlive.rightGrab;
    grab.active = false;
    grab.targetPart = -1;
}

int RagdollModeSystem::grabTargetPart(bool left) const
{
    return left ? mAlive.leftGrab.targetPart : mAlive.rightGrab.targetPart;
}

void RagdollModeSystem::applyControls(float dt, const InputState& input, const Camera& camera, RagdollBody& b)
{
    (void)input;
    const auto& cfg = RagdollModeConfig::instance().data();

    // Physically rotate a body toward the camera look direction. Uses a damped
    // velocity controller (no overshoot) and the part's configured aim axes.
    auto aimAtCamera = [&](RagdollModePart& part, float strength, float maxSpeed) {
        RigidBody& body = part.body;

        glm::vec3 upHint(0.0f, 0.0f, 1.0f);
        if (glm::length(glm::cross(camera.front, upHint)) < 0.05f)
            upHint = camera.up;

        glm::quat target = lookRotation(camera.front, upHint) * part.aimOffset;
        glm::quat diff = glm::normalize(target * glm::inverse(body.orientation));
        float w = glm::clamp(diff.w, -1.0f, 1.0f);
        float angle = 2.0f * std::acos(std::fabs(w));
        float s = std::sqrt(std::max(0.0f, 1.0f - w * w));
        glm::vec3 axis = (s > 1e-4f) ? glm::vec3(diff.x, diff.y, diff.z) / s
                                     : glm::vec3(0.0f, 0.0f, 1.0f);
        if (w < 0.0f) axis = -axis;

        float desiredSpeed = glm::clamp(angle * strength, -maxSpeed, maxSpeed);
        glm::vec3 desiredVel = axis * desiredSpeed;
        float blend = glm::clamp(dt * cfg.lookDamping, 0.0f, 1.0f);
        body.angularVelocity += (desiredVel - body.angularVelocity) * blend;

        float spd = glm::length(body.angularVelocity);
        if (spd > maxSpeed)
            body.angularVelocity *= maxSpeed / spd;
    };

    if (b.headIndex >= 0 && b.headIndex < (int)b.parts.size())
        aimAtCamera(b.parts[b.headIndex], cfg.headRotationStrength, cfg.headRotationSpeed);

    // The torso strongly wishes to face where the camera looks, so the body
    // reads clearly in third person. Strength and max speed are tunable.
    if (b.torsoIndex >= 0 && b.torsoIndex < (int)b.parts.size())
        aimAtCamera(b.parts[b.torsoIndex], cfg.torsoLookSpring, cfg.torsoMaxAngularStep);
}

void RagdollModeSystem::processGrab(const InputState& input, const Camera& camera, const World& world, RagdollBody& b)
{
    const auto& cfg = RagdollModeConfig::instance().data();

    auto tryGrab = [&](RagdollGrabState& grab, int armIndex) {
        if (armIndex < 0 || armIndex >= (int)b.parts.size()) return;
        RagdollModePart& arm = b.parts[armIndex];
        glm::vec3 handWorld = partHandWorld(arm);
        glm::vec3 rayDir = glm::normalize(camera.front);
        glm::vec3 hitPoint, hitNormal;
        if (!raycastWorld(world, handWorld, rayDir, cfg.grabReach, cfg.grabRadius,
                          hitPoint, hitNormal))
            return;

        // Only grab within the grace radius of the hand, otherwise the hand
        // would teleport to a distant point and yank the body into geometry.
        float dist = glm::length(hitPoint - handWorld);
        if (dist > cfg.grabGraceDistance + arm.body.capsuleRadius)
            return;

        grab.active = true;
        grab.grabPoint = hitPoint;
        grab.grabNormal = hitNormal;
        grab.handPosition = handWorld;
        grab.handLocalAnchor = glm::inverse(arm.body.orientation)
            * (handWorld - arm.body.position);
        grab.partIndex = armIndex;
        grab.targetPart = -1;
        grab.strength = 1.0f;
    };

    bool leftHeld = input.grabLeftHeld;
    if (leftHeld && !b.leftGrab.active)
        tryGrab(b.leftGrab, b.leftArmIndex);
    else if (!leftHeld)
        b.leftGrab.active = false;
    b.leftGrab.wasActive = b.leftGrab.active;

    bool rightHeld = input.grabRightHeld;
    if (rightHeld && !b.rightGrab.active)
        tryGrab(b.rightGrab, b.rightArmIndex);
    else if (!rightHeld)
        b.rightGrab.active = false;
    b.rightGrab.wasActive = b.rightGrab.active;
}

void RagdollModeSystem::processExtend(const InputState& input, const Camera& camera, float dt, RagdollBody& b)
{
    const auto& cfg = RagdollModeConfig::instance().data();
    glm::vec3 targetDir = glm::normalize(camera.front);

    b.leftArmExtending = input.extendLeftMouse && b.leftArmIndex >= 0;
    b.rightArmExtending = input.extendRightMouse && b.rightArmIndex >= 0;

    // Steer the arm so the hand points along camera-forward. This changes only
    // the arm's angular velocity (an internal motor at the shoulder), so it adds
    // no net linear momentum and cannot pull the player forward. The rigid
    // shoulder attachment keeps the hand within arm's length of the body.
    auto extend = [&](int armIndex) {
        if (armIndex < 0 || armIndex >= (int)b.parts.size()) return;
        RagdollModePart& part = b.parts[armIndex];
        RigidBody& arm = part.body;

        glm::vec3 shoulder = arm.position + arm.orientation * part.childLocalAnchor;
        glm::vec3 hand = partHandWorld(part);
        glm::vec3 current = hand - shoulder;
        float currentLen = glm::length(current);
        if (currentLen < 1e-5f) return;
        current /= currentLen;

        float dotp = glm::clamp(glm::dot(current, targetDir), -1.0f, 1.0f);
        glm::vec3 axis = glm::cross(current, targetDir);
        float axisLen = glm::length(axis);
        if (axisLen < 1e-5f) return;
        axis /= axisLen;

        float angle = std::acos(dotp);
        float desiredSpeed = glm::clamp(angle * cfg.armExtendStrength,
                                        -cfg.armExtendMaxSpeed, cfg.armExtendMaxSpeed);
        float currentSpeed = glm::dot(arm.angularVelocity, axis);
        float blend = glm::clamp(dt * 12.0f, 0.0f, 1.0f);
        arm.angularVelocity += axis * ((desiredSpeed - currentSpeed) * blend);

        // Reach: push the arm away from the shoulder until it hits max stretch.
        // The one-sided shoulder joint lets it telescope, so this does not drag
        // the body until the arm is fully extended (and only then, if body_pull
        // is enabled, or when the hand is grabbing the world).
        if (part.parentIndex >= 0 && part.parentIndex < (int)b.parts.size()) {
            const RigidBody& parent = b.parts[part.parentIndex].body;
            glm::vec3 parentAnchor = parent.position
                + parent.orientation * part.parentLocalAnchor;
            float sep = glm::length(shoulder - parentAnchor);
            float maxSep = part.restLength + part.maxStretch;
            if (sep < maxSep) {
                arm.linearVelocity += targetDir * (cfg.armStretchForce * dt
                                                   / std::max(arm.mass, 0.001f));
            } else if (cfg.armBodyPull > 0.0f && b.torsoIndex >= 0) {
                RigidBody& torso = b.parts[b.torsoIndex].body;
                torso.linearVelocity += targetDir * (cfg.armBodyPull * dt
                                                     / std::max(torso.mass, 0.001f));
            }
        }
    };

    if (b.leftArmExtending) extend(b.leftArmIndex);
    if (b.rightArmExtending) extend(b.rightArmIndex);
}

void RagdollModeSystem::syncToPlayer(Player& player, RagdollBody& b)
{
    Ragdoll::applyBodyToPlayer(player, b, RagdollModeConfig::instance().data());
}

glm::vec3 RagdollModeSystem::getHeadPosition() const
{
    const RagdollBody& b = mAlive;
    if (b.headIndex >= 0 && b.headIndex < (int)b.parts.size())
        return b.parts[b.headIndex].body.position;
    return b.torsoPosition + glm::vec3(0, 0, 1.15f);
}

glm::mat4 RagdollModeSystem::getHeadTransform() const
{
    const RagdollBody& b = mAlive;
    if (b.headIndex >= 0 && b.headIndex < (int)b.parts.size()) {
        const RigidBody& head = b.parts[b.headIndex].body;
        return glm::translate(glm::mat4(1.0f), head.position)
             * glm::mat4_cast(head.orientation);
    }
    return glm::translate(glm::mat4(1.0f), b.torsoPosition + glm::vec3(0, 0, 1.15f));
}

glm::vec3 RagdollModeSystem::computeCameraPosition(float dt)
{
    RagdollBody& b = mAlive;
    const auto& cfg = RagdollModeConfig::instance().data();
    glm::vec3 headPos = getHeadPosition();

    if (!mCameraSmoothInit) {
        mCameraSmoothPos = headPos;
        mCameraSmoothInit = true;
        return mCameraSmoothPos;
    }

    if (cfg.cameraSmoothFactor <= 0.001f) {
        mCameraSmoothPos = headPos;
    } else {
        float smoothTime = cfg.cameraSmoothFactor * 0.08f;
        float alpha = dt / (dt + smoothTime);
        alpha = glm::clamp(alpha, 0.0f, 1.0f);
        mCameraSmoothPos += (headPos - mCameraSmoothPos) * alpha;
    }
    return mCameraSmoothPos;
}

void RagdollModeSystem::render(const Camera& camera) const
{
    if (!mActive) return;
    const RagdollBody& b = mAlive;

    const auto& rcfg = RagdollModeConfig::instance().data();
    if (!rcfg.debugHitboxesVisible) return;

    const bool showAttach = rcfg.attachmentsVisible || DebugConfig::DEBUG_RAGDOLL;

    if (showAttach) {
        DebugVis::drawWeaponWireSphere(camera, b.rootWorldPosition, 0.07f, glm::vec4(1, 1, 1, 1));
        DebugVis::drawWorldLabel(b.rootWorldPosition + glm::vec3(0, 0, 0.12f), "plrOrigin", glm::vec4(1, 1, 1, 1));
        DebugVis::drawWeaponLine(camera, b.torsoPosition, b.rootWorldPosition, glm::vec4(1, 1, 1, 0.5f));
    }

    for (size_t i = 0; i < b.parts.size(); ++i) {
        const auto& part = b.parts[i];

        float alpha = 0.9f;
        const auto& capsules = RagdollModeConfig::instance().data().capsules;
        auto capCfg = capsules.find(part.name);
        if (capCfg != capsules.end()) alpha = glm::clamp(capCfg->second.alpha, 0.0f, 1.0f);

        glm::vec4 color(0.2f, 0.8f, 1.0f, alpha);
        if (part.name == "head") color = glm::vec4(1.0f, 0.3f, 0.3f, alpha);
        else if (part.name == "torso") color = glm::vec4(0.2f, 0.8f, 1.0f, alpha);
        else if (part.name == "leftArm") color = glm::vec4(0.3f, 1.0f, 0.3f, alpha);
        else if (part.name == "rightArm") color = glm::vec4(0.3f, 0.3f, 1.0f, alpha);
        else if (part.name == "leftLeg") color = glm::vec4(1.0f, 1.0f, 0.3f, alpha);
        else if (part.name == "rightLeg") color = glm::vec4(1.0f, 0.3f, 1.0f, alpha);

        Capsule cap = capsuleOf(part.body);
        DebugVis::drawWeaponCapsuleWire(camera, cap, color);

        if (part.parentIndex >= 0) {
            const RigidBody& parent = b.parts[part.parentIndex].body;
            glm::vec3 parentAttach = parent.position
                + parent.orientation * part.parentLocalAnchor;
            glm::vec3 childAttach = part.body.position
                + part.body.orientation * part.childLocalAnchor;

            DebugVis::drawWeaponLine(camera, parentAttach, childAttach,
                glm::vec4(1.0f, 1.0f, 0.0f, 0.85f));

            if (showAttach) {
                DebugVis::drawWeaponWireSphere(camera, parentAttach, 0.05f, glm::vec4(0, 1, 1, 1));
                DebugVis::drawWeaponWireSphere(camera, childAttach, 0.05f, glm::vec4(1, 0, 1, 1));
                DebugVis::drawWeaponLine(camera, parentAttach, b.rootWorldPosition,
                    glm::vec4(1.0f, 0.5f, 0.0f, 0.55f));
                DebugVis::drawWeaponLine(camera, childAttach, part.body.position,
                    glm::vec4(0.6f, 0.6f, 0.6f, 0.6f));

                // Body frame axes (X red, Y green, Z blue).
                glm::mat4 bodyM = rigidWorld(part.body);
                const float ax = 0.18f;
                DebugVis::drawWeaponLine(camera, part.body.position,
                    part.body.position + glm::vec3(bodyM[0]) * ax, glm::vec4(1, 0.1f, 0.1f, 0.9f));
                DebugVis::drawWeaponLine(camera, part.body.position,
                    part.body.position + glm::vec3(bodyM[1]) * ax, glm::vec4(0.1f, 1, 0.1f, 0.9f));
                DebugVis::drawWeaponLine(camera, part.body.position,
                    part.body.position + glm::vec3(bodyM[2]) * ax, glm::vec4(0.1f, 0.4f, 1, 0.9f));

                char lbl[96];
                snprintf(lbl, sizeof(lbl), "%s attach", part.name.c_str());
                DebugVis::drawWorldLabel(childAttach + glm::vec3(0, 0, 0.08f), lbl,
                    glm::vec4(1, 0, 1, 1));
            }
        }

        if (b.leftGrab.active && b.leftGrab.partIndex == (int)i) {
            DebugVis::drawWeaponWireSphere(camera, b.leftGrab.grabPoint, 0.1f,
                glm::vec4(0.0f, 1.0f, 0.0f, 1.0f));
            DebugVis::drawWeaponLine(camera, part.body.position, b.leftGrab.grabPoint,
                glm::vec4(0.0f, 1.0f, 0.0f, 0.9f));
        }
        if (b.rightGrab.active && b.rightGrab.partIndex == (int)i) {
            DebugVis::drawWeaponWireSphere(camera, b.rightGrab.grabPoint, 0.1f,
                glm::vec4(0.0f, 0.0f, 1.0f, 1.0f));
            DebugVis::drawWeaponLine(camera, part.body.position, b.rightGrab.grabPoint,
                glm::vec4(0.0f, 0.0f, 1.0f, 0.9f));
        }

        if (DebugConfig::DEBUG_RAGDOLL) {
            char label[128];
            snprintf(label, sizeof(label), "%s", part.name.c_str());
            DebugVis::drawWorldLabel(part.body.position + glm::vec3(0, 0, 0.3f),
                label, color);
        }
    }
}

// ============================================================================
// Corpse ragdolls
//
// A corpse is a cloned Player body driven by the same RigidBody solver as the
// alive ragdoll, but with no input, grabs, or look motors. This is the death
// presentation for players and NPCs (they share the Player body type). It is
// simulated client-side now; the spawn parameters are shaped so the server can
// own and broadcast the same event later.
// ============================================================================

void RagdollModeSystem::spawnCorpse(const Player& victim,
                                    const glm::vec3& deathImpulse,
                                    const std::string& actorId,
                                    uint32_t ownerId,
                                    uint32_t deathTick,
                                    uint32_t deathEventId)
{
    const auto& cfg = RagdollModeConfig::instance().data();
    if (!cfg.enabled) return;
    if (!RagdollDeathConfig::instance().enabled()) return;
    if (victim.physicalBody.parts.empty() || victim.physicalBody.partMeshes.empty())
        return;

    RagdollCorpse corpse;
    corpse.actorId = actorId.empty() ? victim.username : actorId;
    corpse.ownerId = ownerId;
    corpse.deathTick = deathTick;
    corpse.deathEventId = deathEventId;
    corpse.seed = corpseSeedFor(corpse.actorId, ownerId, deathTick, deathEventId, 0);
    mLastCorpseSeed = corpse.seed;
    corpse.actor = victim;
    corpse.actor.dead = true;
    corpse.actor.netPredictedDead = false;
    corpse.actor.currentHp = 0;
    // Mark the clone as ragdoll-driven so the render path keeps the physics
    // pose instead of hiding the dead body or re-running procedural animation.
    corpse.actor.ragdollModeActive = true;
    corpse.actor.deathAnim = Player::DeathAnimState{};
    corpse.lifetime = std::max(1.0f, cfg.corpseLifetimeSeconds);
    corpse.age = 0.0f;
    corpse.fade = 0.0f;
    corpse.bloodTimer = 0.0f;
    corpse.bloodInit = false;

    // Build the physical parts from the victim's current (frozen) pose.
    initParts(victim, corpse.body);

    // Inherit the dying actor's momentum and add the killing-blow impulse.
    // The tumble comes from the deterministic seed, so every client that sees
    // the same death produces the same corpse.
    const glm::vec3 playerVel = victim.vel + victim.externalImpulse;
    const glm::vec3 impulse = deathImpulse * cfg.corpseDeathImpulseMultiplier;
    std::uint64_t rng = corpse.seed ? corpse.seed : 0x9E3779B97F4A7C15ull;
    for (auto& part : corpse.body.parts) {
        part.body.linearVelocity =
            playerVel * cfg.corpseSpawnVelocityMultiplier +
            impulse / std::max(part.body.mass, 0.001f);
        part.body.angularVelocity += glm::vec3(
            seededSigned(rng), seededSigned(rng), seededSigned(rng));
    }

    // Write the initial skeleton so the clone renders at its physics pose.
    syncToPlayer(corpse.actor, corpse.body);

    mLastCorpseInfo.ownerId = ownerId;
    mLastCorpseInfo.deathTick = deathTick;
    mLastCorpseInfo.deathEventId = deathEventId;
    mLastCorpseInfo.impulse = deathImpulse;
    mLastCorpseInfo.actorId = corpse.actorId;
    ++mCorpseSerial;

    // Death mist at the point of death (was the DeathGhost ellipsoid).
    const auto& deCfg = HitEffects::config().deathEllipsoid;
    if (deCfg.enabled) {
        glm::vec3 dir = glm::length(deathImpulse) > 0.001f
            ? glm::normalize(deathImpulse) : glm::vec3(0.0f, 0.0f, -1.0f);
        EffectPartSystem::instance().spawnDeathEllipsoid(
            victim.pos, dir, deCfg.length, deCfg.radius, deCfg.lifetime,
            victim.sizeScale);
    }

    // Bounded corpse count keeps the solver cost predictable. The bound is a
    // hot-reloadable config value (config/ragdoll.json corpse.max_corpses).
    const size_t maxCorpses = static_cast<size_t>(std::max(1, cfg.maxCorpses));
    while (mCorpses.size() >= maxCorpses)
        mCorpses.erase(mCorpses.begin());

    mCorpses.push_back(std::move(corpse));

    Debug::log(Debug::Category::Ragdoll,
        "[RAGDOLL CORPSE] spawned actor=%s id=%u parts=%zu lifetime=%.1fs\n",
        corpse.actorId.c_str(), corpse.ownerId,
        mCorpses.back().body.parts.size(), cfg.corpseLifetimeSeconds);
}

void RagdollModeSystem::sprayCorpseBlood(RagdollCorpse& corpse, float dt)
{
    RagdollBody& b = corpse.body;
    if (b.parts.empty()) return;

    corpse.bloodTimer -= dt;
    if (corpse.bloodTimer > 0.0f) return;
    corpse.bloodTimer = std::max(0.02f,
        RagdollModeConfig::instance().data().corpseBloodIntervalSeconds);

    // Spray from the fastest-moving part (the body pouring blood as it flies
    // and tumbles). If everything has settled, bleed from the torso root once.
    int fastest = b.torsoIndex >= 0 ? b.torsoIndex : 0;
    float bestSpeed = 0.0f;
    for (int i = 0; i < (int)b.parts.size(); ++i) {
        const float s = glm::length(b.parts[i].body.linearVelocity);
        if (s > bestSpeed) { bestSpeed = s; fastest = i; }
    }

    const RagdollModePart& part = b.parts[fastest];
    const glm::vec3 pos = part.body.position;
    glm::vec3 dir = glm::length(part.body.linearVelocity) > 0.05f
        ? glm::normalize(part.body.linearVelocity)
        : glm::vec3(0.0f, 0.0f, -1.0f);

    const float damage = glm::clamp(bestSpeed * 6.0f, 5.0f, 80.0f);
    EffectPartSystem::instance().spawnBloodEffect(
        pos, dir, damage, corpse.actorId, corpse.actorId, 0.8f, -1.0f);
}

void RagdollModeSystem::updateCorpses(float dt, const World& world)
{
    if (mCorpses.empty()) return;
    const auto& cfg = RagdollModeConfig::instance().data();

    for (auto it = mCorpses.begin(); it != mCorpses.end();) {
        RagdollCorpse& corpse = *it;
        corpse.age += dt;
        if (corpse.age >= corpse.lifetime) {
            it = mCorpses.erase(it);
            continue;
        }

        const float fadeStart = std::max(0.0f, corpse.lifetime - cfg.corpseFadeSeconds);
        if (corpse.age > fadeStart && cfg.corpseFadeSeconds > 0.0f) {
            corpse.fade = glm::clamp(
                (corpse.age - fadeStart) / cfg.corpseFadeSeconds, 0.0f, 1.0f);
        }

        // Corpses run the same component-driven solve core. They substep at the
        // editable solver rate (solver_hz / 60) so death presentation is
        // deterministic and independent of the render rate.
        Ragdoll::SolveParams params;
        params.iterations = cfg.solverIterations;
        params.gravityScale = cfg.gravityScale;
        const int steps = std::max(1, (int)std::lround(cfg.solverHz / 60.0f));
        const float sub = dt / (float)steps;
        for (int s = 0; s < steps; ++s)
            hotRagdollSolve(corpse.body, params, sub);
        syncToPlayer(corpse.actor, corpse.body);

        if (cfg.corpseBloodEnabled)
            sprayCorpseBlood(corpse, dt);

        ++it;
    }
}

void RagdollModeSystem::renderCorpses(const Camera& camera) const
{
    for (const RagdollCorpse& corpse : mCorpses) {
        if (corpse.fade >= 1.0f) continue;
        renderNetworkPlayer(corpse.actor, camera, 0, false);
    }
}

void RagdollModeSystem::removeCorpsesForOwner(uint32_t ownerId)
{
    if (ownerId == 0) return;
    mCorpses.erase(
        std::remove_if(mCorpses.begin(), mCorpses.end(),
            [ownerId](const RagdollCorpse& corpse) {
                return corpse.ownerId == ownerId;
            }),
        mCorpses.end());
}

void RagdollModeSystem::clearCorpses()
{
    mCorpses.clear();
}

bool RagdollModeSystem::noteNetworkDeath(std::uint32_t ownerId, std::uint32_t deathTick,
                                         std::uint32_t deathEventId)
{
    if (ownerId == 0)
        return false;
    auto result = mNetworkDeaths.emplace(ownerId, std::make_pair(deathTick, deathEventId));
    if (!result.second)
        result.first->second = std::make_pair(deathTick, deathEventId);
    return true;
}

bool RagdollModeSystem::consumeNetworkDeath(std::uint32_t ownerId, std::uint32_t& deathTick,
                                            std::uint32_t& deathEventId)
{
    auto it = mNetworkDeaths.find(ownerId);
    if (it == mNetworkDeaths.end())
        return false;
    deathTick = it->second.first;
    deathEventId = it->second.second;
    mNetworkDeaths.erase(it);
    return true;
}
