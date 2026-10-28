// 2026-10-28T13:45:00Z
/* purpose
* preserve the human-confirmed animation/death frame-pacing investigation as a reusable gold reference
* record exact before/after measurements, code changes, runtime evidence, and the reasoning chain
* explain what the dev loop and logging did well, what wasted time, and how to make the next run far faster
* record the human/AI model workflow used on 2026-10-28
* this file DOES NOT claim the 240 FPS / 60-second target was achieved
* this file DOES NOT replace the performance specification, changelog, or regression record
* this file DOES NOT benchmark AI models; the model path is a human-reported workflow record
*/

# Gold reference: animation and death choppiness investigation

- Human confirmation: 2026-10-28 09:45 EDT
  (`2026-10-28T13:45:00Z`) — jorj:
  > “its smooth i ahvent had issues choppiness etc yet”
- Final build in this investigation: `3633`, build id
  `3633:42f2685b9f924fa7143ee55c0df6d78e9a8d600eddaaee88351772d1587e816c`
- Final journal:
  `logs/2026-10-28/20261028_132142/events.jsonl`
- Related concise note:
  [`20261028_093400-animation-and-death-choppiness.md`](20261028_093400-animation-and-death-choppiness.md)
- Related changelog:
  [`../changelog/2026-10-28/20261028_041903-plan-a-gameplay-diagnostics.md`](../changelog/2026-10-28/20261028_041903-plan-a-gameplay-diagnostics.md)
- Related regression:
  [`../regressions/2026-10-28/animation-death-frame-choppiness-REG.md`](../regressions/2026-10-28/animation-death-frame-choppiness-REG.md)
- Specification:
  [`../specs/performance/performance.md`](../specs/performance/performance.md)

## What the human experienced

The reported symptom was not a low average-FPS number. It was a felt
choppiness that got worse as actors animated, killed each other, and produced
corpses. The explicit engineering target was 240 FPS for 60 seconds in a busy
Juggernaut or TDM scene, which means a 4.17 ms per-frame budget.

The investigation did **not** reach that numerical target. The human did report
that the result was smooth in play at 09:45 EDT and had not shown the previous
choppiness yet. Those are separate claims:

- measured runtime evidence: large owner-level costs were removed;
- human acceptance: ordinary play felt smooth in the reviewed session;
- target acceptance: 240 FPS for 60 seconds was not proven.

That distinction is important. A successful build and a smooth human session do
not prove a sustained 4.17 ms frame budget.

## Model and human workflow used

Recorded as a workflow note, not a controlled model benchmark:

- Worker/executor model: **DeepSeek V4.1 Flash on high**, through opencode,
  as of 2026-10-28 09:47.
- Planner model: sometimes **GPT-6 Luna on medium**.
- Human: jorj, who supplied the live symptom, selected the scenarios, ran the
  game, and gave the final visual acceptance.

The useful loop was:

```text
human notices/describes symptom
  -> planner explains what kind of problem it is and proposes a plan
  -> worker restates and sharpens the plan against the actual source
  -> worker instruments, builds, runs, measures, and edits
  -> human plays and confirms or rejects the visible result
```

The planner was most useful for separating “what is wrong?” from “what do we
change?”. The worker was most useful for holding the measurement loop: read
source, add a scope, build a new executable, run a real scenario, compare
numbers, and make the smallest owner-correct change.

The workflow risk is handoff loss. A planner can produce a plausible plan from
an old build, a different mode, or a stale journal. The worker must re-check the
active build, launch mode, actor count, source generation, and `events_path`
before trusting the plan. The worker should also return a compact handoff with
exact paths and numbers, not just “it is faster now”.

A better planner handoff would be structured:

```text
symptom:
target:
exact build/run:
scenario and actor count:
current measured owner:
files already inspected:
unknowns:
acceptance test:
human confirmation still needed:
```

## The first useful evidence

The measurement surface was a 200 ms `performance.frame-slice` plus the
60-frame `performance.resource-window`. The most useful fields were not total
FPS. They were:

- `frame_p95_ms`, `frame_p99_ms`, `frame_max_ms`;
- `EngineTick::Work` total and per-frame average;
- top inclusive/self scopes;
- top allocation scopes;
- `performance.session-growth` memory and allocation deltas;
- actor/effect/corpse/animation counts;
- the exact `run_id`, build id, executable path, and `events_path`.

