// 2026-09-10T18:49:13Z
/* purpose
* record the always-visible "press / to chat..." chat input affordance
* preserve exact files, lines, build status, and unverified runtime claims
* connect the work to the chat specification and chat-checker skill
* this file DOES NOT replace the feature record
* this file DOES NOT claim visual acceptance that did not happen
*/

# Task

- Task ID: chat-hint-placeholder
- Summary: Make the chat input bar always render so the configured
  `press / to chat...` placeholder is visible as a standing affordance, and
  keep the placeholder visible while the field is focused until text is typed.
- Status: Implemented; build verified; runtime/visual review pending
- Date, time, timezone: 2026-09-10T18:49:13Z, ISO 8601
- Branch: 8292026stash
- Base commit: e9a78f1
- Final commit: uncommitted

# Pre-existing changes

- `git status --porcelain` before this session included modifications to
  `config/accounts/default.json`, `config/analytics.json`,
  `config/gamemodes/retrograd.json`, `config/gameplay.json`,
  `config/ragdoll.json`, `docs/regressions/regressions-v1.md`, `src/camera.*`,
  `src/engine/engine-tick-camera.cpp`, `src/physics/physical-body.*`,
  `src/ragdoll/*`, and three untracked ragdoll changelogs.
- Files not created or modified by this session: all of the above.
- This session created `docs/changelog/2026-09-10/20260910_184913-chat-hint-placeholder.md`.

# Requested behavior

- Add "press / to chat..." to the small chat input box.
- The box should be visible as a standing affordance even when the chat window
  is idle/faded and chat is closed.
- While the field is focused and empty, keep showing the hint until a
  character is typed.

# Specification alignment

- Current specification paths: `docs/specs/ingame-chat/ingame-chat.md`,
  skill `docs/skills/chat-checker-v1.md`.
- Exact requirements: section 6 says pressing `/` opens chat and a clear
  blinking cursor appears; sections 18/39 require the chat window appearance,
  including text and colors, to be JSON editable.
- Why the change follows the specification: the hint text and color continue
  to come from the existing `chatBar` element in `config/gui/hud.json`; no new
  owner or duplicate UI path was introduced.
- Conflicts or decisions: section 19 states the whole box fades together. Per
  explicit human direction this session, the input affordance bar intentionally
  stays visible when idle, so it no longer fades with the message window. This
  is a deliberate, documented deviation.

# Exact implementation changes

## File: `src/gui/ui-text-input.h`

- Lines/functions/headings: `struct UITextInputOptions`.
- Old content or behavior: only `placeholder` and `placeholderColor`.
- New content or behavior: added `bool placeholderWhileFocused = false;` and
  `bool interactive = true;`.
- Reason: allow the chat field to keep its placeholder while focused and to be
  drawn without mouse/click focus changes.
- Why unrelated behavior is preserved: both defaults keep existing text-input
  callers unchanged.

## File: `src/gui/ui-text-input.cpp`

- Lines/functions/headings: `uiTextInputRender`, placeholder branch and mouse
  handling block.
- Old content or behavior: placeholder only drawn when `!state.focused`; mouse
  handling always ran.
- New content or behavior: placeholder condition is
  `if (!state.focused || opts.placeholderWhileFocused)`; a caret is drawn over
  the placeholder when focused; the entire mouse-handling block is wrapped in
  `if (opts.interactive)`.
- Reason: keep the hint visible until text is typed, and keep the idle hint bar
  non-interactive while preserving interactive behavior for the open chat and
  all other inputs.
- Why unrelated behavior is preserved: `interactive` defaults to true and
  `placeholderWhileFocused` defaults to false, so terminal, avatar editor, and
  menu inputs behave exactly as before.

## File: `src/gui/hud/chat-window.cpp`

- Lines/functions/headings: `renderChatWindow`.
- Old content or behavior: `if (state.backgroundOpacity < 0.005f) return;`
  early-out; the input field block was guarded by `if (state.open)` and only
  rendered while chat was open.
- New content or behavior: the early return is replaced by
  `const bool showMessageArea = state.backgroundOpacity >= 0.005f;`; the
  background, message area, and typing indicator are guarded by that flag; the
  input/hint bar is always rendered and calls `uiTextInputRender` with
  `opts.interactive = state.open` and `opts.placeholderWhileFocused = true`.
- Reason: make the hint box a persistent affordance while keeping message/fade
  work skipped when idle.
- Why unrelated behavior is preserved: `chatwindow 0` still hides everything
  through the existing `if (!gChatWindowVisible && !state.open) return;` gate;
  message scrolling, fade, VIP name styling, and typing indicators are
  unchanged when the window is visible.

# Diagnostics

- Owner/category: `Debug::Category::Chat`.
- Input: existing `chat-debug-input-render` and `chat-hud-render` throttled logs.
- Decision: no new logging added; existing logs now emit while the bar is drawn
  closed as well.
- Output: unchanged log format.
- Failure or rejection reason: n/a.
- Rate limiting: existing `Debug::logThrottled` intervals retained.

# Validation

- Focused skill paths and results: `docs/skills/chat-checker-v1.md` traced
  input to render; source-level only.
- Tests and exact commands: `python build_agent.py` from `C:\mimita-priv-v8`.
- Build status: SUCCESS (`=== BUILD CHANGELOG === ... Status: SUCCESS`,
  `build/changelog.txt`, return code 0, 22 compiled, 24.59 s).
- Runtime or hot-reload evidence: none recorded this session.
- Output files: `C:\mimita-priv-v8\mimita.exe`.

# Measured evidence

- Before values: input bar rendered only when `state.open`; placeholder rendered
  only when unfocused.
- After values: input bar rendered every frame the HUD chat path runs;
  placeholder rendered while empty regardless of focus for the chat field.
- Timestamps: build at 2026-09-10T18:49:08Z.
- Tick/frame/network measurements: none.

# Regression review

- Regression entry appended: no
- Why this is or is not a confirmed regression: no previously working behavior
  was observed to break; this is a new affordance and a deliberate fade-behavior
  deviation.
- Related regression paths: `docs/regressions/regressions-v1.md`.

# Human acceptance

- Visual review: not performed.
- Gameplay review: not performed.
- Multiplayer review: not performed.
- Still unverified: hint box visible at rest; hint persists while focused until
  typing; hidden by `chatwindow 0`; no unintended focus/mouse capture when
  closed.

# Related feature record

- Feature path: none created this session.
