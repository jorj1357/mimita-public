# Tick Rate Rule (Hard Rule)

All gameplay collision, damage, and physics MUST run at the fixed 60Hz tick rate, never per-frame. The game uses `constexpr double kClientFixedDt = 1.0 / 60.0` in `engine-tick-combat.cpp`. Running collision per-frame means:
- Low-FPS devices get fewer collision checks (unfair disadvantage)
- High-FPS devices get more collision checks (unfair advantage)
- Gameplay behavior varies by hardware

Weapon collision, NPC overlap detection, damage application, and knockback MUST use the tick accumulator pattern:
```cpp
const float tickDt = 1.0f / 60.0f;
const uint32_t ticksThisFrame = std::max(1u, (uint32_t)std::round(dt / tickDt));
for (uint32_t t = 0; t < ticksThisFrame; t++) {
    // collision + damage logic here
}
```

Never run collision logic in the raw render loop without tick quantization.

## Collision Broadphase Efficiency (Hard Rule)

Before modifying any collision code, VERIFY that broadphase triangle gathering uses
the **cached path** (`gatherGLBTriangles`) and NOT the **uncached path**
(`gatherGLBTrianglesForSphere`).

The cached version (`physics-collision-glb-setup.cpp`) checks `getCachedTriangles()`
and `getCachedSuperset()` before doing any work. The uncached version
(`physics-collision-glb.cpp`) does a full spatial hash search and heap-allocates a
new `std::vector<int>` on every call. With 6 body parts × 3 passes × 6 substeps,
the uncached path runs ~250 times per frame.

### BAD — uncached, allocates, searches every time

```cpp
// physics-collision-glb.cpp — AVOID THIS
std::vector<int> gatherGLBTrianglesForSphere(
    const World& world, glm::vec3 center, float radius,
    const glm::vec3& move, const char* caller)
{
    std::vector<int> out;  // heap alloc every call
    AABB sweepBounds;
    sweepBounds.min = glm::min(center, center + move) - glm::vec3(radius);
    sweepBounds.max = glm::max(center, center + move) + glm::vec3(radius);
    appendChunkTrianglesForAABB(world, sweepBounds, radius, out, ...); // full search
    return out;  // copy + dealloc
}
```

### GOOD — cached, no alloc, instant on cache hit

```cpp
// physics-collision-glb-setup.cpp — USE THIS
void gatherGLBTriangles(
    std::vector<int>& out,  // pass in a reusable buffer
    const World& world, const Capsule& cap,
    const glm::vec3& move, const char* caller)
{
    out.clear();
    AABB sweepBounds = makeSweptCapsuleAABB(cap, move);
    // 1. Cache hit → instant return
    if (getCachedTriangles(sweepBounds, currentFrame, out)) return;
    // 2. Superset hit → instant return
    if (getCachedSuperset(sweepBounds, currentFrame, out)) return;
    // 3. Only if both miss → actual search
    appendChunkTrianglesForAABB(world, sweepBounds, ..., out, ...);
}
```

### Rules

1. **Never call `gatherGLBTrianglesForSphere` in a per-entity or per-sphere loop.** Rewrite to use `gatherGLBTriangles` with a reusable `std::vector<int>` scratch buffer, or build a union AABB and do one `appendChunkTrianglesForAABB` call for the union region.
2. **Never return `std::vector` by value in the collision hot path.** Pass output references instead.
3. **Before adding new collision queries**, check if the existing cached gather covers the region. If the new shape is inside or near an existing capsule/sphere that was already gathered, reuse its candidates.
4. **If a function does >10 broadphase gathers per frame**, it is a candidate for caching or batch-gathering.
5. **Use `PhysicsScratch`** (`physics-scratch.h`) for temporary buffers in the collision hot path. It provides reusable `ints`, `contacts`, and `vec3s` vectors.

---

# Architecture Enforcement Rules

One concept = one owner.

One file = one responsibility.

One function = one job.

Before adding a state variable:

Search repository for existing equivalent concepts.

Before adding a new file:

Explain why an existing file cannot own the behavior.

Avoid duplicate sources of truth.

Collision contacts are the source of truth for grounded state.

Timers may not invent collisions.

---