The first decisive run was build 3596, run
`logs/2026-10-28/20261028_120254/events.jsonl`:

| Metric | Value |
|---|---:|
| p95 | 10.088 ms |
| p99 | 26.649 ms |
| max simulation | 31.747 ms |
| `Network::SnapshotApply` join spike | ~108 ms |
| `Combat::HitResolve` recurring spikes | 16–26 ms |
| `RagdollCorpseUpdate` spikes | up to ~27 ms |

The key reasoning step was to stop saying “animations are slow” and start
asking “which owner runs inside the frame, for every actor, at what rate?”.
The scopes showed work that was too large to be animation interpolation alone:
file scanning, per-tick allocation, diagnostic serialization, and music decode
were all inside the same felt symptom.

## Confirmed causes

### 1. Animation sampling allocated every actor every tick

`interpolateAnimClip` returned a fresh
`std::unordered_map<std::string, AnimOverlayResult>` and built a
`std::vector<std::string>` union of body-part names on every call. In a
32-avatar Juggernaut scene this ran for each actor on each animation update.

Before:

```cpp
static std::unordered_map<std::string, AnimOverlayResult> interpolateAnimClip(
    const AnimClip& clip, float timeSeconds, float speedScale = 1.0f,
    float forcedLoopedTick = -1.0f, float forcedSampleDurationTicks = -1.0f)
{
    std::unordered_map<std::string, AnimOverlayResult> result;
    if (clip.keyframes.empty()) return result;

    // ...
    std::vector<std::string> allParts;
    for (const auto& p : prev->parts) allParts.push_back(p.first);
    for (const auto& p : next->parts) {
        bool found = false;
        for (const auto& existing : allParts) {
            if (existing == p.first) { found = true; break; }
        }
        if (!found) allParts.push_back(p.first);
    }

    for (const auto& partName : allParts) {
        // ...
        result[partName] = overlay;
    }
    return result;
}
```

After:

```cpp
struct AnimOverlayBuffer {
    static constexpr int kMaxParts = 64;
    struct Entry {
        const std::string* name = nullptr;
        AnimOverlayResult value;
    };
    Entry entries[kMaxParts];
    int count = 0;

    const AnimOverlayResult* find(const std::string& name) const;
    void add(const std::string& name, const AnimOverlayResult& value);
};

static void interpolateAnimClip(
    const AnimClip& clip, float timeSeconds, AnimOverlayBuffer& result,
    float speedScale = 1.0f,
    float forcedLoopedTick = -1.0f, float forcedSampleDurationTicks = -1.0f)
{
    if (clip.keyframes.empty()) return;

    // No map, no vector<string>, no per-part heap node.
    // Names point into the clip's keyframe maps.
    for (const auto& p : prev->parts)
        addPart(p.first);
    for (const auto& p : next->parts) {
        if (prev->parts.find(p.first) == prev->parts.end())
            addPart(p.first);
    }
}
```

The selector also built a JSON candidate array even with priority debugging
off. Before:

```cpp
nlohmann::json candidateJson = nlohmann::json::array();
// ...
candidateJson.push_back({
    {"state", state}, {"clip", clip},
    {"priority", priority}, {"eligible", eligible},
    {"clip_found", c != nullptr}
});
```

After:

```cpp
nlohmann::json candidateJson;
if (gAnimationPriorityDebug)
    candidateJson = nlohmann::json::array();

// ...
if (gAnimationPriorityDebug) {
    candidateJson.push_back({
        {"state", state}, {"clip", clip},
        {"priority", priority}, {"eligible", eligible},
        {"clip_found", c != nullptr}
    });
}
```

The measured effect was visible in `PlayerAnimation`. In an earlier Juggernaut
window this scope reached about 32 ms for 84 actor updates (about 0.38 ms per
actor). After the allocation and JSON fixes, build 3633's steady window showed
about **0.86 ms of `PlayerAnimation` per rendered frame across 2,654 actor
updates**, or roughly **0.02–0.05 ms per actor update** depending on the
window.

### 2. File polling ran on the gameplay thread

Avatar animation reload stat-ed every dependency of every loaded avatar every
100 ms. Weapon reload separately scanned `config/weapons` and the 115-file
animation library every 250 ms. `WeaponData` and `WeaponConfig` duplicated part
of that scan.

