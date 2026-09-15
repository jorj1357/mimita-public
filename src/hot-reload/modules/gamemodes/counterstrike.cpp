// 09 14 2026
/* purpose
* Hot Counter-Strike-like objective round mode. Owns the COMPLETE round
* lifecycle with generic primitives only: phase clock (countdown/active/results/
* intermission/next round), participant team assignment, round-start spawn/reset
* through the generic actor.spawn capability, spawn anchors from generic map
* metadata, and the objective state machine (carrier/plant/defuse/timer/
* explosion/elimination/timeout). It claims MatchPhaseOwnership + ObjectiveOwnership
* so the cold phase machine and bomb policy are bypassed. No BombManager,
* ObjectiveType, or mode/player/NPC-specific ABI.
* Does NOT link into the EXE; only into the replaceable game DLL.
*/
#if defined(MIMITA_GAME_DLL)

#include "hot-reload/game-api.h"
#include "hot-reload/hot-package.h"
#include "network/objective-events.h"

#include <algorithm>
#include <cstdint>
#include <vector>

using MimitaNet::GameObjectiveInteractV1;
using MimitaNet::GameObjectiveStateV1;

namespace {

const std::uint64_t kModeCs = gameHash("counterstrike");
const std::uint64_t kCsDomain = gameHash("mode.counterstrike");

const std::uint64_t kObjState = gameHash("CsObjectiveState");
const std::uint64_t kObjProgress = gameHash("CsProgress");
const std::uint64_t kSiteState = gameHash("CsSiteState");
const std::uint64_t kRoundState = gameHash("CsRoundState");
const std::uint64_t kDead = gameHash("CsDead");

const std::uint64_t kTeamState = gameHash("ActorTeamState");
const std::uint64_t kHealthState = gameHash("ActorHealthState");
const std::uint64_t kPhaseOwnership = gameHash("MatchPhaseOwnership");
const std::uint64_t kObjOwnership = gameHash("ObjectiveOwnership");
const std::uint64_t kCarriedBy = gameHash("objective.carried-by");
const std::uint64_t kAtSite = gameHash("objective.at-site");
const std::uint64_t kInteract = gameHash("objective.interact");
const std::uint64_t kStateChanged = gameHash("objective.state-changed");
const std::uint64_t kActorKilled = gameHash("actor.killed");
const std::uint64_t kSiteKind = gameHash("objective.site");
const std::uint64_t kSpawnKind = gameHash("spawn.team");

// Hot-editable round policy (mode values, not kernel constants).
constexpr float kPlantTicks = 3.0f * 60.0f;
constexpr float kDefuseTicks = 5.0f * 60.0f;
constexpr float kBombTicks = 40.0f * 60.0f;
constexpr std::int32_t kCountdownTicks = 3 * 60;
constexpr std::int32_t kResultsTicks = 5 * 60;
constexpr std::int32_t kIntermissionTicks = 15 * 60;
constexpr std::uint64_t kTimeLimitTicks = 115ull * 60ull;
constexpr float kReachPad = 2.0f;
constexpr std::int32_t kSpawnHealth = 100;

// Wire DuelStatePhase values.
constexpr std::uint32_t kWireCountdown = 1;
constexpr std::uint32_t kWireActive = 2;
constexpr std::uint32_t kWireIntermission = 4;
constexpr std::uint32_t kWireResults = 6;

// Local phase indices (mode-private).
constexpr std::uint32_t kPhaseCountdown = 0;
constexpr std::uint32_t kPhaseActive = 1;
constexpr std::uint32_t kPhaseResults = 2;
constexpr std::uint32_t kPhaseIntermission = 3;

constexpr std::uint32_t kDropped = 0;
constexpr std::uint32_t kCarried = 1;
constexpr std::uint32_t kPlanted = 2;
constexpr std::uint32_t kDefused = 3;
constexpr std::uint32_t kExploded = 4;

struct CsObjectiveStateV1 {
    std::uint32_t state;
    std::uint32_t ownerTeam;
    std::uint32_t reserved0;
    std::uint32_t reserved1;
};
struct CsProgressV1 {
    float plant;
    float defuse;
    float bombTimer;
    float reserved;
};
struct CsSiteStateV1 {
    float radius;
    std::uint32_t index;
};
struct CsRoundStateV1 {
    std::uint32_t phase;         // local phase index
    std::int32_t timer;          // ticks left in the current phase
    std::uint32_t roundNumber;
    std::uint32_t ended;
    std::uint64_t roundStartTick;
    std::uint32_t teamsAssigned;
    std::uint32_t reserved;
};
struct CsDeadV1 {
    std::uint32_t dead;
    std::uint32_t reserved;
};
struct TeamStateV1 {
    std::int32_t team;
    std::uint32_t reserved;
};
struct HealthStateV1 {
    std::int32_t current;
    std::int32_t max;
    std::uint32_t dead;
    std::uint32_t reserved;
};

std::uint64_t currentMatch(GameplayContextV1* ctx)
{
    std::uint64_t match = 0;
    if (ctx && ctx->matchCurrent)
        ctx->matchCurrent(ctx->host, &match);
    return match;
}

bool dynRead(GameplayContextV1* ctx, std::uint64_t e, std::uint64_t type, void* out,
             std::uint32_t size)
{
    return ctx && ctx->dynamicReadComponent &&
           ctx->dynamicReadComponent(ctx->host, e, type, out, size);
}
void dynWrite(GameplayContextV1* ctx, std::uint64_t e, std::uint64_t type,
              const void* in, std::uint32_t size)
{
    if (ctx && ctx->dynamicWriteComponent)
        ctx->dynamicWriteComponent(ctx->host, e, type, in, size);
}

bool entityPos(GameplayContextV1* ctx, std::uint64_t e, float* out)
{
    GameTransformComponentV1 t{};
    if (!ctx || !ctx->readComponent ||
        !ctx->readComponent(ctx->host, e, GAME_COMPONENT_TRANSFORM, &t, sizeof(t)))
        return false;
    out[0] = t.position[0];
    out[1] = t.position[1];
    out[2] = t.position[2];
    return true;
}
void setEntityPos(GameplayContextV1* ctx, std::uint64_t e, float x, float y, float z)
{
    if (!ctx || !ctx->writeComponent)
        return;
    GameTransformComponentV1 t{};
    t.position[0] = x;
    t.position[1] = y;
    t.position[2] = z;
    ctx->writeComponent(ctx->host, e, GAME_COMPONENT_TRANSFORM, &t, sizeof(t));
}

std::int32_t teamOf(GameplayContextV1* ctx, std::uint64_t e)
{
    TeamStateV1 t{};
    if (!dynRead(ctx, e, kTeamState, &t, sizeof(t)))
        return -1;
    return t.team;
}

bool actorAlive(GameplayContextV1* ctx, std::uint64_t e)
{
    CsDeadV1 d{};
    if (dynRead(ctx, e, kDead, &d, sizeof(d)) && d.dead)
        return false;
    HealthStateV1 h{};
    if (dynRead(ctx, e, kHealthState, &h, sizeof(h)) && (h.dead || h.current <= 0))
        return false;
    return true;
}

// Deterministic participant list (players + NPCs) for team assignment/spawn.
std::uint32_t enumerateParticipants(GameplayContextV1* ctx, std::uint64_t* out,
                                    std::uint32_t maxOut)
{
    if (!ctx || !ctx->findEntities)
        return 0;
    std::uint32_t n = 0;
    std::uint64_t buf[64] = {0};
    const std::uint32_t domains[2] = {1u /*player*/, 2u /*npc*/};
    for (std::uint32_t dom : domains) {
        const std::uint32_t c = ctx->findEntities(ctx->host, dom, 0, buf, 64);
        for (std::uint32_t i = 0; i < c && n < maxOut; ++i)
            out[n++] = buf[i];
    }
    std::sort(out, out + n);
    return n;
}

std::vector<std::uint64_t> teamActorsSorted(GameplayContextV1* ctx)
{
    std::uint64_t ids[128] = {0};
    const std::uint32_t n = ctx->dynamicEnumerateComponent
        ? ctx->dynamicEnumerateComponent(ctx->host, kTeamState, ids, 128) : 0;
    std::vector<std::uint64_t> v(ids, ids + n);
    std::sort(v.begin(), v.end());
    return v;
}

void emitStateChanged(GameplayContextV1* ctx, std::uint64_t objective,
                      std::uint64_t actor, std::uint64_t site, std::uint32_t state,
                      std::uint32_t tick)
{
    if (!ctx || !ctx->emitEvent)
        return;
    GameObjectiveStateV1 p{};
    p.objectiveEntity = objective;
    p.actorEntity = actor;
    p.siteEntity = site;
    p.state = state;
    p.tick = tick;
    GameEventV1 e{};
    e.typeId = kStateChanged;
    e.payloadVersion = 1;
    e.payloadSize = sizeof(p);
    e.tick = tick;
    e.payload = &p;
    reinterpret_cast<GameEmitEventFn>(ctx->emitEvent)(ctx, &e);
}

std::uint64_t carriedBy(GameplayContextV1* ctx, std::uint64_t objective)
{
    std::uint64_t to = 0, value = 0;
    if (ctx->relationshipQuery &&
        ctx->relationshipQuery(ctx->host, kCarriedBy, objective, &to, &value, 1) > 0)
        return to;
    return 0;
}
std::uint64_t siteOf(GameplayContextV1* ctx, std::uint64_t objective)
{
    std::uint64_t to = 0, value = 0;
    if (ctx->relationshipQuery &&
        ctx->relationshipQuery(ctx->host, kAtSite, objective, &to, &value, 1) > 0)
        return to;
    return 0;
}

std::uint64_t nearestSite(GameplayContextV1* ctx, const float* pos, float* outRadius)
{
    std::uint64_t sites[16] = {0};
    const std::uint32_t n = ctx->dynamicEnumerateComponent
        ? ctx->dynamicEnumerateComponent(ctx->host, kSiteState, sites, 16) : 0;
    std::uint64_t best = 0;
    float bestD2 = 1e30f;
    for (std::uint32_t i = 0; i < n; ++i) {
        CsSiteStateV1 s{};
        float sp[3] = {0};
        if (!dynRead(ctx, sites[i], kSiteState, &s, sizeof(s)) ||
            !entityPos(ctx, sites[i], sp))
            continue;
        const float dx = pos[0] - sp[0];
        const float dy = pos[1] - sp[1];
        const float dz = pos[2] - sp[2];
        const float d2 = dx * dx + dy * dy + dz * dz;
        const float reach = s.radius + kReachPad;
        if (d2 <= reach * reach && d2 < bestD2) {
            bestD2 = d2;
            best = sites[i];
            if (outRadius)
                *outRadius = s.radius;
        }
    }
    return best;
}

std::uint32_t mapAnchors(GameplayContextV1* ctx, GameMapAnchorV1* out,
                         std::uint32_t maxOut)
{
    if (!ctx || !ctx->resolveCapability)
        return 0;
    auto fn = reinterpret_cast<GameMapAnchorsFn>(
        ctx->resolveCapability(ctx->host, GAME_CAP_MAP_ANCHORS));
    return fn ? fn(ctx->host, out, maxOut) : 0;
}

std::uint64_t ensureSites(GameplayContextV1* ctx)
{
    std::uint64_t sites[16] = {0};
    const std::uint32_t n = ctx->dynamicEnumerateComponent
        ? ctx->dynamicEnumerateComponent(ctx->host, kSiteState, sites, 16) : 0;
    if (n > 0)
        return sites[0];
    if (!ctx->entityCreate)
        return 0;
    GameMapAnchorV1 anchors[GAME_MAX_MAP_ANCHORS];
    const std::uint32_t anchorCount = mapAnchors(ctx, anchors, GAME_MAX_MAP_ANCHORS);
    std::uint32_t created = 0;
    for (std::uint32_t i = 0; i < anchorCount; ++i) {
        if (anchors[i].kind != kSiteKind)
            continue;
        std::uint64_t site = 0;
        if (!ctx->entityCreate(ctx->host, 0, &site) || site == 0)
            continue;
        CsSiteStateV1 s{};
        s.radius = anchors[i].radius;
        s.index = created;
        dynWrite(ctx, site, kSiteState, &s, sizeof(s));
        setEntityPos(ctx, site, anchors[i].position[0], anchors[i].position[1],
                     anchors[i].position[2]);
        ++created;
    }
    if (created == 0) {
        std::uint64_t site = 0;
        if (ctx->entityCreate(ctx->host, 0, &site) && site != 0) {
            CsSiteStateV1 s{};
            s.radius = 6.0f;
            dynWrite(ctx, site, kSiteState, &s, sizeof(s));
            setEntityPos(ctx, site, 10.0f, 0.0f, 0.0f);
            created = 1;
        }
    }
    ctx->dynamicEnumerateComponent(ctx->host, kSiteState, sites, 16);
    return created > 0 ? sites[0] : 0;
}

std::uint64_t ensureObjective(GameplayContextV1* ctx)
{
    std::uint64_t objs[16] = {0};
    const std::uint32_t n = ctx->dynamicEnumerateComponent
        ? ctx->dynamicEnumerateComponent(ctx->host, kObjState, objs, 16) : 0;
    if (n > 0)
        return objs[0];
    std::uint64_t objective = 0;
    if (!ctx->entityCreate || !ctx->entityCreate(ctx->host, 0, &objective))
        return 0;
    CsObjectiveStateV1 s{};
    s.state = kDropped;
    dynWrite(ctx, objective, kObjState, &s, sizeof(s));
    CsProgressV1 p{};
    dynWrite(ctx, objective, kObjProgress, &p, sizeof(p));
    setEntityPos(ctx, objective, 0.0f, 0.0f, 0.0f);
    return objective;
}

void resetObjective(GameplayContextV1* ctx, std::uint64_t objective)
{
    const std::uint64_t carrier = carriedBy(ctx, objective);
    if (carrier != 0 && ctx->relationshipRemove)
        ctx->relationshipRemove(ctx->host, kCarriedBy, objective, carrier);
    const std::uint64_t site = siteOf(ctx, objective);
    if (site != 0 && ctx->relationshipRemove)
        ctx->relationshipRemove(ctx->host, kAtSite, objective, site);
    CsObjectiveStateV1 s{};
    dynRead(ctx, objective, kObjState, &s, sizeof(s));
    s.state = kDropped;
    dynWrite(ctx, objective, kObjState, &s, sizeof(s));
    CsProgressV1 p{};
    dynWrite(ctx, objective, kObjProgress, &p, sizeof(p));
}

void claimOwnership(GameplayContextV1* ctx, std::uint64_t match)
{
    std::uint32_t owned = 1;
    dynWrite(ctx, match, kPhaseOwnership, &owned, sizeof(owned));
    dynWrite(ctx, match, kObjOwnership, &owned, sizeof(owned));
}

// Mode policy: assign team 0/1 deterministically to participants without a team.
void assignTeams(GameplayContextV1* ctx)
{
    std::uint64_t parts[128] = {0};
    const std::uint32_t n = enumerateParticipants(ctx, parts, 128);
    for (std::uint32_t i = 0; i < n; ++i) {
        TeamStateV1 ts{};
        if (dynRead(ctx, parts[i], kTeamState, &ts, sizeof(ts)) && ts.team >= 0)
            continue;  // keep an existing assignment
        ts.team = static_cast<std::int32_t>(i % 2);
        dynWrite(ctx, parts[i], kTeamState, &ts, sizeof(ts));
    }
}

// Mode policy: round-start spawn/reset through the generic actor.spawn mechanism.
void spawnAll(GameplayContextV1* ctx, std::uint32_t tick, std::uint32_t roundNumber)
{
    GameMapAnchorV1 anchors[GAME_MAX_MAP_ANCHORS];
    const std::uint32_t na = mapAnchors(ctx, anchors, GAME_MAX_MAP_ANCHORS);
    std::uint32_t perTeam[2] = {0, 0};
    for (std::uint32_t i = 0; i < na; ++i)
        if (anchors[i].kind == kSpawnKind && anchors[i].tag < 2u)
            ++perTeam[anchors[i].tag];

    GameActorSpawnFn spawnFn = nullptr;
    if (ctx->resolveCapability)
        spawnFn = reinterpret_cast<GameActorSpawnFn>(
            ctx->resolveCapability(ctx->host, GAME_CAP_ACTOR_SPAWN));

    std::uint64_t parts[128] = {0};
    const std::uint32_t n = enumerateParticipants(ctx, parts, 128);
    std::uint32_t used[2] = {0, 0};
    for (std::uint32_t i = 0; i < n; ++i) {
        const std::int32_t team = teamOf(ctx, parts[i]);
        if (team < 0 || team > 1)
            continue;
        // Deterministic anchor choice: spawn anchors of the team, by index.
        std::uint32_t seen = 0;
        const GameMapAnchorV1* chosen = nullptr;
        for (std::uint32_t a = 0; a < na; ++a) {
            if (anchors[a].kind != kSpawnKind || anchors[a].tag != (std::uint32_t)team)
                continue;
            if (perTeam[team] > 0 && seen == (used[team] % perTeam[team]))
                chosen = &anchors[a];
            ++seen;
        }
        float px = 0.0f, py = 0.0f, pz = 0.0f, yaw = 0.0f;
        if (chosen) {
            px = chosen->position[0];
            py = chosen->position[1];
            pz = chosen->position[2];
            yaw = chosen->yaw;
        } else {
            // Fallback: first objective site anchor, else origin.
            for (std::uint32_t a = 0; a < na; ++a) {
                if (anchors[a].kind != kSiteKind)
                    continue;
                px = anchors[a].position[0];
                py = anchors[a].position[1];
                pz = anchors[a].position[2];
                break;
            }
        }
        ++used[team];

        GameActorSpawnV1 req{};
        req.actorEntity = parts[i];
        req.position[0] = px;
        req.position[1] = py;
        req.position[2] = pz;
        req.velocity[0] = 0.0f;
        req.velocity[1] = 0.0f;
        req.velocity[2] = 0.0f;
        req.yaw = yaw;
        req.health = kSpawnHealth;
        req.flags = 1u | 2u | 4u;  // reset velocity, clear dead, reset health
        if (spawnFn)
            spawnFn(ctx->host, &req);
        else
            setEntityPos(ctx, parts[i], px, py, pz);

        // Clear the round dead marker generically.
        CsDeadV1 d{};
        d.dead = 0;
        dynWrite(ctx, parts[i], kDead, &d, sizeof(d));
    }
    (void)roundNumber;
}

bool roundResult(GameplayContextV1* ctx, std::uint32_t winnerTeam,
                 std::uint64_t reason, std::uint32_t tick)
{
    if (!ctx->resolveCapability)
        return false;
    auto fn = reinterpret_cast<GameMatchRoundResultFn>(
        ctx->resolveCapability(ctx->host, GAME_CAP_MATCH_ROUND_RESULT));
    if (!fn)
        return false;
    GameMatchRoundResultV1 r{};
    r.winnerTeam = winnerTeam;
    r.reasonHash = reason;
    r.tick = tick;
    return fn(ctx->host, &r);
}

void MIMITA_GAME_CALL onActorKilled(void* host, const GameEventV1* event)
{
    auto* ctx = static_cast<GameplayContextV1*>(host);
    auto* in = event ? static_cast<GameActorKilledV1*>(event->payload) : nullptr;
    if (!ctx || !in || in->victimEntity == 0)
        return;
    CsDeadV1 d{};
    d.dead = 1;
    dynWrite(ctx, in->victimEntity, kDead, &d, sizeof(d));
}

void MIMITA_GAME_CALL onInteract(void* host, const GameEventV1* event)
{
    auto* ctx = static_cast<GameplayContextV1*>(host);
    auto* in = event ? static_cast<GameObjectiveInteractV1*>(event->payload) : nullptr;
    if (!ctx || !in || in->actorEntity == 0)
        return;
    const std::uint64_t objective = in->objectiveEntity != 0
        ? in->objectiveEntity : ensureObjective(ctx);
    if (objective == 0)
        return;
    CsObjectiveStateV1 s{};
    if (!dynRead(ctx, objective, kObjState, &s, sizeof(s)))
        return;
    if (s.state == kDropped) {
        if (ctx->relationshipAdd)
            ctx->relationshipAdd(ctx->host, kCarriedBy, objective, in->actorEntity, 0);
        s.state = kCarried;
        dynWrite(ctx, objective, kObjState, &s, sizeof(s));
        emitStateChanged(ctx, objective, in->actorEntity, 0, kCarried, in->tick);
    } else if (s.state == kCarried &&
               carriedBy(ctx, objective) == in->actorEntity) {
        if (ctx->relationshipRemove)
            ctx->relationshipRemove(ctx->host, kCarriedBy, objective, in->actorEntity);
        s.state = kDropped;
        dynWrite(ctx, objective, kObjState, &s, sizeof(s));
        emitStateChanged(ctx, objective, 0, 0, kDropped, in->tick);
    }
    in->handled = 1;
}

void phaseTo(GameplayContextV1* ctx, CsRoundStateV1& rs, std::uint32_t phase,
             std::uint32_t wire, std::int32_t timer)
{
    rs.phase = phase;
    rs.timer = timer;
    if (ctx->matchSetPhase)
        ctx->matchSetPhase(ctx->host, wire);
}

void MIMITA_GAME_CALL csTick(void* host, std::uint64_t tick, float /*dt*/)
{
    auto* ctx = static_cast<GameplayContextV1*>(host);
    if (!ctx)
        return;
    const std::uint64_t match = currentMatch(ctx);
    if (match == 0)
        return;
    claimOwnership(ctx, match);

    CsRoundStateV1 rs{};
    if (!dynRead(ctx, match, kRoundState, &rs, sizeof(rs))) {
        rs.phase = kPhaseCountdown;
        rs.timer = kCountdownTicks;
        rs.roundNumber = 1;
        rs.roundStartTick = tick;
        rs.teamsAssigned = 0;
        dynWrite(ctx, match, kRoundState, &rs, sizeof(rs));
    }
    if (!rs.teamsAssigned) {
        assignTeams(ctx);
        rs.teamsAssigned = 1;
    }

    ensureSites(ctx);
    const std::uint64_t objective = ensureObjective(ctx);
    if (objective == 0)
        return;

    const std::uint32_t t32 = static_cast<std::uint32_t>(tick);

    if (rs.phase == kPhaseCountdown) {
        if (rs.timer > 0)
            --rs.timer;
        if (rs.timer <= 0) {
            spawnAll(ctx, t32, rs.roundNumber);
            resetObjective(ctx, objective);
            rs.roundStartTick = tick;
            rs.ended = 0;
            phaseTo(ctx, rs, kPhaseActive, kWireActive, 0);
        }
        dynWrite(ctx, match, kRoundState, &rs, sizeof(rs));
        return;
    }

    if (rs.phase == kPhaseResults) {
        if (rs.timer > 0)
            --rs.timer;
        if (rs.timer <= 0)
            phaseTo(ctx, rs, kPhaseIntermission, kWireIntermission,
                    kIntermissionTicks);
        dynWrite(ctx, match, kRoundState, &rs, sizeof(rs));
        return;
    }

    if (rs.phase == kPhaseIntermission) {
        if (rs.timer > 0)
            --rs.timer;
        if (rs.timer <= 0) {
            ++rs.roundNumber;
            resetObjective(ctx, objective);
            phaseTo(ctx, rs, kPhaseCountdown, kWireCountdown, kCountdownTicks);
        }
        dynWrite(ctx, match, kRoundState, &rs, sizeof(rs));
        return;
    }

    if (rs.phase != kPhaseActive)
        return;

    // ── Active phase: objective state machine (hot-owned) ─────────────
    const std::vector<std::uint64_t> actors = teamActorsSorted(ctx);
    std::uint32_t aliveT = 0, aliveCt = 0;
    for (std::uint64_t a : actors) {
        if (!actorAlive(ctx, a))
            continue;
        const std::int32_t team = teamOf(ctx, a);
        if (team == 0) ++aliveT;
        else if (team == 1) ++aliveCt;
    }

    CsObjectiveStateV1 s{};
    dynRead(ctx, objective, kObjState, &s, sizeof(s));
    CsProgressV1 p{};
    dynRead(ctx, objective, kObjProgress, &p, sizeof(p));

    bool decided = false;
    std::uint32_t winner = 0;
    std::uint64_t reason = 0;

    if (s.state == kDropped) {
        for (std::uint64_t a : actors) {
            if (teamOf(ctx, a) == 0 && actorAlive(ctx, a)) {
                if (ctx->relationshipAdd)
                    ctx->relationshipAdd(ctx->host, kCarriedBy, objective, a, 0);
                s.state = kCarried;
                dynWrite(ctx, objective, kObjState, &s, sizeof(s));
                emitStateChanged(ctx, objective, a, 0, kCarried, t32);
                break;
            }
        }
    } else if (s.state == kCarried) {
        const std::uint64_t carrier = carriedBy(ctx, objective);
        if (carrier == 0 || !actorAlive(ctx, carrier) || teamOf(ctx, carrier) != 0) {
            if (carrier != 0 && ctx->relationshipRemove)
                ctx->relationshipRemove(ctx->host, kCarriedBy, objective, carrier);
            s.state = kDropped;
            dynWrite(ctx, objective, kObjState, &s, sizeof(s));
            emitStateChanged(ctx, objective, 0, 0, kDropped, t32);
        } else {
            float cpos[3] = {0};
            float radius = 0.0f;
            const std::uint64_t site =
                entityPos(ctx, carrier, cpos) ? nearestSite(ctx, cpos, &radius) : 0;
            if (site != 0) {
                p.plant += 1.0f;
                if (p.plant >= kPlantTicks) {
                    if (ctx->relationshipRemove)
                        ctx->relationshipRemove(ctx->host, kCarriedBy, objective,
                                                carrier);
                    if (ctx->relationshipAdd)
                        ctx->relationshipAdd(ctx->host, kAtSite, objective, site, 0);
                    s.state = kPlanted;
                    s.ownerTeam = 0;
                    p.plant = kPlantTicks;
                    p.bombTimer = kBombTicks;
                    dynWrite(ctx, objective, kObjState, &s, sizeof(s));
                    emitStateChanged(ctx, objective, carrier, site, kPlanted, t32);
                }
            } else {
                p.plant = 0.0f;
            }
        }
    } else if (s.state == kPlanted) {
        const std::uint64_t site = siteOf(ctx, objective);
        float spos[3] = {0};
        float radius = 0.0f;
        if (site != 0) {
            entityPos(ctx, site, spos);
            CsSiteStateV1 ss{};
            if (dynRead(ctx, site, kSiteState, &ss, sizeof(ss)))
                radius = ss.radius;
        }
        bool defusing = false;
        if (site != 0) {
            for (std::uint64_t a : actors) {
                if (teamOf(ctx, a) != 1 || !actorAlive(ctx, a))
                    continue;
                float apos[3] = {0};
                if (!entityPos(ctx, a, apos))
                    continue;
                const float dx = apos[0] - spos[0];
                const float dy = apos[1] - spos[1];
                const float dz = apos[2] - spos[2];
                const float reach = radius + kReachPad;
                if (dx * dx + dy * dy + dz * dz <= reach * reach) {
                    defusing = true;
                    break;
                }
            }
        }
        if (defusing) {
            p.defuse += 1.0f;
            if (p.defuse >= kDefuseTicks) {
                s.state = kDefused;
                dynWrite(ctx, objective, kObjState, &s, sizeof(s));
                dynWrite(ctx, objective, kObjProgress, &p, sizeof(p));
                emitStateChanged(ctx, objective, 0, site, kDefused, t32);
                decided = true;
                winner = 1;
                reason = gameHash("objective.defused");
            }
        } else {
            p.defuse = 0.0f;
        }
        if (!decided) {
            p.bombTimer -= 1.0f;
            if (p.bombTimer <= 0.0f) {
                p.bombTimer = 0.0f;
                s.state = kExploded;
                dynWrite(ctx, objective, kObjState, &s, sizeof(s));
                emitStateChanged(ctx, objective, 0, site, kExploded, t32);
                if (ctx->resolveCapability) {
                    auto dmg = reinterpret_cast<GameDamageApplyFn>(
                        ctx->resolveCapability(ctx->host, GAME_CAP_DAMAGE_APPLY));
                    if (dmg) {
                        for (std::uint64_t a : actors) {
                            if (!actorAlive(ctx, a))
                                continue;
                            float apos[3] = {0};
                            if (!entityPos(ctx, a, apos))
                                continue;
                            const float dx = apos[0] - spos[0];
                            const float dy = apos[1] - spos[1];
                            const float dz = apos[2] - spos[2];
                            if (dx * dx + dy * dy + dz * dz > 8.0f * 8.0f)
                                continue;
                            GameDamageApplyV1 req{};
                            req.victimEntity = a;
                            req.sourceEntity = objective;
                            req.amount = 500;
                            dmg(ctx->host, &req);
                        }
                    }
                }
                decided = true;
                winner = 0;
                reason = gameHash("objective.exploded");
            }
        }
    }

    if (!decided &&
        (s.state == kDropped || s.state == kCarried)) {
        if (aliveT == 0) {
            decided = true; winner = 1; reason = gameHash("objective.elimination");
        } else if (aliveCt == 0) {
            decided = true; winner = 0; reason = gameHash("objective.elimination");
        } else if (rs.roundStartTick != 0 &&
                   tick - rs.roundStartTick >= kTimeLimitTicks) {
            decided = true; winner = 1; reason = gameHash("objective.time");
        }
    }

    if (decided) {
        roundResult(ctx, winner, reason, t32);
        rs.ended = 1;
        phaseTo(ctx, rs, kPhaseResults, kWireResults, kResultsTicks);
        dynWrite(ctx, match, kRoundState, &rs, sizeof(rs));
        return;
    }

    dynWrite(ctx, objective, kObjState, &s, sizeof(s));
    dynWrite(ctx, objective, kObjProgress, &p, sizeof(p));
    dynWrite(ctx, match, kRoundState, &rs, sizeof(rs));
}

} // namespace

