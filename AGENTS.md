Favor deleting code over adding code when both solutions achieve the same result.

# Mandatory Overseer Check

Before marking ANY task as complete, ALWAYS run `python overseer.py` from the workspace root.

Every checker must pass. If any checker reports a finding — fix it and re-run.

Do not claim work is complete while `overseer.py` returns anything other than `Overall Status: PASS`.

This is the final quality gate. Nothing overrides it.

# Asset Rules

* Gameplay sound effects under `assets/sound/entity/`, `assets/sound/ui/`, `assets/sound/weapon/` ARE tracked in Git.
* Music production files under `assets/sound/music/` and loose source/production `.wav`/`.mp3` files are IGNORED by `.gitignore` and must NEVER be committed.
* Game loads audio from `assets/sound/` at runtime.
* If a game feature needs new gameplay SFX, add the `.wav` or `.mp3` file to `assets/sound/entity/`, `assets/sound/ui/`, or `assets/sound/weapon/` and `git add -f` it (the global ignore allows explicit tracking of gameplay audio).
* Do not create, modify, or delete music production files.

# Development Rules

* Local repository is the source of truth.
* VPS is deployment target only.
* Development happens locally first.
* Do not create production-only fixes.
* Do not edit production files unless investigating.
* Fixes discovered on VPS must be implemented locally.
* Test locally before deployment.

SSH inspection command:

ssh root@107.191.48.226

SSH may be used for:

* logs
* PM2
* nginx
* database inspection
* deployment verification
* server health checks

SSH should NOT be used as the primary development environment.

Preferred workflow:

1. Inspect
2. Plan
3. Implement locally
4. Test locally
5. Commit locally
6. Deploy
7. Verify

---

# Mimita Engine

This is a C++17 OpenGL game engine.

## Repository Workflow

When solving coding problems:

1. Search the repository first.
2. Identify only relevant files.
3. Read only relevant sections.
4. Reason about the issue.
5. Implement the smallest correct fix.

Never answer from assumptions before searching.

Use repository search before reasoning.

Rules:

* Read repository files directly.
* Answer coding questions concretely.
* Prefer minimal patches.
* Avoid unnecessary rewrites.
* Do not ask for clarification unless required to proceed.
* Fix problems directly.
* Keep solutions practical and shippable.

If there is a TODO comment in the file you are working on, and it is easy enough to do, just do it and continue rather than skipping it.

Do not output chain-of-thought.
Do not narrate internal reasoning.

When building or testing the EXE, use build_agent.py instead of build.py, because build.py opens the EXE on the computer and may falsely appear to error when it has not.

After any build_agent.py invocation, check the build result status printed in the output:

```
=== BUILD CHANGELOG ===
Status: SUCCESS
```

or:

```
Status: NOTHING_CHANGED
```

If the status is NOTHING_CHANGED and you expected changes, the human may have built first. Read `build/changelog.txt` for the full build log. The changelog always reflects the most recent build_agent.py run.

If you get NOTHING_CHANGED but changed source files, the human may have built first. Read `build/changelog.txt` for the full build log. The changelog always reflects the most recent build_agent.py run.

If you get NOTHING_CHANGED but changed source files, force a rebuild by deleting the EXE:
```powershell
Remove-Item -Force "mimita.exe" -ErrorAction SilentlyContinue; python build_agent.py
```

The changelog at `build/changelog.txt` is written after every `build_agent.py` invocation. Its first three lines always show:

```
=== BUILD CHANGELOG ===
Time: YYYY-MM-DD HH:MM:SS
Status: SUCCESS|NOTHING_CHANGED|FAILED
```

Always check this status after building. If the human built between your source edits and your build_agent.py call, you will see NOTHING_CHANGED even though your edits should trigger a rebuild. Delete mimita.exe and rebuild in that case.

---

# Core Philosophy

Optimize for:

1. Performance
2. Simplicity
3. Readability
4. Debuggability
5. Extensibility

Performance is the highest priority.

Target hardware includes:

* Low-end PCs
* Integrated graphics
* Weak laptops
* Old hardware
* Small phones
* Low-memory devices

Avoid unnecessary allocations, copies, complexity, and hidden work.

If a simpler implementation achieves the same result, prefer the simpler implementation.

---

# Architecture Direction

Target architecture. Use judgment.

## Small Files

Prefer small files.

Ideal:

* under 100 lines
* one responsibility
* one obvious owner

Examples:

gravity.cpp
doGravity()