That is why a frame could show a large unscoped `Simulation::SimulateTick` cost
or a `Combat::WeaponReloadPoll` cost that did not correlate with actor count.
One measured `Combat::WeaponReloadPoll` spike was **43.19 ms**.

Before:

```cpp
void AvatarAnimationRegistry::pollReload(std::uint32_t activationTick)
{
    // ...
    for (auto& [name, entry] : mEntries) {
        bool changed = !entry.loaded ||
            fileWriteStamp(... + "/avatar.json") != entry.avatarWrite ||
            fileWriteStamp(entry.animPath) != entry.animWrite;
        for (const auto& [path, stamp] : entry.dependencyWrites)
            changed = changed || fileWriteStamp(path) != stamp;
        // ...
    }
}
```

After, the main thread only consumes a background dirty flag:

```cpp
class JsonFileWatch {
public:
    JsonFileWatch(std::vector<std::string> roots,
                  std::chrono::milliseconds interval);
    bool consumeDirty();
    void markDirty();
private:
    void run();
    std::atomic<bool> mDirty{true};
    std::atomic<bool> mStop{false};
    std::thread mThread;
};
```

```cpp
// background thread
Stamps last = scan();
while (!mStop.load(std::memory_order_acquire)) {
    std::this_thread::sleep_for(mInterval);
    Stamps next = scan();
    if (next != last) {
        last = std::move(next);
        mDirty.store(true, std::memory_order_release);
    }
}
```

The same owner now feeds avatar animation and weapon JSON reloads. The duplicate
`WeaponConfig::pollHotReload()` call was removed from the viewmodel update; the
shared weapon-definition scan reloads both runtime definitions and viewmodel
config.

Before, both owners could run the same signature scan:

```cpp
// WeaponSystem::update
if (WeaponData::reloadBuiltinWeaponsIfChanged())
    Terminal::instance().addLog("[WEAPON] Reloaded weapon definitions");

// WeaponViewModel::update
WeaponConfig& wc = WeaponConfig::instance();
wc.pollHotReload(); // scans weaponDefinitionSignature() again
```

After, one result updates both owners:

```cpp
if (WeaponData::reloadBuiltinWeaponsIfChanged()) {
    WeaponConfig::instance().reloadNow();
    Terminal::instance().addLog("[WEAPON] Reloaded weapon definitions");
}
```

This removed a measured `Combat::WeaponReloadPoll` spike of **43.19 ms** from
the inspected runs.

### 3. Walk/run state flapped at one threshold

The old transition was a single 12 m/s comparison:

```cpp
if (moving)
    return horizontalSpeed > 12.0f ? AnimationMovementState::Run
                                   : AnimationMovementState::Walk;
```

An actor moving near 12 m/s could select walk, then run, then walk on
consecutive ticks. Each transition restarted the clip at tick 0. That is both a
visual stutter and a logging/allocation amplifier.

One Juggernaut run logged **75,689 `anim.state.changed` records in about two
minutes**. The dominant pairs were walk↔sprint, each around 15,000 transitions.

After:

```cpp
if (moving) {
    constexpr float kRunEnterSpeed = 12.0f;
    constexpr float kRunExitSpeed = 10.0f;
    return (horizontalSpeed > kRunEnterSpeed ||
            (preferRun && horizontalSpeed > kRunExitSpeed))
        ? AnimationMovementState::Run
        : AnimationMovementState::Walk;
}
```

`preferRun` is true when the actor is currently using the configured run clip.
The hysteresis band is 12 m/s to enter run and 10 m/s to leave run.

The diagnostic was also rate-limited per actor to one record per 250 ms, with a
`suppressed_since_last` count. In the comparable 32-avatar Juggernaut run,
`anim.state.changed` fell from **75,689 to 1,244**.

That change mattered twice:

1. fewer clip restarts and fewer visual transitions;
2. far less JSON construction, logging, disk I/O, and journal size.

The journal for the run that produced 75,689 records was about 75 MB. The
comparable later run was about 28 MB.

### 4. Corpses ran an alive-ragdoll solver budget

`stepBody` is used only for corpses, but it used the same 24 joint iterations,
two self-collision iterations, and two world-depenetration passes as the
interactive alive ragdoll. With six simulated corpses, that work landed inside
one fixed tick.

Before:

