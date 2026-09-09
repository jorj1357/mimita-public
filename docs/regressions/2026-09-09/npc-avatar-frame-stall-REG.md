# NPC Avatar Frame Stall

Time created: 2026-09-09T11:51:43Z
Time last updated: 2026-09-09T11:51:43Z

Status: ATTEMPTED FIX (1)

Related specification:
`docs/specs/performance/performance.md`

Related changelog:
`docs/changelog/2026-09-09/20260909_075200_npc-avatar-background-queue.md`

---

## Regression Occurrence 1

### Observed

Time:
`2026-09-09T11:11:03Z`

### Expected Behavior

NPC avatar preparation must not block the gameplay/render frame. A missing or invalid avatar must use a fallback and must not retry every frame.

### Actual Behavior

The client repeatedly attempted to load `abusivegirlhead` during rendering. The requested path `assets/avatars/abusivegirlhead/avatar.json` was missing, and the client logged repeated cache misses and parse failures. A frame reached 141.4ms against a 16.667ms budget.

### Why This Is Bad

The game pauses while preparing an NPC avatar. Repeating the failed file lookup every frame wastes work indefinitely and can make networking/render responsiveness appear broken.

### Specification

`docs/specs/performance/performance.md`

Relevant requirement:

Performance work should be predictable, avoid blocking work in hot paths, and reduce frame time toward the fixed frame budget.

### Wrong Code

File:

`src/render/render-player.cpp`

```cpp
if (!p.avatarName().empty() &&
    (!p.avatarInstance || p.avatarInstance->name != p.avatarName()))
    av.applyAvatarToPlayer(p, p.avatarName());
```

`applyAvatarToPlayer()` called `getOrLoadAvatar()`, which synchronously parsed `avatar.json` and returned no cached failure state.

### Confirmed Cause

Avatar preparation was reachable from the per-player render path. Failed avatar loads did not enter a failed cache state, so the render condition remained true and retried the same missing/invalid file every frame.

Evidence:

- `Gameterminal_log_070954.txt` records repeated `AVATAR CACHE miss` and `failed to parse` messages.
- The same run records `FRAME TOTAL 141.4ms` at `70.174s`.
- `Performance_log_070954.txt` records a 141.408ms frame against a 16.667ms budget, with 136.762ms attributed to Networking.
- A valid avatar atlas build later took approximately 174ms between `317.699s` and `317.871s`.

### Attempted Fix 1

Time:
`2026-09-09T11:51:43Z`

Change:

Reused the existing `ReplaySaveWorker` below-normal worker by attaching it to `AvatarSystem`. Avatar JSON parsing and CPU atlas generation are now queued as one bounded background job. Results are polled once per engine frame, failed names are cached, and the render path skips names that are queued, loading, or failed.

Result:

The source builds successfully. Runtime frame-time proof and human visual acceptance are still required.

### Corrected Code

File:

`src/avatar/avatar-atlas.cpp`

```cpp
const bool accepted = mBackgroundWorker->enqueue([this, name, basePath, jsonPath]() {
    PendingAvatarLoad pending;
    pending.name = name;
    pending.definition.clear();
    pending.definition.name = name;
    pending.definition.basePath = basePath;
    pending.success = parseAvatarJson(jsonPath, basePath, pending.definition);
    if (pending.success)
        pending.atlasPixels = buildAtlasPixels(pending.definition, basePath);
    else
        pending.error = "avatar.json missing or invalid";
    std::lock_guard<std::mutex> lock(mPendingAvatarMutex);
    mPendingAvatarLoads.push_back(std::move(pending));
});
```

File:

`src/render/render-player.cpp`

```cpp
if (!p.avatarName().empty() && !av.isAvatarLoadPending(p.avatarName()) &&
    (!p.avatarInstance || p.avatarInstance->name != p.avatarName()))
    av.applyAvatarToPlayer(p, p.avatarName());
```

### Fix

The first implementation now separates CPU avatar preparation from the frame path, uses one shared low-priority worker, deduplicates work through avatar load states, caches failures, and limits result processing to one completed avatar per engine frame. OpenGL work remains on the main thread.

### Proof

Human review:

Pending. A human must spawn NPCs with valid and missing avatars and confirm fallback-to-avatar behavior without visible frame freezes.

Automated proof:

- `git diff --check` passed.
- `python -m py_compile build_agent.py` passed.
- `python build_agent.py` completed with `Status: SUCCESS`.
- The worker-backed avatar code and one-result-per-frame setup code compiled into the canonical `mimita.exe`.

The low-priority queue hypothesis is not yet proven or falsified because a fresh controlled runtime performance comparison has not been completed.

### Solution

Not confirmed. Keep status as `ATTEMPTED FIX (1)` until runtime measurements and human review are complete.