movement.cpp
doMovement()

send-packet.cpp
sendPacket()

spawn-blood.cpp
spawnBlood()

Avoid giant files that own many unrelated systems.

---

## Ownership

Every feature should have a clear owner.

Good:

src/replay/
src/duel/
src/npc/
src/weapon/
src/network/
src/ui/

Bad:

Feature logic spread across many unrelated files.

Searching for:

npc
weapon
replay
duel

should immediately reveal the owning subsystem.

---

## Main Files

Main files should orchestrate.

They should not own feature logic.

Example:

main.cpp should contain:

* initialization
* registration
* update loop
* shutdown

Not:

* replay implementation
* weapon implementation
* npc implementation
* duel implementation

Move ownership into subsystems.

---

## Public APIs

Subsystems should expose small APIs.

Example:

Replay:
saveReplay()
loadReplay()

Gravity:
doGravity()

Movement:
doMovement()

Weapon:
fireWeapon()

Keep interfaces small and obvious.

---

# Terminal Commands

Every gameplay action should eventually be callable through terminal commands.

The game should be playable and testable entirely through terminal commands.

Examples:

shoot
reload
spawnnpc
savedreplay
hostserver
joinserver

UI, hotkeys, AI, networking, and gameplay should reuse shared actions where practical.

Avoid duplicate implementations.

---

## Command Registration

Move command registration out of main.cpp.

Preferred:

registerReplayCommands();
registerWeaponCommands();
registerNpcCommands();
registerDuelCommands();
registerDebugCommands();

Feature systems own their own commands.

Example:

src/replay/replay-commands.cpp

void registerReplayCommands();

---

## Command Metadata

Commands should store metadata:

* name
* description
* usage
* category
* dateAdded

Support:

help
help2
help_recent

Commands should be grouped by feature.

Examples:

Replay
Weapon
NPC
Duel
Network
Debug
UI

---

# Debugging

Debug everything.

Visibility is preferred over guessing.

Every system should be able to explain:

* what it is doing
* why it is doing it
* what inputs it received
* what outputs it produced

Use centralized logging.

Prefer:

Debug::log(category, ...)

Avoid scattered printf() usage.

---

## Debug Categories

Examples:

UI
Render
Combat
NpcCombat
Physics
Collision
Replay
Networking
Audio
Animation
Geometry
World
Duel
Ragdoll

Categories should be independently enabled and disabled.

Example:

ui_debug 1

npc_damage_debug 1

ragdoll_debug 1

---

## Rate Limiting

Avoid log spam.

Support:

* rate-limited logging
* aggregated logging
* summary logging

Prefer:

one useful summary per second

instead of

sixty identical logs per second

---

## Logging Rules

NEVER use printf for debug logging.

Always use the centralized Debug::log / Debug::warn / Debug::logThrottled system.

Every log must:

* Use a specific category (Auth, Gui, Weapons, etc.)
* Support throttling — never print identical messages every frame
* Explain expected state, actual state, and why the engine made a decision
* Include enough context to diagnose issues without recompiling

Spam is forbidden.

If a message repeats identically more than once per second, it must use logThrottled.

Console output should maximize signal and minimize noise.

When adding a new log:

1. Pick the right category (add a new one if needed)
2. Use Debug::warn for important state transitions (always visible)
3. Use Debug::log for detailed diagnostics (gated by debug flags)
4. Use Debug::logThrottled for per-frame checks
5. Never use printf

---

# Accessibility

Design for the widest possible audience.

Consider:

* keyboard-only use
* controller use
* limited mobility
* visual impairments
* low-end hardware
* low frame rates

Accessibility should be considered during implementation, not added later.

---

# Player and NPC Philosophy

Players and NPCs should share systems whenever practical.

Preferred architecture:

# Player

Entity
+
Human Input

# NPC

Entity
+
AI Input

Shared:

* movement
* physics
* weapons
* animation
* inventory
* damage
* death

The primary difference should be who generates input.

Avoid maintaining separate gameplay systems when a shared system is possible.

---

# Performance Rules

Prefer:

* predictable execution
* cache-friendly structures
* simple data flow
* low allocation counts
* low memory usage

Measure before optimizing.

But when two implementations are equal:

prefer the simpler and faster one.

Avoid architecture that exists only for abstraction.

Abstractions should earn their cost.

---

# General Rule

If a feature becomes difficult to find, debug, test, or optimize:

the ownership is probably wrong.