```cpp
solveJoints(cfg.solverIterations, true, b);          // 24
if (cfg.selfCollision) {
    for (int i = 0; i < cfg.selfCollisionIterations; ++i) // 2
        selfCollision(b);
}
if (cfg.worldCollision) {
    for (auto& part : b.parts)
        depenetrateWorld(part.body, world, 2);
}
```

After, corpse presentation gets its own budget:

```json
{
  "blood_interval_seconds": 0.12,
  "blood_enabled": true,
  "solver_iterations": 10,
  "self_collision_iterations": 1,
  "world_depenetration_iterations": 1,
  "max_steps_per_tick": 6
}
```

```cpp
solveJoints(cfg.corpseSolverIterations, true, b);     // 10
solveJoints(cfg.corpseSolverIterations / 2, true, b); // 5
if (cfg.selfCollision) {
    for (int i = 0; i < cfg.corpseSelfCollisionIterations; ++i) // 1
        selfCollision(b);
}
if (cfg.worldCollision && cfg.corpseWorldDepenetrationIterations > 0) {
    for (auto& part : b.parts)
        depenetrateWorld(part.body, world,
                         cfg.corpseWorldDepenetrationIterations); // 1
}
```

High-volume corpse diagnostics were also moved from `Important` to `Trace` and
guarded by `shouldLog(Ragdoll, Trace)`:

- `ragdoll.corpse.position_sample`
- `ragdoll.corpse.heartbeat`
- `ragdoll.corpse.pose_step_trace`
- `ragdoll.corpse.pose_render_trace`

Spawn, rejection, removal, and first-render records remain useful lifecycle
evidence.

Measured before: `Ragdoll::StepBody` reached **5.99 ms** in a 32-avatar
Juggernaut run; `RagdollCorpseUpdate` reached **6.35 ms**. After the budget and
trace changes, steady windows showed about **0.55 ms per frame** for corpse
step, with the observed step peak around **2.06 ms** in the 60-frame resource
window. That is the difference between “death freezes the frame” and “death
costs a fraction of a 4.17 ms budget”.

### 5. Opus music decoding could launch FFmpeg on the gameplay thread

Before, `resolvePlaybackPath` ran FFmpeg through `std::system` whenever an Opus
track was not already in the WAV cache. A track change produced a measured
**701.9 ms `Audio` scope**.

After, a background predecode thread converts Opus tracks to the cache. The
main thread only resolves an existing cache file:

```cpp
std::string MusicManager::resolvePlaybackPath(const std::string& path)
{
    if (!isOpusPath(path))
        return path;

    // Never spawn FFmpeg on the gameplay thread.
    const std::string output = opusCachePath(path);
    if (output.empty() || !std::filesystem::exists(output))
        return {};
    return output;
}
```

If the cache is not ready, playback retries once per second. The 701.9 ms
spike was absent in the later run.

## Before/after summary

These are not a single controlled benchmark. They are the same mode and actor
class measured in successive builds while other networking work was also
changing in the same tree.

| Measurement | Before | After |
|---|---:|---:|
| Best busy TDM p95 (14 avatars, build 3626) | ~10.1 ms (earlier run) | 5.305 ms |
| Best busy TDM p99 (14 avatars, build 3626) | ~26.6 ms (earlier run) | 6.342 ms |
| Juggernaut 32-avatar p95 (build 3627) | — | 9.283 ms |
| Juggernaut 32-avatar p99 (build 3627) | — | 12.157 ms |
| `anim.state.changed` in ~2 min | 75,689 | 1,244 |
| `PlayerAnimation` per actor update | ~0.38 ms | ~0.02–0.05 ms |
| `Ragdoll::StepBody` peak | 5.99–6.35 ms | ~2.06 ms window peak |
| Corpse step per frame | — | ~0.55 ms |
| Music `Audio` spike | 701.9 ms | not observed |
| `Combat::WeaponReloadPoll` spike | 43.19 ms | not observed after background watch |
| Build 3633 steady window average | — | 5.5–8.8 ms |
| Build 3633 steady window p95 | — | 8–16 ms |

The 240 FPS target requires **4.17 ms**, so 5.5–8.8 ms average and 8–16 ms p95
are still over budget. The human-reported smoothness is real and useful, but it
is not the same as a 240 FPS acceptance result.

## What remained expensive in the final run

Final Juggernaut build 3633, 32 avatars, run
`20261028_132142`:

- `Network::SnapshotApply::NpcLifeReset` / `NpcAvatarApply` reached
  **159–178 ms** during snapshot/death bursts. This is Plan B networking work,
  not animation or ragdoll math.
- First-time avatar/animation-set binding reached about **385 ms** in one frame.
- 32-actor rendering averaged about **6.85 ms/frame**:
  - network entities: ~2.68 ms;
  - effects: ~1.77 ms;
  - ragdoll corpses: ~0.96 ms;
  - player body: ~1.14 ms;
  - shadows/world: ~0.34 ms each.
- `Audio::StartSound` reached about **29.5 ms** during a mass death.
- A first-use animation set load can still be a one-frame hitch; this is a good
  next candidate for preload or per-tick load budgeting.

## Dev loop review

### What was helpful

- `--request build --wait-for-build` gave an exact result, for example
  `result=success build=3633`.
- `--status --json` exposed `running_build`, `latest_successful_build`,
  `source_generation`, `source_current`, `builds_match`, `launch_mode`,
  `events_path`, and process identity. Without those fields it is easy to read
  the wrong journal.
- `--command` delivered a real input and returned a completion with a tick:
  `command="npc_void" delivered=True completed=True event=terminal.command.completed tick=3803`.
- `--versioninfo` proved executable path, PID, process role, run ID, and
  `EVENTS_JSONL_PATH`.
- The build identity hash and server/client build match prevented a stale
  executable claim.
- The dev loop kept the game, build, journal, and version probe in one place.
  That is much better than manually launching an unknown `mimita.exe`.

### What was not helpful

- **Concurrent source edits.** Another agent was editing Plan B networking
  files and launching different modes in the same working tree. The dev loop
  correctly rebuilt on source changes, but a run could be replaced or
  contaminated while being measured. `source_generation` moved from 232 to 82
  across daemon restarts, which is confusing.
- **No benchmark reservation.** There was no way to say “hold this dev loop and
  this source tree for the next 90 seconds”. The mode changed from 42 to 40 to
  31 to 9 during the session.
- **No exact busy-scene profile.** The `npc-navigation` profile still produced
  only two animation sets in TDM mode 31/41. The 32-avatar Juggernaut result
  required mode 9. A `perf` profile with explicit actor count and mode should
  exist.
- **No deterministic death scenario.** `npc_void` killed actors, but it was
  discovered late and did not produce a repeatable timeline. A scenario should
  specify when deaths occur and which actors die.
- **Metric inconsistency.** `performance.frame-slice` reported `corpses=0`
  even when 68 corpse spawns and heartbeat records existed. That made the
  corpse cost look absent in the slice summary.
- **Scope truncation.** Slices expose only the top 10 scopes. A nested
  `Ragdoll::StepBody` or a specific config poll can be missing, forcing a
  second run and more scopes.
- **Relative event time.** `t` in the journal is relative to process start,
  while the human review time is local/UTC. Correlating “around 09:45” to a
  journal window required manual conversion.
- **No automatic pass/fail.** `perf-evidence.py` summarizes well, but the dev
  loop does not run it against a threshold and fail the run.

## Logging review

### What was good

- `performance.frame-slice` at 200 ms was the right resolution. A 1-second
  average hid the hitches; 200 ms showed them.
- `performance.session-growth` exposed retained memory and allocation growth
  separately from frame time.
- Allocation scopes were essential. `EngineTick::Work` allocation bytes and
  count made the animation sampling allocation visible as a pattern.
- Named scopes made the first divergence concrete: `Combat::WeaponReloadPoll`,
  `Simulation::SimulateTick`, `RagdollCorpseUpdate`, `Audio::MusicStartTrack`.
- Structured identity (`run_id`, build id, process role, executable,
  `events_path`) made the evidence auditable.
- `--versioninfo` plus the canonical `events.jsonl` is a strong pattern that
  should remain mandatory.

### What was bad

- High-volume diagnostics used `Important`, so they were always constructed and
  written. `anim.state.changed` reached 75,689 records. Ragdoll heartbeats and
  pose traces were also `Important` even though they are trace evidence.
- JSON was often built before the logger decided whether the level was enabled.
  The cost is paid even when nobody will read the record.
- Scopes overlapped and were summarized as a flat top 10, so “total ms” was easy
  to double-count and nested owners disappeared.
- No event field carried the benchmark phase (`spawn`, `steady`, `death`,
  `post-death`) or exact actor/corpse count, so comparisons relied on external
  inference.
