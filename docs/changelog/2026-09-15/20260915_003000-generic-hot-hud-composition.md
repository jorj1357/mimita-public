# Generic hot HUD/UI composition (ui.frame + render.ui)

Date: 2026-09-15 00:30 EST (UTC 2026-09-15T04:30:00Z)
Branch: `8292026stash` (worktree, uncommitted)
Result: `PASS_WITH_HUMAN_REVIEW`

## 1. HUD ownership before

- COLD composition/policy: `engineTickUI` -> `engineTickUIHUD` /
  `engine-tick-ui-hud.cpp` -> `MatchTimer`/`MatchLeaderboard`.
- COLD reusable backend mechanism: `gui/ui-system.*` (`uiDrawText`,
  `uiDrawRect`, `uiDrawImage`, font rasterization).
- Already hot: the generic HUD text seed (`GAME_RENDER_DEBUG_HUD_TEXT`).
- Dead: none identified in this region.

## 2. Generic hot UI model

One generic `render.ui` capability with `GameUiCommandV1` primitives: TEXT,
PANEL, BAR, IMAGE (rect, color, value, scale, logical resource id, text). Layout
(stack/row/anchor) is data the hot system computes; the kernel only draws
primitives. No TdmHud/FfaHud/CounterStrikeHud type.

## 3. Cold backend kept cold

`LiveUi` draws through the existing `gui/ui-system.*` (text/rect/image/font).
Input/window/GPU submission remain cold.

## 4. ui.frame hot domain

`GAME_DOMAIN_UI` runs once per frame in `engine-tick-ui.cpp`:
`LiveUi::beginFrame` -> hot `ui.frame` systems -> `LiveUi::endFrameAndDraw`.
The engine loop only knows "run ui.frame".

## 5. First real migration: match timer

Hot `modules/ui/hud.cpp` (`hot.match-hud`) composes the timer (top center panel)
from the generic replicated `MatchHudState` dynamic component. Editing this hot
source changes the running HUD.

## 6. Second region: score panel

The same hot system composes the score panel (RED/BLUE labels + scores) and a
round bar. Both regions are hot-owned; the cold code no longer draws them when
the hot system emits.

## 7/8. Mode-owned composition + widget sufficiency

A runtime mode package can register a `ui.frame` system reading its own schemas;
the timer/score/phase/bar primitives are sufficient for a Counter-Strike-like
HUD (round timer, team scores, phase text, objective text via a future
`MatchHudState` field set). `MatchHudState` population by the mode package is the
next integration step.

## 9. Objective UI

Design-ready: the hot HUD reads a generic replicated state component. The server
objective implementation is another agent's area; no objective-specific kernel UI
branch was added.

## 10. No hot pointer caching

Widget commands are plain data (rect/color/text/logical resource id). No DLL
function pointers or raw GPU handles are cached in UI state.

## 11. Resource provider integration

Not implemented for images yet; the IMAGE command carries a path fallback. Wiring
`UiImage` to `PresentationResourceProvider` logical ids is a follow-up. No second
UI-asset hot-reload system was created.

## 12. Cold owner removed

`engine-tick-ui-hud.cpp` cold `MatchTimer` draw is now gated on
`!LiveUi::hotOwnsHud()` (compatibility fallback once a hot ui.frame system is
active). Not yet deleted; classification: compatibility fallback.

## 13. Live edit target

Not run (no visible client). Editing `modules/ui/hud.cpp` and saving would
activate a new generation and change the HUD on the running client.

## 14. Falsification tests

`--hot-combat-selftest`: render.ui resolves; buffers a widget; ui.frame fails
safe without match HUD state; emits HUD widget commands with state; composition is
deterministic. No gamemode UI enum; no feature-specific game-api field.

## Evidence

- `python build_agent.py` -> `Status: SUCCESS`.
- `build_game_dll.py` -> `build/mimita-game.dll`.
- `--hot-combat-selftest` -> PASS; full suite PASS
  (`dynamic-replication`, `dynamic-lifecycle`, `live-code`, `capability`,
  `gamemode-hot`, `movement-parity`, `hot-authoritative`, `entity-slice`).

## Classification

- SELFTEST PROVEN: render.ui buffering; ui.frame fails safe; hot HUD emits
  deterministic widget commands.
- COMPILED INTEGRATION: ui.frame domain in the engine loop; cold timer yield;
  hot match HUD composition.
- LIVE VISUAL PROVEN: none.
- HUMAN VERIFICATION NEEDED: visible hot HUD, live HUD edit, cold timer yield,
  mode package populating `MatchHudState`.

## Files changed

`src/hot-reload/game-api.h`, `src/live-code/live-ui.{h,cpp}` (new),
`src/live-code/live-behavior.cpp`, `src/engine/engine-tick-ui.cpp`,
`src/engine/engine-tick-ui-hud.cpp`, `src/hot-reload/hot-ui.h` (new),
`src/hot-reload/hot-modules.json`,
`src/hot-reload/modules/ui/hud.cpp` (new),
`src/network/hot-combat-selftest.cpp`,
`docs/architecture/live-development/hot-cold-audit.md`,
`docs/architecture/live-development/hot-kernel-next-steps.md`.

## Next cold owner selected

Populate `MatchHudState` from the mode package (smallest integration), then
player/NPC generic presentation, then animation/effects presentation.
