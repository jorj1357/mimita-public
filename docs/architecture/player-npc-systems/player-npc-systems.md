# Player and NPC Philosophy

---

Accessibility should be considered during implementation, not added later.

* low frame rates
* low-end hardware
* visual impairments
* limited mobility
* controller use
* keyboard-only use

Consider:

Design for the widest possible audience.

# Accessibility

---

5. Never use printf
4. Use Debug::logThrottled for per-frame checks
3. Use Debug::log for detailed diagnostics (gated by debug flags)
2. Use Debug::warn for important state transitions (always visible)
1. Pick the right category (add a new one if needed)

When adding a new log:

Console output should maximize signal and minimize noise.

If a message repeats identically more than once per second, it must use logThrottled.

Spam is forbidden.

improvement.
rate-limited and useful; adding duplicate spam does not count as an
why the existing diagnostics are sufficient. Repeated paths must remain
agent must improve the relevant diagnostics or explicitly verify and document
debug category. On every AI-agent task that changes repository behavior, the
reason, and must use the owning subsystem's log file as well as the centralized
must identify the important input, decision, output, and rejection/failure
owner or entry point, directly beside the code it explains. The diagnostic
Every new feature must add or improve an adjacent debug logging hook at its

## Feature Diagnostic Requirement

* Include enough context to diagnose issues without recompiling
* Explain expected state, actual state, and why the engine made a decision
* Support throttling — never print identical messages every frame
* Use a specific category (Auth, Gui, Weapons, etc.)

Every log must:

Always use the centralized Debug::log / Debug::warn / Debug::logThrottled system.

NEVER use printf for debug logging.

## Logging Rules

---

sixty identical logs per second

instead of

one useful summary per second

Prefer:

* summary logging
* aggregated logging
* rate-limited logging

Support:

Avoid log spam.

## Rate Limiting

---

ragdoll_debug 1

npc_damage_debug 1

ui_debug 1

Example:

Categories should be independently enabled and disabled.

Ragdoll
Duel
World
Geometry
Animation
Audio
Networking
Replay
Collision
Physics
NpcCombat
Combat
Render
UI

Examples:

## Debug Categories

---

Avoid scattered printf() usage.

Debug::log(category, ...)

Prefer:

Use centralized logging.

* what outputs it produced
* what inputs it received
* why it is doing it
* what it is doing

Every system should be able to explain:

Visibility is preferred over guessing.

Debug everything.

# Debugging

---

UI
Debug
Network
Duel
NPC
Weapon
Replay

Examples:

Commands should be grouped by feature.

help_recent
help2
help

Support:

* dateAdded
* category
* usage
* description
* name

Commands should store metadata:

## Command Metadata

---

void registerReplayCommands();

src/replay/replay-commands.cpp

Example:

Feature systems own their own commands.

registerDebugCommands();
registerDuelCommands();
registerNpcCommands();
registerWeaponCommands();
registerReplayCommands();

Preferred:

Move command registration out of main.cpp.

## Command Registration

---

Avoid duplicate implementations.

UI, hotkeys, AI, networking, and gameplay should reuse shared actions where practical.

joinserver
hostserver
savedreplay
spawnnpc
reload
shoot

Examples:

The game should be playable and testable entirely through terminal commands.

Every gameplay action should eventually be callable through terminal commands.

# Terminal Commands

---

Keep interfaces small and obvious.

fireWeapon()
Weapon:

doMovement()
Movement:

doGravity()
Gravity:

loadReplay()
saveReplay()
Replay:

Example:

Subsystems should expose small APIs.

## Public APIs

---

Move ownership into subsystems.

* duel implementation
* npc implementation
* weapon implementation
* replay implementation

Not:

* shutdown
* update loop
* registration
* initialization

main.cpp should contain:

Example:

They should not own feature logic.

Main files should orchestrate.

## Main Files

---

should immediately reveal the owning subsystem.

duel
replay
weapon
npc

Searching for:

Feature logic spread across many unrelated files.

Bad:

src/ui/
src/network/
src/weapon/
src/npc/
src/duel/
src/replay/

Good:

Every feature should have a clear owner.

## Ownership

---

Avoid giant files that own many unrelated systems.

spawnBlood()
spawn-blood.cpp

sendPacket()
send-packet.cpp

doMovement()
movement.cpp

doGravity()
gravity.cpp

Examples:

* one obvious owner
* one responsibility
* under 100 lines

Ideal:

Prefer small files.

## Small Files

Target architecture. Use judgment.

# Architecture Direction

---

If a simpler implementation achieves the same result, prefer the simpler implementation.

Avoid unnecessary allocations, copies, complexity, and hidden work.

* Low-memory devices
* Small phones
* Old hardware
* Weak laptops
* Integrated graphics
* Low-end PCs

Target hardware includes:

Performance is the highest priority.

5. Extensibility
4. Debuggability
3. Readability
2. Simplicity
1. Performance

Optimize for:

# Core Philosophy

---

4. Never delete the build lock or run a second build to "speed things up." The lock serializes builds; the first build wins. If your changes landed after the running build compiled, wait for it to finish, then run your own build to compile the remainder.