- A per-slice `corpses` counter could disagree with actual corpse lifecycle
  records.
- There was no standard event for “this frame is a benchmark sample”; the
  human had to know which journal and which time range mattered.
- The amount of diagnostic output made the journal itself part of the
  performance problem.

### What should be better

- Diagnostic events should default to `Trace` or `Verbose`; only lifecycle and
  failure records should be `Important`.
- Add `phase`, `mode`, `actors`, `corpses`, `effects`, and `build_id` to every
  performance slice.
- Emit a full ordered scope tree or at least top 30 with inclusive/self time,
  not only top 10.
- Add a `performance.benchmark` record with scenario name, seed, duration,
  threshold, and pass/fail.
- Make corpse/actor/effect counts come from one owner and assert consistency.
- Add a UTC wall-clock field alongside relative `t`.
- Rate-limit every repeated diagnostic by actor and transition, with a
  suppressed count.

## What made the work take longer than necessary

1. **Finding a busy scene was harder than finding the bug.** The first TDM
   runs had only 2 animation sets because the active profile supplied one NPC.
   The 32-avatar Juggernaut scene was only found after trying mode 9. A
   `perf` profile with a fixed actor count would have saved many launches.
2. **Concurrent edits made every measurement provisional.** A build could be
   stale or replaced by another agent. A dedicated worktree or dev-loop lease
   would remove this entire class of wasted time.
3. **The first profiling surface was too coarse.** We had to add scopes after
   seeing unexplained 18–31 ms simulation time. A complete scope tree or
   automatic owner attribution would have shortened the first discovery.
4. **Top-10 truncation caused a second investigation.** The unscoped cost was
   not visible until more scopes were added. Full-scope export would have made
   the first run conclusive.
5. **The corpse metric lied.** `corpses=0` in the slice did not mean no corpses;
   it meant the slice counter was not populated the same way as the lifecycle
   records. That cost time and reduced trust in the dashboard.
6. **Death injection was manual and late.** `npc_void` was useful, but the
   scenario was not repeatable. A scripted “kill 18 actors at tick N” would have
   produced comparable before/after windows immediately.
7. **Build/launch/parse was a manual three-step loop.** The state and journal
   are already machine-readable; the loop should be one command.
8. **The 240 FPS threshold was not enforced anywhere.** The human had to ask
   “is this smooth?” because no automated test answered “did 60 seconds stay
   under 4.17 ms?”.

## How to make the next run 100x faster

The biggest win is not a faster compiler. It is an automated benchmark owner.

Proposed `perf.run` workflow:

```text
perf.run <scenario> --build <id> --duration 60 --json
```

It should:

1. reserve the dev loop and prevent another launch mode from replacing it;
2. launch the requested mode and wait for the expected actor count;
3. run a deterministic scenario (actor count, weapons, deaths at fixed ticks,
   camera path, seed);
4. record the exact build id, executable, process role, `events_path`, and
   start/end UTC times;
5. sample frame time, per-owner scopes, allocation, memory, thread CPU, and
   GPU/frame-pacing data;
6. annotate the journal with `spawn`, `steady`, `death`, and `post-death`
   phases;
7. compute p50/p95/p99/max and the fraction of frames over 4.17 ms;
8. compare against the previous accepted baseline;
9. fail with a short report if the threshold or regression margin is missed;
10. keep human visual acceptance as a separate required record.

For the 240 FPS goal specifically:

```text
Acceptance:
  duration >= 60 s
  frames_over_4.17_ms < 1%
  p99 <= 4.17 ms
  no single frame > 16.67 ms after spawn
  release and debug reported separately
```

The existing 200 ms slice and `perf-evidence.py` are good raw material for this.
They should become the measurement engine behind a pass/fail command rather than
a manual post-processing step.

What could make the next iteration worse:

- adding many always-on scopes and `Important` diagnostics without a cost
  budget;
- hiding actors or effects with aggressive culling instead of fixing the
  measured cost;
- moving simulation to worker queues without proving determinism and ordering;
- trusting average FPS or a single smooth human session as “240 FPS achieved”;
- adding a second animation/ragdoll/audio path to avoid the cost instead of
  fixing the existing owner.

Other high-value automation:

- A `perf.scenario` JSON format with `mode`, `map`, `actor_count`, `weapons`,
  `deaths`, `duration`, and `seed`.