const MimitaHotPackage::ModeRegistrar s_csMode{
    {kModeCs, kCsDomain, kObjState, gameHash("CsObjectiveState.v1"),
     "Counter Strike"}};
const MimitaHotPackage::SchemaRegistrar s_csObjSchema{
    {kObjState, gameHash("CsObjectiveState.v1"), sizeof(CsObjectiveStateV1), 4,
     GAME_COPY_AUTHORING, GAME_NET_ALL, "CsObjectiveState", 1, 0}};
const MimitaHotPackage::SchemaRegistrar s_csProgressSchema{
    {kObjProgress, gameHash("CsProgress.v1"), sizeof(CsProgressV1), 4,
     GAME_COPY_AUTHORING, GAME_NET_ALL, "CsProgress", 1, 0}};
const MimitaHotPackage::SchemaRegistrar s_csSiteSchema{
    {kSiteState, gameHash("CsSiteState.v1"), sizeof(CsSiteStateV1), 4,
     GAME_COPY_AUTHORING, GAME_NET_ALL, "CsSiteState", 1, 0}};
const MimitaHotPackage::SchemaRegistrar s_csRoundSchema{
    {kRoundState, gameHash("CsRoundState.v1"), sizeof(CsRoundStateV1), 8,
     GAME_COPY_AUTHORING, GAME_NET_ALL, "CsRoundState", 1, 0}};