3. **Only run a new `python build_agent.py` if no build is running** (lock file missing, or its owner PID is gone).

   - Only fix YOUR OWN errors from that shared log. Do not fix errors from other agents' files unless you own that code.
   - Check that your scope's object files under `build/obj-debug/` / `build/obj-release/` are newer than your edited sources (confirming your changes got compiled), and that `mimita.exe` was relinked after them.
   - Read `build/changelog.txt` (written after every run; first three lines show `Status: SUCCESS|NOTHING_CHANGED|FAILED`).
2. **If a build is already running — DO NOT start another one.** Use that running build's log to verify your own changes:

   If the file exists, look up the owner PID with `Get-Process -Id <pid>`. If that process is alive, a build is in progress.
   ```
   Get-Content "build\build-agent.lock" -ErrorAction SilentlyContinue | ConvertFrom-Json
   ```powershell
1. **Before building, always check if a build is already running:**

Multiple people/agents share one machine and one `build_agent.py` build lock. To keep agents from interfering:

# Single Shared Build (Multiple Agents)

Always check this status after building. If the human built between your source edits and your build_agent.py call, you will see NOTHING_CHANGED even though your edits should trigger a rebuild. Delete mimita.exe and rebuild in that case.

```
Status: SUCCESS|NOTHING_CHANGED|FAILED
Time: YYYY-MM-DD HH:MM:SS
=== BUILD CHANGELOG ===
```

The changelog at `build/changelog.txt` is written after every `build_agent.py` invocation. Its first three lines always show:

```
Remove-Item -Force "mimita.exe" -ErrorAction SilentlyContinue; python build_agent.py
```powershell
If you get NOTHING_CHANGED but changed source files, force a rebuild by deleting the EXE:

If you get NOTHING_CHANGED but changed source files, the human may have built first. Read `build/changelog.txt` for the full build log. The changelog always reflects the most recent build_agent.py run.

If the status is NOTHING_CHANGED and you expected changes, the human may have built first. Read `build/changelog.txt` for the full build log. The changelog always reflects the most recent build_agent.py run.

```
Status: NOTHING_CHANGED
```

or:

```
Status: SUCCESS
=== BUILD CHANGELOG ===
```

After any build_agent.py invocation, check the build result status printed in the output:

`mimita.exe`; never kill a possibly user-owned process to release the file.
`mimita.exe` or a non-EXE test harness. Before building, close any running
or feature-specific test executables. Focused tests must use the canonical
executables such as `mimita-chat-test.exe`, `mimita-duel-handshake-test.exe`,
Do not set `MIMITA_EXE_NAME` and do not create alternate development

```
python build_agent.py
```

Run:

```
C:\mimita-priv-v8\mimita.exe
```

All development builds must use the single canonical output:

## Single EXE Output

When building or testing the EXE, use build_agent.py instead of build.py, because build.py opens the EXE on the computer and may falsely appear to error when it has not.

Do not narrate internal reasoning.
Do not output chain-of-thought.

If there is a TODO comment in the file you are working on, and it is easy enough to do, just do it and continue rather than skipping it.

* Keep solutions practical and shippable.
* Fix problems directly.
* Do not ask for clarification unless required to proceed.
* Avoid unnecessary rewrites.
* Prefer minimal patches.
* Answer coding questions concretely.
* Read repository files directly.

Rules:

Use repository search before reasoning.

Never answer from assumptions before searching.

5. Implement the smallest correct fix.
4. Reason about the issue.
3. Read only relevant sections.
2. Identify only relevant files.
1. Search the repository first.

When solving coding problems:

## Repository Workflow

This is a C++17 OpenGL game engine.

# Mimita Engine

---

7. Verify
6. Deploy
5. Commit locally
4. Test locally
3. Implement locally
2. Plan
1. Inspect

Preferred workflow:

SSH should NOT be used as the primary development environment.

* server health checks
* deployment verification
* database inspection
* nginx
* PM2
* logs

SSH may be used for:

ssh root@107.191.48.226

SSH inspection command:

To apply changes: save the JSON file, wait ~1 second for hot-reload, changes appear immediately.

| `config/weaponsets.json` | Weapon set configurations | Weapon lists |
| `config/onlinemodes.json` | Community server modes | Mode definitions |
| `config/killfeed.json` | Killfeed display mode and weapon verbs | `mode` ("hud" or "chat"), weapon verbs |
| `config/gamemodes/duel.json` | Duel mode settings | `goal_value`, `time_limit_seconds` |
| `config/gamemodes/tdm.json` | TDM mode settings | `goal_value`, `time_limit_seconds`, `countdown_seconds` |
| `config/gamemodes/ffa.json` | FFA mode settings | `goal_value`, `time_limit_seconds`, `countdown_seconds` |
| `config/gui/pause-menu.json` | ESC pause menu | Button text, positions, colors |
| `config/gui/help-menu.json` | Help menu content and layout | Text, positions, colors |
| `config/gui/duel-queue-hud.json` | Duel queue/waiting screen | All elements |
| `config/gui/duel-match-hud.json` | Network duel match HUD (countdown, scoreboard) | `fontSize`, `textColor`, positions |
| `config/gui/duel-hud.json` | Local duel HUD (countdown, score, timer) | `fontSize`, `textColor`, positions |
| `config/gui/match-hud.json` | Match countdown, intermission text, recording indicator | `fontSize`, `textColor`, `x`, `y` positions |
|------|----------|--------------|
| File | Controls | What to edit |

