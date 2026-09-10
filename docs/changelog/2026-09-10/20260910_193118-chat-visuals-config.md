// 2026-09-10T19:31:18Z
/* purpose
* record config-driven typing indicators and chat bubbles in config/gui/hud.json
* capture the networking heartbeat, tick-based fade, and local-player changes
* connect the work to the chat specification and chat-checker skill
* this file DOES NOT replace the feature record
* this file DOES NOT claim visual or multiplayer acceptance that did not happen
*/

# Task

- Task ID: chat-visuals-config
- Summary: Make the above-head typing indicator, in-chat typing line, and
  above-head chat bubbles configurable from `config/gui/hud.json` (size,
  distance/time fade in ticks, colors, toggles), add a typing heartbeat so
  long typing does not time out, and show the local player in the chat typing
  line.
- Status: Implemented; build verified; runtime/multiplayer review pending
- Date, time, timezone: 2026-09-10T19:31:18Z, ISO 8601
- Branch: 8292026stash
- Base commit: e1301a8
- Final commit: uncommitted

# Pre-existing changes

- `git status --porcelain` for this session's files is listed under
  "Exact implementation changes". Unrelated pre-existing modifications
  (ragdoll, camera, physics, config, regressions) were present before this
  session and were not touched.
- Files not created or modified by this session: all pre-existing modified
  files listed by `git status` outside this task's file list.

# Requested behavior

- The typing indicator should be config-driven from `config/gui/hud.json`
  with size, distance fade, time fade in ticks, above-head toggle, and
  in-chat toggle.
- Keep the above-head bubble typing indicator and above-head chat bubbles.
- Show a typing indicator in chat for every typing player.
- Make it work in online play (heartbeat so long typing persists).

# Specification alignment

- Current specification paths: `docs/specs/ingame-chat/ingame-chat.md`,
  skill `docs/skills/chat-checker-v1.md`.
- Exact requirements: section 16 (typing true/false on open/Enter/Escape,
  optional heartbeat, dedupe), section 17 (world typing bubble, JSON
  configurable, hidden when typing ends/timeout, frame-independent blink),
  section 20 (UI tick clock), section 39 (JSON + hot reload).
- Why the change follows the specification: typing state still uses the one
  `ChatTypingStateRequestPacket`/`ChatTypingStateEventPacket` path; visuals
  read hot-reloadable `hud.json`; blink and fade use the fixed 60 Hz UI tick
  clock instead of `glfwGetTime()`/milliseconds.
- Conflicts or decisions: the plan proposed `showSelfAboveHead:false`; the
  shipped `hud.json` uses `true` to preserve the existing local above-head
  dots. Distance fade clamps to 0.0 (fully fades) instead of the healthbar's
  0.25 floor. Keys are camelCase to match `hud.json` while copying the
  healthbar start/end/max distance semantics.

# Exact implementation changes

## File: `src/gui/gui-layout.h`

- Lines/functions/headings: `struct GuiElement`.
- Old content or behavior: had generic text/layout fields only.
- New content or behavior: added `maxDistance`, `fadeStartDistance`,
  `fadeEndDistance`, `fadeTicks`, `timeoutTicks`, `blinkTicks`,
  `heartbeatTicks`, `heightOffset`, `nameFontSize`, `lineHeight`, `maxWidth`,
  `maxItems`, `durationBaseTicks`, `durationPerCharTicks`, `showAboveHead`,
  `showInChat`, `showSelfAboveHead`, `showSelfInChat`.
- Reason: hold config for the world/chat visuals.
- Why unrelated behavior is preserved: new fields default to zero/false and
  are unused by other elements.

## File: `src/gui/gui-layout.cpp`

- Lines/functions/headings: `parseElement` and `GuiLayout::save`.
- Old content or behavior: did not parse or write the new fields.
- New content or behavior: parses all new fields in `parseElement` and writes
  the non-default ones in `save`, so the GUI editor keeps them.
- Reason: hot-reload and editor round-trip.
- Why unrelated behavior is preserved: all reads use `value(key, default)`.

## File: `config/gui/hud.json`

- Added `typingIndicator` (`type: chat_typing`) and `chatBubble`
  (`type: chat_bubble`) elements with the values described above.

