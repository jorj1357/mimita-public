// 09 06 2026, 00 00
/* purpose
* define terminal commands as a complete control and validation surface
* make every important game feature usable without the visual interface
* let commands drive the same actions used by GUI, input, AI, and tests
* this file DOES NOT require the terminal to replace the visual interface
* this file DOES NOT define feature behavior owned by other specifications
* this file DOES NOT permit commands to bypass server authority or security
*/

# Terminal Commands

## Core rule

The entire game should become playable and testable through terminal commands.
The terminal is an action and validation interface, not a second implementation
of gameplay.

Build the command path first or alongside the visual path. The GUI, keyboard,
controller, AI, TAS bot, and automated tests should call the same shared action
functions that terminal commands call.

This lets an agent or developer reproduce a behavior, change configuration,
run a scenario, and inspect the result without opening a graphics window or
performing a visual test for every iteration.

## Command coverage

Over time, commands should cover:

- movement: `walkforward`, `walkbackward`, strafe, jump, dash, and down-dash;
- aiming and weapons: aim, fire, reload, equip, and weapon-set selection;
- actors: spawn, remove, possess, NPC difficulty, and actor input;
- matches: create, join, leave, start, stop, mode, map, score, and timer;
- networking: host, connect, disconnect, retry, inspect state, and packet test;
- chat and accounts: send chat, open or close chat, login, logout, and profile;
- avatars and presentation: select avatar, reload JSON, inspect HUD, and set
  graphics or accessibility options;
- replay: record, stop, save, load, play, inspect events, and export;
- diagnostics: enable categories, inspect state, run self-tests, and capture
  structured results;
- lifecycle: pause, resume, quit, and controlled server shutdown.

The exact command names are implementation details. The coverage goal is that
no important feature is reachable only through a mouse click or visual menu.

## Timing model

Commands should have explicit timing semantics. A command such as
`walkforward` may apply movement input for one simulation tick, while `jump`
should enqueue the next valid jump action as soon as possible. Input capture may
be responsive per frame, but authoritative gameplay and simulation remain on
their documented fixed tick.

Commands that send network input should use the same client send path as normal
input. Commands must not directly mutate server-owned state from the client.

## Command ownership

Commands belong to the subsystem they control. Prefer small registrations such
as replay, weapon, NPC, duel, network, UI, and debug command modules rather than
placing all command logic in `main.cpp`.

Each command should define a name, description, usage, category, date added,
arguments, accepted values, and a structured success or failure result.

## Deterministic validation

Commands should support noninteractive scenarios with explicit seeds, timeouts,
tick counts, and machine-readable output. A useful test should prove the input,
decision, resulting state, and rejection reason where applicable.

When a visual feature is changed, first validate its configuration load, state,
and command result. Human visual review is still required for appearance, but
it should not be the only way to discover whether the feature works.

## Live identity command

`versioninfo` is a hot-package diagnostic command. It prints the running EXE
path, process/PID, session, process uptime, local active generation/hash,
server generation/hash/phase/tick, room/server identity, and the authoritative
`events.jsonl` path. It also writes `versioninfo.executed` to that JSONL.

The terminal dispatches hot-package commands before cold `ConsoleCommand`
entries. The command implementation can therefore change in the DLL and be
activated without restarting the EXE. The generic `runtime.info` capability
is the single cold-owned fact provider; future diagnostic commands should use
that same bridge instead of adding one cold command per report.

## AI runtime validation workflow

An AI agent investigating or changing behavior must use the real runtime path
before creating a new test. The required loop is:

1. Read the routed specification, ownership documents, changelog, regression
   records, and existing attempts.
2. Trace the current input/configuration-to-behavior path and identify the
   first missing or incorrect state.
3. Add bounded, owner-level diagnostics through `StructuredLogger`; do not
   create a gameplay-local log file or a new synthetic test merely to make a
   claim observable.
4. Build a newly named executable with the canonical build process.
5. Run `mimita-<timestamp>.exe --versioninfo` first. Capture the prominently
   printed `EVENTS_JSONL_PATH` value and use that exact file for the rest of
   the run.
6. Launch the real executable and exercise the behavior through the normal
   game, terminal-command, or bounded runtime-scenario path. Use a deterministic
   tick schedule when timing matters.
7. Read the live `events.jsonl` while the run is active and after it ends.
   Verify that the intended path was reached, then compare expected, actual,
   difference, and first-divergence fields.
8. Iterate on the owning implementation until the runtime numbers satisfy the
   specification. Report build, runtime-log, and human visual/gameplay
   evidence separately.

Existing unit and component tests remain useful for narrow invariants, file
formats, and failure handling. They must be labeled by what they actually
exercise and must not be presented as proof of live gameplay. New behavior
should receive a runtime scenario and structured JSONL evidence before a new
synthetic self-test is considered.