These files are hot-reloaded automatically when saved. Edit them while the game is running:

# Hot-Reloadable JSON Config Files

* Test locally before deployment.
* Fixes discovered on VPS must be implemented locally.
* Do not edit production files unless investigating.
* Do not create production-only fixes.
* Development happens locally first.
* VPS is deployment target only.
* Local repository is the source of truth.

# Development Rules

* Do not create, modify, or delete music production files.
* If a game feature needs new gameplay SFX, add the `.wav` or `.mp3` file to `assets/sound/entity/`, `assets/sound/ui/`, or `assets/sound/weapon/` and `git add -f` it (the global ignore allows explicit tracking of gameplay audio).
* Game loads audio from `assets/sound/` at runtime.
* Music production files under `assets/sound/music/` and loose source/production `.wav`/`.mp3` files are IGNORED by `.gitignore` and must NEVER be committed.
* Gameplay sound effects under `assets/sound/entity/`, `assets/sound/ui/`, `assets/sound/weapon/` ARE tracked in Git.

# Asset Rules

This is the final quality gate. Nothing overrides it.

Do not claim work is complete while `overseer.py` returns anything other than `Overall Status: PASS`.

Every checker must pass. If any checker reports a finding — fix it and re-run.

Before marking ANY task as complete, ALWAYS run `python overseer.py` from the workspace root.

# Mandatory Overseer Check

Vague statements like "this rendering looks wrong" or "try a different approach" are insufficient. Always trace the exact data flow from input to output.

   - "The `death_ellipsoid` path computes `scaleVec` from a direction vector but applies it as a non-uniform scale in world space. When the hit direction is `(0.7, 0.7, 0)`, the scaleVec becomes `(0.7, 0.7, 1.0)` which stretches the sphere diagonally in XY but not along the actual hit axis. `drawFilledSphereOriented` builds an orthonormal basis from the direction vector and remaps via `impactBasis()`, so the sphere's local Z pole always aligns with the hit direction regardless of world-space orientation."
3. **Evidence from logs or code logic** — explain WHY the current code fails and WHY the fix works, citing specific function behavior, data flow, or log output. Example:

   ```
   DebugVis::drawFilledSphereOriented(camera, center, axis, 1.0f, drawColor, dims, localAxis);
   // CORRECT: oriented along hit direction

   DebugVis::drawFilledSphere(camera, effect.position, rad, drawColor, scaleVec);
   glm::vec3 scaleVec = dir * (len / std::max(rad, 0.001f)) + glm::vec3(1.0f) - dir;
   // WRONG: stretches along world axes
   ```cpp
2. **The exact code that should replace it** — include the full replacement snippet, not vague descriptions. Example:

   - The `death_ellipsoid` branch at `effect-part-render.cpp:297` uses `drawFilledSphere` with a `scaleVec` that stretches along world axes, not the hit direction
   - `src/effects/effect-part-render.cpp:307` renders `damage_impact_sphere` using `drawFilledSphereOriented` with `impact.localDimensions`
1. **The exact code path that is wrong** — include file path, line numbers, and the actual code. Example:

When reporting a bug or explaining a fix, ALWAYS show:

# Bug Report and Explanation Standards

You don't. Use `maxFrames` in the terminal or settings to cap FPS. If the frame rate is unstable, fix the frame pacer or reduce render cost — do not add VSync.

## If You Think You Need VSync

- Boot sequence — loud `[VSYNC] FORCED OFF` log confirming state
- Terminal `vsync` command — logs blocked, refuses to enable
- Settings menu — shows "VSync: OFF (forced)" with no toggle
- `FramePacer::setVSync()` — forces `mVSync = false`, logs blocked attempt
- `VideoSettings::save()` — always writes `"vsync": false`
- `VideoSettings::load()` — overrides JSON `"vsync": true` to `false`
- `VideoSettings::setVSync()` — forces `mVSync = false`, logs blocked attempt
- `Renderer::forceVSyncOff()` — explicit force-off with reason logging
- `Renderer::applyVideoMode()` — always calls `glfwSwapInterval(0)` after monitor changes
- `Renderer::setVSync()` — always calls `glfwSwapInterval(0)`, logs `[VSYNC] BLOCKED` if `on==true`

## Enforcement Points

VSync adds 16-29ms of driver-level stall per frame (measured at the Swap scope). The engine uses a software frame pacer (`FramePacer::endFrame()`) with `maxFrames` to control frame rate. VSync is incompatible with the performance target of <1ms frame times.

## Why

8. **Any code path that calls `Renderer::setVSync(true)` must be blocked** with a `[VSYNC] BLOCKED` log.
7. **Fullscreen/window transitions must always call `glfwSwapInterval(0)`** after `glfwSetWindowMonitor`.
6. **Never skip the software frame pacer** (`FramePacer::endFrame()`) by claiming "VSync handles pacing."
5. **Never add command-line flags that control VSync.**
4. **Never read `"vsync": true` from config and apply it.** Config loads must override any stored vsync value to `false`.
3. **Never add a UI toggle, slider, or terminal command that enables VSync.**
2. **Never write `mVSync = true`** in any file (Renderer, VideoSettings, FramePacer).
1. **Never call `glfwSwapInterval(1)` anywhere in the codebase.** Every call must be `glfwSwapInterval(0)`.