## File: `src/gui/hud/chat-bubble.h` / `chat-bubble.cpp`

- Added `TypingIndicatorConfig` / `ChatBubbleConfig`, `getTypingIndicatorConfig()`
  and `getChatBubbleConfig()` reading `config/gui/hud.json`.
- `ChatMessage` now stores `durationTicks`/`ageTicks`; `addChatMessage`
  computes duration from `durationBaseTicks`/`durationPerCharTicks`;
  `updateChatBubbles(state)` advances one tick per fixed combat tick.
- `renderChatBubbles` uses configured size, padding, max width, line height,
  max items, height offset, colors, and healthbar-style distance + tick fade.
- `renderTypingIndicator(player, camera, isLocal)` uses configured size,
  height offset, max/start/end distance fade, `timeoutTicks`, `fadeTicks`,
  color, and a fixed-tick blink; honors `enabled`/`showAboveHead`/
  `showSelfAboveHead`.
- `computeChatDuration` (seconds) kept for replay effect lifetime.

## File: `src/gui/hud/chat-window.h` / `chat-window.cpp`

- `ChatWindowState` gained `lastTypingSentUiTick`.
- Added `updateChatTypingHeartbeat(state, clock)`: while chat is open it
  refreshes `THE_PLAYER.isTyping`/`typingStartedMs` (fixes the local 5 s
  timeout) and resends the typing request every `heartbeatTicks`.
- The in-chat typing line is now config-driven, gated by `showInChat`, and
  includes the local player when open and `showSelfInChat`.

## File: `src/engine/engine-tick.cpp`

- Calls `updateChatTypingHeartbeat(gChatWindowState, gChatUiTickClock)` right
  after the UI tick clock advances.

## File: `src/engine/engine-tick-combat.cpp`

- `updateChatBubbles` calls updated to the tick-based signature (no `dt`).

## File: `src/engine/engine-tick-ui-game-hud.cpp`

- `renderTypingIndicator` calls now pass `isLocal` (true for the local player,
  false for remote players).

# Diagnostics

- Owner/category: `Debug::Category::Chat` (existing open/close/typing logs).
- Input: existing throttled chat logs remain.
- Decision: no new logs added; heartbeat does not log per send.
- Output: unchanged.
- Failure or rejection reason: none.
- Rate limiting: heartbeat interval is `heartbeatTicks` (hud.json).

# Validation

- Focused skill paths and results: `docs/skills/chat-checker-v1.md` traced
  input to packet to history to render; source-level only.
- Tests and exact commands: `python build_agent.py` from `C:\mimita-priv-v8`.
- Build status: SUCCESS (`Status: SUCCESS`, return code 0, 9 compiled,
  11.07 s). An unrelated externally started build at 15:27:17 reported FAILED
  with no captured output; the session build afterwards succeeded.
- Runtime or hot-reload evidence: none recorded this session.
- Output files: `C:\mimita-priv-v8\mimita.exe`.

# Measured evidence

- Before values: typing timeout 5000 ms, distance 60, scale 0.28/0.35, dots
  blinked from `glfwGetTime()`, all hardcoded.
- After values: config-driven; `timeoutTicks=300`, `heartbeatTicks=120`,
  `blinkTicks=30` defaults in `hud.json`.
- Timestamps: build at 2026-09-10T19:31:06Z.
- Tick/frame/network measurements: none.

# Regression review

- Regression entry appended: no
- Why this is or is not a confirmed regression: source-only changes with a
  successful build; no human-observed break recorded. `updateChatBubbles` now
  assumes one call per fixed 60 Hz combat tick, matching the runtime invariant.
- Related regression paths: `docs/regressions/regressions-v1.md`.

# Human acceptance

- Visual review: not performed.
- Gameplay review: not performed.
- Multiplayer review: not performed.
- Still unverified: above-head dots persist for local and remote long typing;
  in-chat line lists local and remote names; bubbles honor configured size/
  distance/tick fade; editing `hud.json` hot-reloads the changes; toggles
  (`showAboveHead`, `showInChat`, `showSelf*`) work online.

# Related feature record

- Feature path: none created this session.