- A `perf.compare` command that shows before/after owner deltas, not just total
  frame time.
- A build metadata block embedded in every performance event.
- A `scope_tree` emission with both inclusive and self time.
- An automatic “first expensive owner” report that follows the scope tree to a
  source file and function.
- A release-build benchmark lane, because debug-build frame time is not a
  shipping result.
- A GPU/thread-CPU lane so CPU-only scopes do not get blamed for a GPU-bound
  frame.
- A journal-size guard, because diagnostics that describe performance can also
  cause it.

## What I would do differently

1. **Reserve the workspace and source generation first.** Freeze the dev-loop
   mode and prevent overlapping agents from changing the tree during a measured
   run.
2. **Create the busy scenario before adding scopes.** Know the exact mode,
   actor count, and death schedule. A benchmark without a stable scenario is
   only an anecdote.
3. **Instrument the full owner tree once.** Add phase and actor/corpse counts at
   the same time as the scopes, not in a later pass.
4. **Move all filesystem watching off the gameplay thread up front.** A
   background dirty flag is simple and removes a whole class of unexplained
   frame spikes.
5. **Remove per-tick allocation and eager JSON before tuning numbers.** These
   are structural wins; they also make the remaining measurements trustworthy.
6. **Run one release benchmark early.** If the target is 240 FPS, a debug-only
   result can send the investigation in the wrong direction.
7. **Treat diagnostics as production load.** Every repeated `Important` event
   must justify its cost.
8. **Keep human confirmation separate from target acceptance.** “It feels
   smooth” and “p99 is under 4.17 ms for 60 seconds” are different claims.

## Validation and evidence quality

The strongest part of this session was not the final patch. It was the evidence
chain around the patch:

1. A real executable and exact build identity.
2. A real launch mode, map, actor count, and journal.
3. Before/after runs with the same owner scopes.
4. Code-level confirmation of the owner.
5. A human observation recorded separately from the engineering claim.
6. An explicit statement of what remained unproven.

That is much better than “the build passed” or “it felt faster”. It is also the
right foundation for expanding AI auto-testing: the AI already has a canonical
journal, build identity, command path, and parsable timing owner. The missing
piece is a deterministic benchmark command that turns those assets into a
pass/fail result.

Recommended expansion:

- make a 60-second busy-scene run a first-class AI test, not an ad hoc manual
  session;
- assert the target threshold and a regression margin;
- keep debug and release results separate;
- require the AI to attach the exact `events_path` and build id to every claim;
- require human acceptance as a separate field, never inferred from logs;
- preserve an accepted baseline so future AI sessions can compare rather than
  re-litigate.

Over time this can become the normal loop for 240 FPS, corpse cost, animation
cost, load time, and memory growth. The goal is not to let the AI declare
success. The goal is to make it produce evidence strong enough that a human can
accept or reject the result quickly.

This investigation is strong evidence for the causes it removed:

- source-level traces into the owning functions;
- before/after code in the same files;
- multiple live builds and journals;
- build identity, source generation, and server/client build match;
- exact process roles and `events_path`;
- comparable 32-avatar Juggernaut runs;
- quantitative scope and record-count changes;
- a direct human confirmation at 09:45 EDT.

It is not proof of full performance acceptance:

- the 240 FPS / 60-second threshold was not met;
- the reviewed human session is qualitative, not a frame-time capture;
- the runs were debug builds on one machine;
- other networking work was changing in the same tree;
- GPU/frame-pacing data was not the primary measurement;
- the final run still had large snapshot/NPC reset and render costs.

That is the correct evidence boundary. The work removed confirmed owner-level
costs and produced a better human experience. It did not prove the full target.

## Permanent lesson

Performance work should be organized around repetition:

```text
cost per operation
  x operations per actor
  x actors
  x ticks per second
  x render frames per second
  + diagnostics that describe the work
```

A tiny allocation, JSON object, file stat, or state transition becomes a major
frame cost when it runs for every actor on every fixed tick. The correct
response is to measure the owner, move non-simulation work off the gameplay
thread, use bounded storage, add hysteresis to threshold-driven state, and keep
diagnostics bounded by default.

The next step is not “optimize animations more” in the abstract. It is to make
the benchmark automatic, deterministic, and threshold-aware so an AI or human
can prove whether a change achieved 240 FPS for 60 seconds, rather than
guessing from a smooth play session.
