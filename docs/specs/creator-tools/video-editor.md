# In-App Video Editor

**Purpose:** specify a built-in editor for turning MiMITA replays and user
media into finished videos.
**Audience:** replay, rendering, audio, UI, accessibility, and content-tool
contributors.
**Status:** proposed product specification; implementation and performance
acceptance are pending.
**Does not:** promise parity with every professional editor, publish content
automatically, or require users to sign in or upload.

## Product goal

MiMITA has one creator application inside the game. The video editor, replay
editor, map editor, and future audio tools are integrated workspaces in the
same product, not separate MiMITA Studio, DAW, and video-editor applications.
Platform builds may have different input layouts and capabilities while
sharing project formats and core workflows where practical.

A new user should be able to make a simple, shareable clip from a replay in
about five minutes. Deeper tools should be available without making the quick
path harder to discover.

## Project contents

A video project can refer to:

- one or more MiMITA replay ranges;
- imported video and still images;
- music, sound effects, and recorded or generated voice;
- titles, subtitles, and optional speaker labels;
- camera paths, field-of-view changes, transitions, and visual effects;
- output dimensions, frame rate, and quality settings.

The editor should preserve source files and replay data. Edits are
non-destructive project instructions until the user exports a finished file.
Projects should report missing media clearly and let the user relink it.

## Editing experience

- Provide a timeline with selectable, draggable clips and clear track labels.
- Make the visible selection match the item that receives keyboard, pointer,
  or controller actions. A click must not activate a nearby control.
- Support trim, split, reorder, audio level, fades, titles, captions, and
  preview. Undo and redo cover edits to the current project.
- Let users select, move, and keyframe replay cameras and supported effects
  without changing the recorded gameplay.
- Keep presets editable. The built-in MiMITA effects and exports are free to
  use. List included effects and disclose any external dependency or cost
  before the effect is applied or exported.
- Make subtitle timing and speaker-label visibility independently adjustable.

## Replay and media integration

Replay editing is the source of game-state footage; imported media is an
additional source. The project can combine both on one timeline. Replay
playback and export must reuse the shared replay, animation, audio, and effects
owners wherever possible rather than creating export-only copies.

The editor can consume a generated voice line from
[Live Voice Generation](../live-voice-generation/live-voice-generation.md).
It does not turn proximity voice chat into editable character dialogue.

## Rendering and responsiveness

Preview, editing interaction, and final export are separate workloads. Long
exports run without blocking the editing interface and report progress,
completion, or a useful failure reason. Where supported, exports can continue
without rendering every frame into a visible game window.

The 240-FPS-on-low-end-hardware idea is an aspiration to investigate, not a
universal promise. Keep gameplay simulation at its specified fixed tick rate;
video output frame rate is an export setting and must not change recorded
gameplay. Measure preview frame time, export throughput, memory use, and
quality on named representative hardware before making performance claims.
Use reduced-quality preview when needed while preserving final export quality.

## ClipForge integration

The user-provided ClipForge CMD folder is a source of workflow and rendering
ideas. Before integration, inventory every file, preset, media dependency,
configuration input, and automation behavior. Reuse or port the local
render-plan, trimming, overlay, and encoding behavior that fits this spec; do
not make the in-game editor depend on a second user-facing Python application.

Keep external posting explicit and user initiated. Browser-profile selection,
account tokens, automatic posting, daily streaks, and repeated upload reminders
do not become default game behavior. Do not copy credentials or account
configuration into MiMITA or the repository. Any later platform upload
integration requires its own opt-in, privacy, and platform-policy design.

## Accessibility and acceptance

The editor must support keyboard navigation and controller navigation where
the platform allows it. Provide readable hit targets, scalable text, captions,
independent audio controls, reduced-motion options, and clear focus/selection
feedback.

Acceptance should include:

1. A first-time user creates a simple clip from a replay and exports it without
   an account or upload.
2. A project combines replay footage, an imported clip, music, a title, and a
   caption, then reopens with the same edits.
3. Missing media, failed export, and a long-running export are explained
   without freezing editing input.
4. The user can see which effects are included and any cost or dependency
   before use.
5. Pointer, keyboard, and controller selection do not apply edits to a
   neighboring control.

## Related specifications

- [Replay](../replay/replay.md)
- [Distribution](../distribution/distribution.md)
- [Accessibility and portability](../../architecture/accessibility/accessibility.md)
- [Live voice generation](../live-voice-generation/live-voice-generation.md)
- [MiMITA DAW direction](../non-game-specs/DAW/mimita-daw.md)