Move it to the subsystem that logically owns it.

---

==================================================
CHARACTER PHYSICS DESIGN PRINCIPLE
==================================

Mimita uses full-body collision.

The player is not represented solely by a movement capsule.

Instead:

* Head collides
* Torso collides
* Left arm collides
* Right arm collides
* Left leg collides
* Right leg collides
* Equipped weapon collides
* Future held objects collide

The movement capsule may still exist as a stability/root movement collider, but body parts are real physical colliders that participate in:

* World collision
* Damage detection
* Hit detection
* Physics interactions
* Future object interaction

Goal:

If a body part physically reaches a wall, object, floor, ceiling, or obstacle, that body part should collide with it.

Weapons should not pass through walls.

Arms should not pass through walls.

Legs should not pass through walls.

Body parts should not pass through walls.

Future systems should assume full-body collision is desired.

==================================================

# Code Architecture

## Function Reuse First

Before writing a new function:

Search the entire repo.

Assume the function already exists until proven otherwise.

## Prove Necessity

Every new function must answer:

Why can't an existing function be reused?

## Delete Duplicates

If two functions perform substantially the same job:

Prefer merging them.

Prefer deleting one.

## One Source of Truth

Common behaviors should have exactly one implementation.

Examples:

weapon grip position

hand attachment

socket lookup

collision queries

world-space labels

NPC targeting

spawn selection

map loading

## Reuse Over Creation

Preferred order:

1. Reuse existing function
2. Expand existing function
3. Generalize existing function
4. Create new function only if required

## Minimize Code Growth

Adding code is not automatically progress.

If a feature can be implemented with:

+0 functions

instead of

+3 functions

prefer +0.

## Expand Before Creating

Before creating a new function with its own arguments, logic, and responsibilities:

Prefer expanding an existing function's parameters instead.

Examples:

* Adding an optional bool parameter to an existing function.
* Adding a defaulted enum argument to control behavior.
* Extending an existing signature rather than writing a parallel function.

A function with 8 parameters that already exists is better than 3 new functions with 3 parameters each.

Existing functions already have:

* Call sites.
* Test coverage.
* Known behavior.
* Integration with surrounding systems.

New functions need all of the above from scratch.

Prefer:

```cpp
// existing, expanded
void doJump(Player& p, bool jumpHeld, float dt, bool allowWorldContactJump = false);
```

Over:

```cpp
// new, separate
void doWorldContactJump(Player& p, float dt);
void doWallJump(Player& p, float dt);
void doCeilingJump(Player& p, float dt);
```

This does not mean functions should never be split.

It means:

Expanding an existing function should be the default choice.

Creating a new function requires justification.

==================================================

---

# TASK COMPLETION REQUIREMENTS

Before ending any task:

1. **Run specialized skills** — Load and execute any skill relevant to the work (collision, physics, state, dependencies, etc.). See "Specialized Skills Check" in the Overseer skill.
2. **Build** if code changed (using `build_agent.py`)
3. **Run relevant validation/tests**
4. **Verify expected outputs exist**

## Final Validation (Required)

Before considering ANY task complete, always execute:

```
python overseer.py
```

Every checker must pass.

Do not claim work is complete while any checker reports failures.

If a checker fails:

1. Fix the reported issues.
2. Run overseer.py again.
3. Repeat until every checker passes.

5. **Save logs**
6. **Trigger completion notification script**
7. **Print summary**

Agents should never simply stop after editing files.

They must validate work first.

If Overseer cannot be loaded: treat this as a FAILURE condition. Run the diagnostics section below and report the exact reason.

---

# TASK COMPLETION HOOK

When work is complete, run:

```
devscripts\agent_finish.bat [task_name]
```

or:

```
python devscripts/agent_task_complete.py [task_name]
```

This will:

* Play a completion sound (`assets/sound/entity/player/spawning.wav`)
* Show a Windows toast notification ("MiMITA Agent: Task Completed")
* Print `[AGENT COMPLETE]` to the console

## Example

After building and verifying:

```
python build_agent.py
:: check for SUCCESS
devscripts\agent_finish.bat "Fix duel replay flow"
```

## Failure handling

If the sound file is missing: a warning is printed.
If the notification fails: a warning is printed.
The script never crashes.

---

# PAUSE / QUESTION PROTOCOL

Before stopping work or asking the user a question, ALWAYS run:

```
python devscripts/agent_task_complete.py "(reason)"
```