## Rules

VSync is **forced OFF globally** and must never be re-enabled. This is a permanent performance constraint.

# VSync Policy (Hard Rule — Never Override)

Never run collision logic in the raw render loop without tick quantization.

```
}
    // collision + damage logic here
for (uint32_t t = 0; t < ticksThisFrame; t++) {
const uint32_t ticksThisFrame = std::max(1u, (uint32_t)std::round(dt / tickDt));
const float tickDt = 1.0f / 60.0f;
```cpp
Weapon collision, NPC overlap detection, damage application, and knockback MUST use the tick accumulator pattern:

- Gameplay behavior varies by hardware
- High-FPS devices get more collision checks (unfair advantage)
- Low-FPS devices get fewer collision checks (unfair disadvantage)
All gameplay collision, damage, and physics MUST run at the fixed 60Hz tick rate, never per-frame. The game uses `constexpr double kClientFixedDt = 1.0 / 60.0` in `engine-tick-combat.cpp`. Running collision per-frame means:

# Tick Rate Rule (Hard Rule)

For build purposes, human or AI agents are authorized to terminate existing `mimita.exe` processes at any time, until this instruction is changed. This authorization applies only to releasing the executable lock so the updated build can be produced and tested.

Never launch `mimita.exe` without `--server` or `--timeout <secs>`. Without these flags the game opens a full graphics window and stays open indefinitely (it won't automatically exit). If you need to test server behavior, always use `--server --timeout 30 --no-coordinator` or similar so the process self-terminates.

6. Verify the deployed commit and restart only the relevant existing service after a successful pull.
5. Preserve and report any pre-existing untracked or local VPS files; never delete or overwrite them as part of a pull.
4. Pull only the confirmed branch with `git pull --ff-only origin <confirmed-branch>`.
3. Do not assume `develop/v2.0.1` or any version branch is current; those branches may be stale.
2. Confirm its branch name, latest commit, commit date, and intended task with the user before deployment.
1. Identify the most recently updated candidate branch from the authoritative Git remote.

Before pulling code onto the VPS:

# VPS Branch Verification and Deployment

# EXE Safety

*/
* fill in 3rd line
* fill in 2nd line
* fill in what this file DOES NOT do
* fill in 3rd line
* fill in 2nd line
* fill in purpose of file
/* purpose
// mm dd yyyy, hh mm
7 18 2026 addition: always maintain this format at the top of files

10. Report source/build evidence separately from final human playtesting.
9. Run `python overseer.py` and require `Overall Status: PASS`.
8. Build `mimita.exe` when code changes.
7. Run focused validation, including JSON or hot-reload checks when applicable.
6. Make the smallest correct fix.
5. Compare current behavior with the specification.
4. Trace the data flow from input to output.
3. Find the current implementation owner.
2. Search `docs/regressions/` for similar failures.
1. Read the routed specifications.

For an ambiguous bug:

## Bug Workflow

- `docs/operations/task-completion/task-completion.md`
- `docs/operations/vps-deployment/vps-deployment.md` when VPS work is involved
- `docs/operations/build-and-exe/build-and-exe.md`

Build, EXE, deployment, or completion:

- `docs/specs/debug-logging/debug-logging.md`

Logging or diagnostics:

- the relevant movement, networking, or GUI specification
- `docs/specs/gamemodes/gamemodes.md`

Game modes, matches, scoring, timers, or spawns:

- `docs/regressions/`
- the relevant weapons or gamemodes specification
- `docs/specs/networking/networking.md`

Networking or multiplayer:

- `docs/specs/performance/performance.md`
- `docs/architecture/collision/collision.md`
- `docs/specs/networking/networking.md`
- `docs/specs/weapons/weapons.md`

Weapons, damage, hit detection, or knockback:

- `docs/specs/performance/performance.md`
- `docs/architecture/collision/collision.md`
- `docs/specs/movement/movement.md`

Movement, physics, grounded state, or collision:

- `docs/regressions/`
- `docs/architecture/json-configuration/json-configuration.md` when settings or hot reload are involved
- `docs/specs/weapons/weapons.md` when damage or hit events are involved
- `docs/specs/gui/gui.md`

GUI, HUD, menus, damage numbers, or missing text:

## Task Routing

implementation. Do not silently rewrite a specification to match existing code.
Use the specification as the intended behavior, then compare it with the current
Before changing behavior, classify the task and read the routed documents below.

Known recurring failures belong under `docs/regressions/`.
Build, deployment, and completion procedures are under `docs/operations/`.
Architecture rules are under `docs/architecture/`.
The detailed behavioral specifications are organized under `docs/specs/`.

# MiMITA Documentation Router

Favor deleting code over adding code when both solutions achieve the same result.
The overseer automatically discovers and runs all checker scripts. No changes to `overseer.py` are required.

---

Avoid maintaining separate gameplay systems when a shared system is possible.

The primary difference should be who generates input.

* death
* damage
* inventory
* animation
* weapons
* physics
* movement

Shared:

AI Input
+
Entity

# NPC

Human Input
+
Entity

# Player

Preferred architecture:

Players and NPCs should share systems whenever practical.

# Player and NPC Philosophy

---

Accessibility should be considered during implementation, not added later.

* low frame rates
* low-end hardware
* visual impairments
* limited mobility
* controller use
* keyboard-only use

Consider:

Design for the widest possible audience.

# Accessibility

---

5. Never use printf
4. Use Debug::logThrottled for per-frame checks
3. Use Debug::log for detailed diagnostics (gated by debug flags)
2. Use Debug::warn for important state transitions (always visible)
1. Pick the right category (add a new one if needed)

When adding a new log:

Console output should maximize signal and minimize noise.

If a message repeats identically more than once per second, it must use logThrottled.

Spam is forbidden.

improvement.
rate-limited and useful; adding duplicate spam does not count as an
why the existing diagnostics are sufficient. Repeated paths must remain
agent must improve the relevant diagnostics or explicitly verify and document
debug category. On every AI-agent task that changes repository behavior, the
reason, and must use the owning subsystem's log file as well as the centralized
must identify the important input, decision, output, and rejection/failure
owner or entry point, directly beside the code it explains. The diagnostic
Every new feature must add or improve an adjacent debug logging hook at its

## Feature Diagnostic Requirement

* Include enough context to diagnose issues without recompiling
* Explain expected state, actual state, and why the engine made a decision
* Support throttling — never print identical messages every frame
* Use a specific category (Auth, Gui, Weapons, etc.)

Every log must:

Always use the centralized Debug::log / Debug::warn / Debug::logThrottled system.

NEVER use printf for debug logging.

## Logging Rules

---

sixty identical logs per second

instead of

one useful summary per second

Prefer:

* summary logging
* aggregated logging
* rate-limited logging

Support:

Avoid log spam.

## Rate Limiting

---

ragdoll_debug 1

npc_damage_debug 1

ui_debug 1

Example:

Categories should be independently enabled and disabled.

Ragdoll
Duel
World
Geometry
Animation
Audio
Networking
Replay
Collision
Physics
NpcCombat
Combat
Render
UI

Examples:

## Debug Categories

---

Avoid scattered printf() usage.

Debug::log(category, ...)

Prefer:

Use centralized logging.

* what outputs it produced
* what inputs it received
* why it is doing it
* what it is doing

Every system should be able to explain:

Visibility is preferred over guessing.

Debug everything.

# Debugging

---

UI
Debug
Network
Duel
NPC
Weapon
Replay

Examples:

Commands should be grouped by feature.

help_recent
help2
help

Support:

* dateAdded
* category
* usage
* description
* name

Commands should store metadata:

## Command Metadata

---

void registerReplayCommands();

src/replay/replay-commands.cpp

Example:

Feature systems own their own commands.

registerDebugCommands();
registerDuelCommands();
registerNpcCommands();
registerWeaponCommands();
registerReplayCommands();

Preferred:

Move command registration out of main.cpp.

## Command Registration

---

Avoid duplicate implementations.

UI, hotkeys, AI, networking, and gameplay should reuse shared actions where practical.

joinserver
hostserver
savedreplay
spawnnpc
reload
shoot

Examples:

The game should be playable and testable entirely through terminal commands.

Every gameplay action should eventually be callable through terminal commands.

# Terminal Commands

---

Keep interfaces small and obvious.

fireWeapon()
Weapon:

doMovement()
Movement:

doGravity()
Gravity:

loadReplay()
saveReplay()
Replay:

Example:

Subsystems should expose small APIs.

## Public APIs

---

Move ownership into subsystems.

* duel implementation
* npc implementation
* weapon implementation
* replay implementation

Not:

* shutdown
* update loop
* registration
* initialization

main.cpp should contain:

Example:

They should not own feature logic.

Main files should orchestrate.

## Main Files

---

should immediately reveal the owning subsystem.

duel
replay
weapon
npc

Searching for:

Feature logic spread across many unrelated files.

Bad:

src/ui/
src/network/
src/weapon/
src/npc/
src/duel/
src/replay/

Good:

Every feature should have a clear owner.

## Ownership

---

Avoid giant files that own many unrelated systems.

spawnBlood()
spawn-blood.cpp

sendPacket()
send-packet.cpp

doMovement()
movement.cpp

doGravity()
gravity.cpp

Examples:

* one obvious owner
* one responsibility
* under 100 lines

Ideal:

Prefer small files.

## Small Files

Target architecture. Use judgment.

# Architecture Direction

---

If a simpler implementation achieves the same result, prefer the simpler implementation.

Avoid unnecessary allocations, copies, complexity, and hidden work.

* Low-memory devices
* Small phones
* Old hardware
* Weak laptops
* Integrated graphics
* Low-end PCs

Target hardware includes:

Performance is the highest priority.

5. Extensibility
4. Debuggability
3. Readability
2. Simplicity
1. Performance

Optimize for:

# Core Philosophy

---

4. Never delete the build lock or run a second build to "speed things up." The lock serializes builds; the first build wins. If your changes landed after the running build compiled, wait for it to finish, then run your own build to compile the remainder.

3. **Only run a new `python build_agent.py` if no build is running** (lock file missing, or its owner PID is gone).

   - Only fix YOUR OWN errors from that shared log. Do not fix errors from other agents' files unless you own that code.
   - Check that your scope's object files under `build/obj-debug/` / `build/obj-release/` are newer than your edited sources (confirming your changes got compiled), and that `mimita.exe` was relinked after them.
   - Read `build/changelog.txt` (written after every run; first three lines show `Status: SUCCESS|NOTHING_CHANGED|FAILED`).
2. **If a build is already running — DO NOT start another one.** Use that running build's log to verify your own changes:

   If the file exists, look up the owner PID with `Get-Process -Id <pid>`. If that process is alive, a build is in progress.
   ```
   Get-Content "build\build-agent.lock" -ErrorAction SilentlyContinue | ConvertFrom-Json
   ```powershell
1. **Before building, always check if a build is already running:**

Multiple people/agents share one machine and one `build_agent.py` build lock. To keep agents from interfering:

# Single Shared Build (Multiple Agents)

Always check this status after building. If the human built between your source edits and your build_agent.py call, you will see NOTHING_CHANGED even though your edits should trigger a rebuild. Delete mimita.exe and rebuild in that case.

```
Status: SUCCESS|NOTHING_CHANGED|FAILED
Time: YYYY-MM-DD HH:MM:SS
=== BUILD CHANGELOG ===
```

The changelog at `build/changelog.txt` is written after every `build_agent.py` invocation. Its first three lines always show:

```
Remove-Item -Force "mimita.exe" -ErrorAction SilentlyContinue; python build_agent.py
```powershell
If you get NOTHING_CHANGED but changed source files, force a rebuild by deleting the EXE:

If you get NOTHING_CHANGED but changed source files, the human may have built first. Read `build/changelog.txt` for the full build log. The changelog always reflects the most recent build_agent.py run.

If the status is NOTHING_CHANGED and you expected changes, the human may have built first. Read `build/changelog.txt` for the full build log. The changelog always reflects the most recent build_agent.py run.

```
Status: NOTHING_CHANGED
```

or:

```
Status: SUCCESS
=== BUILD CHANGELOG ===
```

After any build_agent.py invocation, check the build result status printed in the output:

`mimita.exe`; never kill a possibly user-owned process to release the file.
`mimita.exe` or a non-EXE test harness. Before building, close any running
or feature-specific test executables. Focused tests must use the canonical
executables such as `mimita-chat-test.exe`, `mimita-duel-handshake-test.exe`,
Do not set `MIMITA_EXE_NAME` and do not create alternate development

```
python build_agent.py
```

Run:

```
C:\mimita-priv-v8\mimita.exe
```

All development builds must use the single canonical output:

## Single EXE Output

When building or testing the EXE, use build_agent.py instead of build.py, because build.py opens the EXE on the computer and may falsely appear to error when it has not.

Do not narrate internal reasoning.
Do not output chain-of-thought.

If there is a TODO comment in the file you are working on, and it is easy enough to do, just do it and continue rather than skipping it.

* Keep solutions practical and shippable.
* Fix problems directly.
* Do not ask for clarification unless required to proceed.
* Avoid unnecessary rewrites.
* Prefer minimal patches.
* Answer coding questions concretely.
* Read repository files directly.

Rules:

Use repository search before reasoning.

Never answer from assumptions before searching.

5. Implement the smallest correct fix.
4. Reason about the issue.
3. Read only relevant sections.
2. Identify only relevant files.
1. Search the repository first.

When solving coding problems:

## Repository Workflow

This is a C++17 OpenGL game engine.

# Mimita Engine

---

7. Verify
6. Deploy
5. Commit locally
4. Test locally
3. Implement locally
2. Plan
1. Inspect

Preferred workflow:

SSH should NOT be used as the primary development environment.

* server health checks
* deployment verification
* database inspection
* nginx
* PM2
* logs

SSH may be used for:

ssh root@107.191.48.226

SSH inspection command:

To apply changes: save the JSON file, wait ~1 second for hot-reload, changes appear immediately.

| `config/weaponsets.json` | Weapon set configurations | Weapon lists |
| `config/onlinemodes.json` | Community server modes | Mode definitions |
| `config/killfeed.json` | Killfeed display mode and weapon verbs | `mode` ("hud" or "chat"), weapon verbs |
| `config/gamemodes/duel.json` | Duel mode settings | `goal_value`, `time_limit_seconds` |
| `config/gamemodes/tdm.json` | TDM mode settings | `goal_value`, `time_limit_seconds`, `countdown_seconds` |
| `config/gamemodes/ffa.json` | FFA mode settings | `goal_value`, `time_limit_seconds`, `countdown_seconds` |
| `config/gui/pause-menu.json` | ESC pause menu | Button text, positions, colors |
| `config/gui/help-menu.json` | Help menu content and layout | Text, positions, colors |
| `config/gui/duel-queue-hud.json` | Duel queue/waiting screen | All elements |
| `config/gui/duel-match-hud.json` | Network duel match HUD (countdown, scoreboard) | `fontSize`, `textColor`, positions |
| `config/gui/duel-hud.json` | Local duel HUD (countdown, score, timer) | `fontSize`, `textColor`, positions |
| `config/gui/match-hud.json` | Match countdown, intermission text, recording indicator | `fontSize`, `textColor`, `x`, `y` positions |
|------|----------|--------------|
| File | Controls | What to edit |

These files are hot-reloaded automatically when saved. Edit them while the game is running:

# Hot-Reloadable JSON Config Files

* Test locally before deployment.
* Fixes discovered on VPS must be implemented locally.
* Do not edit production files unless investigating.
* Do not create production-only fixes.
* Development happens locally first.
* VPS is deployment target only.
* Local repository is the source of truth.

# Development Rules

* Do not create, modify, or delete music production files.
* If a game feature needs new gameplay SFX, add the `.wav` or `.mp3` file to `assets/sound/entity/`, `assets/sound/ui/`, or `assets/sound/weapon/` and `git add -f` it (the global ignore allows explicit tracking of gameplay audio).
* Game loads audio from `assets/sound/` at runtime.
* Music production files under `assets/sound/music/` and loose source/production `.wav`/`.mp3` files are IGNORED by `.gitignore` and must NEVER be committed.
* Gameplay sound effects under `assets/sound/entity/`, `assets/sound/ui/`, `assets/sound/weapon/` ARE tracked in Git.

# Asset Rules

This is the final quality gate. Nothing overrides it.

Do not claim work is complete while `overseer.py` returns anything other than `Overall Status: PASS`.

Every checker must pass. If any checker reports a finding — fix it and re-run.

Before marking ANY task as complete, ALWAYS run `python overseer.py` from the workspace root.

# Mandatory Overseer Check

Vague statements like "this rendering looks wrong" or "try a different approach" are insufficient. Always trace the exact data flow from input to output.

   - "The `death_ellipsoid` path computes `scaleVec` from a direction vector but applies it as a non-uniform scale in world space. When the hit direction is `(0.7, 0.7, 0)`, the scaleVec becomes `(0.7, 0.7, 1.0)` which stretches the sphere diagonally in XY but not along the actual hit axis. `drawFilledSphereOriented` builds an orthonormal basis from the direction vector and remaps via `impactBasis()`, so the sphere's local Z pole always aligns with the hit direction regardless of world-space orientation."
3. **Evidence from logs or code logic** — explain WHY the current code fails and WHY the fix works, citing specific function behavior, data flow, or log output. Example:

   ```
   DebugVis::drawFilledSphereOriented(camera, center, axis, 1.0f, drawColor, dims, localAxis);
   // CORRECT: oriented along hit direction

   DebugVis::drawFilledSphere(camera, effect.position, rad, drawColor, scaleVec);
   glm::vec3 scaleVec = dir * (len / std::max(rad, 0.001f)) + glm::vec3(1.0f) - dir;
   // WRONG: stretches along world axes
   ```cpp
2. **The exact code that should replace it** — include the full replacement snippet, not vague descriptions. Example:

   - The `death_ellipsoid` branch at `effect-part-render.cpp:297` uses `drawFilledSphere` with a `scaleVec` that stretches along world axes, not the hit direction
   - `src/effects/effect-part-render.cpp:307` renders `damage_impact_sphere` using `drawFilledSphereOriented` with `impact.localDimensions`
1. **The exact code path that is wrong** — include file path, line numbers, and the actual code. Example:

When reporting a bug or explaining a fix, ALWAYS show:

# Bug Report and Explanation Standards

You don't. Use `maxFrames` in the terminal or settings to cap FPS. If the frame rate is unstable, fix the frame pacer or reduce render cost — do not add VSync.

## If You Think You Need VSync

- Boot sequence — loud `[VSYNC] FORCED OFF` log confirming state
- Terminal `vsync` command — logs blocked, refuses to enable
- Settings menu — shows "VSync: OFF (forced)" with no toggle
- `FramePacer::setVSync()` — forces `mVSync = false`, logs blocked attempt
- `VideoSettings::save()` — always writes `"vsync": false`
- `VideoSettings::load()` — overrides JSON `"vsync": true` to `false`
- `VideoSettings::setVSync()` — forces `mVSync = false`, logs blocked attempt
- `Renderer::forceVSyncOff()` — explicit force-off with reason logging
- `Renderer::applyVideoMode()` — always calls `glfwSwapInterval(0)` after monitor changes
- `Renderer::setVSync()` — always calls `glfwSwapInterval(0)`, logs `[VSYNC] BLOCKED` if `on==true`

## Enforcement Points

VSync adds 16-29ms of driver-level stall per frame (measured at the Swap scope). The engine uses a software frame pacer (`FramePacer::endFrame()`) with `maxFrames` to control frame rate. VSync is incompatible with the performance target of <1ms frame times.

## Why

8. **Any code path that calls `Renderer::setVSync(true)` must be blocked** with a `[VSYNC] BLOCKED` log.
7. **Fullscreen/window transitions must always call `glfwSwapInterval(0)`** after `glfwSetWindowMonitor`.
6. **Never skip the software frame pacer** (`FramePacer::endFrame()`) by claiming "VSync handles pacing."
5. **Never add command-line flags that control VSync.**
4. **Never read `"vsync": true` from config and apply it.** Config loads must override any stored vsync value to `false`.
3. **Never add a UI toggle, slider, or terminal command that enables VSync.**
2. **Never write `mVSync = true`** in any file (Renderer, VideoSettings, FramePacer).
1. **Never call `glfwSwapInterval(1)` anywhere in the codebase.** Every call must be `glfwSwapInterval(0)`.

## Rules

VSync is **forced OFF globally** and must never be re-enabled. This is a permanent performance constraint.

# VSync Policy (Hard Rule — Never Override)

Never run collision logic in the raw render loop without tick quantization.

```
}
    // collision + damage logic here
for (uint32_t t = 0; t < ticksThisFrame; t++) {
const uint32_t ticksThisFrame = std::max(1u, (uint32_t)std::round(dt / tickDt));
const float tickDt = 1.0f / 60.0f;
```cpp
Weapon collision, NPC overlap detection, damage application, and knockback MUST use the tick accumulator pattern:

- Gameplay behavior varies by hardware
- High-FPS devices get more collision checks (unfair advantage)
- Low-FPS devices get fewer collision checks (unfair disadvantage)
All gameplay collision, damage, and physics MUST run at the fixed 60Hz tick rate, never per-frame. The game uses `constexpr double kClientFixedDt = 1.0 / 60.0` in `engine-tick-combat.cpp`. Running collision per-frame means:

# Tick Rate Rule (Hard Rule)

For build purposes, human or AI agents are authorized to terminate existing `mimita.exe` processes at any time, until this instruction is changed. This authorization applies only to releasing the executable lock so the updated build can be produced and tested.

Never launch `mimita.exe` without `--server` or `--timeout <secs>`. Without these flags the game opens a full graphics window and stays open indefinitely (it won't automatically exit). If you need to test server behavior, always use `--server --timeout 30 --no-coordinator` or similar so the process self-terminates.

6. Verify the deployed commit and restart only the relevant existing service after a successful pull.
5. Preserve and report any pre-existing untracked or local VPS files; never delete or overwrite them as part of a pull.
4. Pull only the confirmed branch with `git pull --ff-only origin <confirmed-branch>`.
3. Do not assume `develop/v2.0.1` or any version branch is current; those branches may be stale.
2. Confirm its branch name, latest commit, commit date, and intended task with the user before deployment.
1. Identify the most recently updated candidate branch from the authoritative Git remote.

Before pulling code onto the VPS:

# VPS Branch Verification and Deployment

# EXE Safety

*/
* fill in 3rd line
* fill in 2nd line
* fill in what this file DOES NOT do
* fill in 3rd line
* fill in 2nd line
* fill in purpose of file
/* purpose
// mm dd yyyy, hh mm
7 18 2026 addition: always maintain this format at the top of files

10. Report source/build evidence separately from final human playtesting.
9. Run `python overseer.py` and require `Overall Status: PASS`.
8. Build `mimita.exe` when code changes.
7. Run focused validation, including JSON or hot-reload checks when applicable.
6. Make the smallest correct fix.
5. Compare current behavior with the specification.
4. Trace the data flow from input to output.
3. Find the current implementation owner.
2. Search `docs/regressions/` for similar failures.
1. Read the routed specifications.

For an ambiguous bug:

## Bug Workflow

- `docs/operations/task-completion/task-completion.md`
- `docs/operations/vps-deployment/vps-deployment.md` when VPS work is involved
- `docs/operations/build-and-exe/build-and-exe.md`

Build, EXE, deployment, or completion:

- `docs/specs/debug-logging/debug-logging.md`

Logging or diagnostics:

- the relevant movement, networking, or GUI specification
- `docs/specs/gamemodes/gamemodes.md`

Game modes, matches, scoring, timers, or spawns:

- `docs/regressions/`
- the relevant weapons or gamemodes specification
- `docs/specs/networking/networking.md`

Networking or multiplayer:

- `docs/specs/performance/performance.md`
- `docs/architecture/collision/collision.md`
- `docs/specs/networking/networking.md`
- `docs/specs/weapons/weapons.md`

Weapons, damage, hit detection, or knockback:

- `docs/specs/performance/performance.md`
- `docs/architecture/collision/collision.md`
- `docs/specs/movement/movement.md`

Movement, physics, grounded state, or collision:

- `docs/regressions/`
- `docs/architecture/json-configuration/json-configuration.md` when settings or hot reload are involved
- `docs/specs/weapons/weapons.md` when damage or hit events are involved
- `docs/specs/gui/gui.md`

GUI, HUD, menus, damage numbers, or missing text:

## Task Routing

implementation. Do not silently rewrite a specification to match existing code.
Use the specification as the intended behavior, then compare it with the current
Before changing behavior, classify the task and read the routed documents below.

Known recurring failures belong under `docs/regressions/`.
Build, deployment, and completion procedures are under `docs/operations/`.
Architecture rules are under `docs/architecture/`.
The detailed behavioral specifications are organized under `docs/specs/`.

# MiMITA Documentation Router

Favor deleting code over adding code when both solutions achieve the same result.
The overseer automatically discovers and runs all checker scripts. No changes to `overseer.py` are required.
