// 2026-11-27
/* purpose
* define the build-time and resource budgets the finished system must meet
* make the whole-game DLL fast, small, and predictable under concurrent agents
* replace "more jobs" tuning with measured, memory-aware work reduction
* this file DOES NOT change build authority or approval rules
* this file DOES NOT claim current builds meet these budgets
*/

# Build performance budget

**Status:** Desired behavior contract.

## Budgets

Measured from request to a runnable generation on this checkout. Values are
wall time on the development machine, not summed CPU time.

| Scenario | p50 | p90 | Hard ceiling |
|---|---:|---:|---:|
| One changed leaf `.cpp` in one hot module | 10 s | 20 s | 30 s |
| 15–20 changed files in one hot module | 20 s | 35 s | 60 s |
| Changed header with fan-out ≤ 50 | 25 s | 45 s | 75 s |
| Full cold whole-game link | 120 s | 180 s | 240 s |
| Publish + activate | 1 s | 2 s | 5 s |
| Test run start to first ready event | 5 s | 10 s | 20 s |

The headline goal is **sub-10-second reload for a changed hot module**, not
sub-10 seconds for one monolithic 237 MB DLL.

## Resource budgets

Per idle test client, at the default test frame cap:

```text
CPU      <= 0.25 logical core
memory   <= 350 MB working set
audio    muted unless explicitly enabled
```

Per whole-game build:

```text
host CPU mean   <= 70% of logical capacity
free memory     >= 2 GB at all times
compiler jobs   memory-aware, never "all cores"
compiler priority below normal; link normal
```

Four idle test clients plus one build MUST fit without paging.

## Build profiles

Three explicit profiles, each with its own object cache key:

| Profile | Optimization | Debug info | Use |
|---|---|---|---|
| `fast` | `-Og` | `-g0` | agent verification and test runs |
| `debug` | `-Og` | `-g1` | interactive debugging |
| `release` | `-O2` | `-g` | shipping |

`fast` MUST use:

```text
-ffunction-sections -fdata-sections
-Wl,--gc-sections
-Wl,--build-id=none
```

and MAY use `--strip-debug` for the runtime artifact while keeping an
unstripped copy beside it for symbols. The goal is a smaller link input and a
smaller output without losing journal line numbers (`__LINE__` is compile-time).

A profile change is a distinct cache identity. It must not silently invalidate
the existing cache; it creates a parallel one.

## Compiler cache (ccache)

The worker may prepend `ccache` to the compiler command. That is a second cache
layer on top of the content-addressed object cache, and it is allowed only
under a measured policy:

- Every build records `ccache` direct hits, misses, cache size, and the wall
  time spent in cache lookup/store.
- Keep `ccache` only when it produces a measured wall-time win over the object
  cache alone on this checkout.
- If the hit rate is low or the lookup/store time exceeds the saved compile
  time, disable it in the worker and remove the second cache layer.
- The decision and the numbers go in the postmortem.
- Agents still never invoke `ccache` directly. The root dev-loop worker is the
  only caller, and only under this policy.
- A cache directory change or a flags change is recorded as a cache identity
  event, not as a silent miss.

Acceptance for the policy: a build summary answers "did ccache help, by how
much, and why is it still enabled or disabled" without reading the worker
source.

## Memory-aware jobs

Job count is computed, not hardcoded:

```text
free = available_bytes
     - reserve(2.5 GB)
     - live_run_working_sets
     - other_active_build_working_sets

p95_tu_peak = per-path p95 from resource samples, else global p95
jobs = clamp(floor(free / p95_tu_peak), 2, min(logical_cores, 12))

if host_cpu_5s > 85%:
    jobs = max(2, jobs // 2)
```

Rules:

- No fixed `--jobs 10`.
- No `high` priority for compiles.
- The final link runs at normal priority so it finishes predictably.
- The estimate and the postmortem record the chosen jobs and the reason.

## Link budget

A link is a fixed per-generation tax. The finished system MUST keep it under
the budget by reducing inputs, not by accepting a 45-second link.

Required reductions, in order:

1. **Hot module split.** Link only the modules whose sources changed. Each
   module target is 30–60 MB and 1–3 s of lld time.
2. **Fast profile.** `-g0` and section GC reduce output size substantially.
3. **Identity wrappers.** The eleven module-identity wrappers MUST NOT rebuild
   on every generation. Runtime identity comes from host services; only
   standalone module DLLs keep compile-time identity.
4. **Object hygiene.** Remove stale objects and unknown-owner depfiles so the
   link input is the real input.

## Compile outliers

No single translation unit may exceed 10% of the full-build compile budget.

Current known outlier: `src/npc/recast-navigation.cpp` at 95–123 s. It MUST be
split into at least:

- the Recast/Detour build and configuration;
- the tile/mesh adoption;
- the query and path result adapter.

Each part must compile in a few seconds and keep the same public behavior.

## Header and fan-out budget

Covered in detail by
[`hot-modules-and-header-boundaries.md`](hot-modules-and-header-boundaries.md).
The build budget consumes those rules:

- a header edit reports its exact affected count;
- a header over budget cannot gain a dependent;
- PCH changes report that they invalidate every unit;
- generated identity headers do not fan out per generation.

## Estimate

Before a check or build, produce:

```text
changed files, affected TU count
compile p50/p90, worst TU
link p50/p90 and object count
estimate p50/p90
admission result
top three actions with seconds saved
```

The estimate learns per-path compile seconds from
`.dev/build-timings/*.json` and the resource samples. It is a forecast, not a
promise, and the postmortem records the error.

## Postmortem

Every build records:

- why;
- phase seconds: snapshot, compile, link, publish, activate;
- slowest TUs;
- per-TU queue wait;
- header fan-out;
- cache invalidations;
- host CPU mean and peak, minimum free memory;
- jobs chosen and why;
- suggestions ranked by measured seconds.

The largest measured cause MUST be acted on before the next build request.

## Acceptance

- A one-file leaf change in a hot module completes under 10 s p50 and 20 s p90.
- A 20-file batch in one module completes under 20 s p50 and 35 s p90.
- No translation unit exceeds 10% of the full-compile budget.
- A build never drives free memory below 2 GB.
- Four idle test clients consume less than one logical core total.
- The estimate is within 25% of measured p50 on the same machine.
- The postmortem identifies the largest cause and the next action.