This plays a completion sound so the user knows the agent has paused and expects interaction. Examples:

* Before asking the user which phase to work on next
* Before presenting a choice or asking a question
* Before stopping work to wait for user input

DO NOT run this for simple status updates mid-task. Only run it when the agent is about to yield control to the user for a decision or because work is complete.

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

# Mimita Website Design Philosophy v1

Mimita should NOT feel like:

- a startup
- SaaS
- modern corporate UI
- conversion optimized
- investor friendly
- clean minimalism
- Apple
- Stripe
- Discord

Instead it should feel like:

- Yale School of Art
- old Newgrounds
- old Miniclip
- early Roblox
- early 4chan
- Blingee
- PicMix
- Windows XP
- Windows 7
- weird internet art
- pages made by someone who genuinely thought "this looks cool"

The site should make visitors think:

"What is this?"
"This is crazy."
"I want to explore."
"I want to become good at Mimita."
"I want to contribute."

## Core Principles

### 1. Usability always wins.

Important actions remain simple:

- login
- signup
- reset password
- delete account
- donations
- reports
- downloads

Serious flows should be calmer than the rest of the website.

### 2. The website is a toy.

Every page should contain something fun.

Examples:

- draggable objects
- global counters
- silly NPCs
- interactive buttons
- small physics toys
- playful animations

### 3. Controlled chaos.

Things should feel intentionally imperfect.

Examples:

- random 1-3px offsets
- occasional overlapping decorations
- slightly rotated stickers
- images shifted a few pixels

Never enough to hurt usability.

### 4. Dark first.

Use OLED black backgrounds.

Bright colors exist inside darkness.

### 5. Ugly color theory.

Use combinations people normally avoid.

Examples:

acid green
blood red
purple
cyan
yellow
beige
dark blue

Never because they convert better.

Only because they have personality.

### 6. Variation.

Different pages should feel different.

Different:

- colors
- decorations
- backgrounds
- toys
- accents

The site should not feel template-driven.

### 7. Procedural personality.

Small decorative randomness is encouraged.

Safe randomness:

- rotation
- hue shift
- sticker position
- decorative images
- tiny offsets

Never randomize:

- important buttons
- forms
- navigation
- accessibility

### 8. Animations.

Animations should feel like:

Windows XP
Windows 7
Flash
old web

Not because they are actually lagging.

The illusion is jank.

Implementation should remain smooth.

### 9. Boxes.

Default style:

- square corners
- mostly 1px borders
- occasional thick borders
- occasional no borders

Avoid rounded corners.

### 10. Everything should encourage curiosity instead of conversion.

---

# Asset Management

* `assets/sound/music/ingame/donttrack` is intentionally excluded from source control via `.gitignore`. It contains local music work files, exports, experiments, and temporary audio assets. Any file that should become part of the game must be moved out of this folder into the proper tracked asset location.

---

# Mandatory Final Check

Before completing any task that modifies code, configuration, scripts, documentation, or build files, always execute:

```
python overseer.py
```

Every checker must pass.

## Zero-Tolerance Policy

Overseer is the final authority. Nothing is considered complete until `python overseer.py` returns:

```
Overall Status:  PASS
```

The following statuses are NOT acceptable: WARNING, LOW, MEDIUM, HIGH, CRITICAL, or any finding that requires action. Any finding whatsoever — including "acceptable warning," "justified," "known issue," "technical debt," or "follow-up item" — is a failed review.

## Workflow

1. Make code changes
2. Run `python overseer.py`
3. If any checker finds ANY issue — fix it, re-run overseer.py
4. Repeat until every checker passes
5. Only then claim the task is complete

## Forbidden

- Arguing that a warning is acceptable
- Marking work complete with unresolved findings
- Deferring issues to a follow-up
- Accepting technical debt without fixing it immediately

## Exceptions

If overseer.py cannot run (e.g., Python not available), explain exactly why in the completion report. Do not skip the check.

## Temporary Notice (2026-07-15)

overseer.py is currently broken and needs to be fixed. Until it is repaired, treat it as a warning, not a blocker. Do not let overseer failure prevent task completion. Focus on building and verifying via build_agent.py instead.

## Extending

To add a new checker, create a new directory under `.opencode/skills/<name>/` with a `checker.py` that:

- Prints results to stdout
- Returns exit code 0 for PASS, non-zero for FAIL
- Runs in under 120 seconds

The overseer automatically discovers and runs all checker scripts. No changes to `overseer.py` are required.