const MimitaHotPackage::SchemaRegistrar s_csDeadSchema{
    {kDead, gameHash("CsDead.v1"), sizeof(CsDeadV1), 4, GAME_COPY_AUTHORING,
     GAME_NET_ALL, "CsDead", 1, 0}};
const MimitaHotPackage::SchemaRegistrar s_csPhaseOwnershipSchema{
    {kPhaseOwnership, gameHash("MatchPhaseOwnership.v1"), 4, 4,
     GAME_COPY_AUTHORING, GAME_NET_SERVER_ONLY, "MatchPhaseOwnership", 1, 0}};
const MimitaHotPackage::SchemaRegistrar s_csObjOwnershipSchema{
    {kObjOwnership, gameHash("ObjectiveOwnership.v1"), 4, 4, GAME_COPY_AUTHORING,
     GAME_NET_SERVER_ONLY, "ObjectiveOwnership", 1, 0}};
const MimitaHotPackage::SystemRegistrar s_csSystem{
    {gameHash("counterstrike.tick"), kCsDomain, 0, 0, csTick, "counterstrike.tick"}};
const MimitaHotPackage::EventRegistrar s_csKilled{
    {kActorKilled, gameHash("actor.killed.v1"), kCsDomain, onActorKilled,
     "counterstrike.actor-killed"}};
const MimitaHotPackage::EventRegistrar s_csInteract{
    {kInteract, 0, kCsDomain, onInteract, "counterstrike.interact"}};

#endif
