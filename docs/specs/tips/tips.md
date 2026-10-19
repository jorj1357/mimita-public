// 2026-10-19
/* purpose
* define the current and desired behavior of player-facing gameplay tips
* specify an editable content format that can grow to a large library
* define optional text and card presentation such as size, color, and animation
* this file DOES NOT define notification behavior unrelated to tips
* this file DOES NOT claim that proposed rich-text or terminal styling exists
* this file DOES NOT choose final authoring tools or exact visual values
*/

# Gameplay Tips

## Audience and scope

This document is for people authoring tips and implementing their content and
presentation. A tip is a small, optional piece of game information shown in the
existing notification UI. It is not the proposed general event queue or event
history. Future event-feed work needs its own specification.

## Current behavior (repository evidence, 2026-10-19)

- `config/tips.json` is a JSON array of strings. `Tips::load()` reads it once
  and keeps only string entries in memory; it does not hot-reload the content.
- `NotificationSystem::updateTips()` schedules a tip when tips are enabled and
  the configured tick interval expires. `config/tipsconfig.json` owns the
  enabled flag and minimum/maximum ticks between tips. The interval is selected
  randomly in that range.
- A tip is selected randomly, with an attempt to avoid selecting the same
  index twice in a row. `pushTip()` labels it `TIP #<array index + 1>` and sends
  the string to the notification system. Empty strings count as loaded tips,
  but `pushTip()` drops an empty selection; the checked-in content has empty
  entries, so an empty selection can result in no visible tip for that turn.
- The notification system owns lifetime, typewriter reveal, wrapping, scroll,
  panel layout, and visibility. `config/notifications.json` owns global
  notification behavior; `config/gui/notifications.json` owns panel and
  element presentation. The message is drawn as wrapped plain text using one
  configured text color and font size. Critical and important notices have
  special colors; ordinary tips have no per-tip style.
- `src/devtools/terminal.cpp` stores terminal output as strings. In
  `src/devtools/terminal-render.cpp`, the terminal assigns color by checking
  textual prefixes (`[OK]`, `[ERROR]`, `] `, `[TERMINAL]`) and otherwise uses a
  common pale blue-gray. Command output has no typed style metadata, so host
  command responses receive no dedicated light-purple style. A command can
  happen to match a prefix rule, but that is not host-role styling.

These points describe source and checked-in configuration, not a live visual
acceptance run.

## Desired authoring and display behavior

Tips remain easy to write as plain text. A tip may optionally supply a small
set of presentation overrides. Missing fields inherit the notification/tip
defaults, so authors can style only the exceptional tip. The whole message may
use one style, or an author may mark text runs independently when a tip needs
mixed emphasis. Styling applies only to that tip and must not change gameplay,
notification priority, timing, or meaning.

Supported presentation capabilities should include:

- text size within documented minimum and maximum bounds;
- text color, including a named palette color or explicit RGBA value;
- whole-message rainbow cycling, with speed and phase defaults controlled by
  a shared effect preset;
- optional per-run color/effect when only part of a message needs emphasis;
- notification-card background and outline color overrides;
- readable wrapping, scrolling, clipping, and typewriter reveal for styled
  text, including when runs cross a line boundary.

Styles must preserve readable contrast. Animation is presentation-only and
must be bounded to the notification being rendered; it does not create a
separate timer or update loop for each character. At minimum, provide a static
fallback for unsupported or invalid effects. Do not animate every tip unless
its data requests an effect. Exact size limits, rainbow rate, and background
alpha remain `NEEDS_SPEC_DECISION` until chosen during implementation review.

## Large-library structure proposal

Target: a library that can grow to 100,000 or more tips without turning one
giant file into a fragile editing bottleneck. Keep the authoring data as the
source of truth; any lookup index is generated and rebuildable.

Proposed repository layout:

```text
config/tips/
  manifest.json             # schema version, shard list, defaults
  styles.json               # shared named palettes and effect presets
  shards/
    00.jsonl                # stable-ID records; one complete JSON object/line
    01.jsonl
    ...
    ff.jsonl
```

Each record has a stable ID independent of its current file or line, a topic
category, optional tags, eligibility metadata, content, and optional style
overrides. For example:

```json
{"id":"movement.double-jump.0001","category":"movement","tags":["air","jump"],"enabled":true,"weight":1,"content":"Press space while airborne to double jump.","style":{"textSize":"large","textColor":"accent.lavender","effect":"rainbow.soft"}}
```

JSONL means each physical line is one self-contained JSON record. Keep shard
files bounded (initial proposal: about 500–1,000 records per shard); split or
rebalance files with a maintenance tool when they grow beyond the target.
Authors can search by stable ID, category, or tags and change one record
without reformatting a 100,000-entry array. Category and tags remain record
metadata, not directory ownership, so a tip can be discoverable under multiple
topics without duplicate copies. Stable IDs are never reused after deletion.

`manifest.json` declares schema version, defaults, and the shard files to
load. `styles.json` owns shared style names and effect parameters so authors
can consistently request a lavender color or rainbow preset without copying
RGBA values into many records. Per-tip overrides may use a named style or a
validated inline value. Unknown fields, invalid colors, oversized text sizes,
unknown effects, and duplicate IDs should be reported with shard and line/ID;
one malformed record should not silently corrupt other records. Disabled or
empty records are excluded from selection.

At load, build one lightweight in-memory index of ID, shard/record location,
category, tags, enabled state, and weight. Load or decode the full content only
when needed if profiling shows that holding every rendered record is costly.
With 100,000 records, avoid scanning every full text record on each tip, making
one operating-system file per tip, or maintaining a hand-edited duplicate
index. A generated index must be disposable and rebuildable from the manifest
and shard files. Selection should choose uniformly among eligible records by
default, with optional weight only when explicitly authored; avoid immediate
repeats whenever at least two eligible tips exist.

The initial migration should preserve all existing nonempty tip text and
behavioral settings. Do not keep two active sources of tip content after the
migration. `config/tips.json` can be converted once and retired after parity
and loader validation.

## Host command output color

Host command responses in the in-game terminal should be light purple. This is
a role/semantic style, not a magic string prefix: preserve whether output came
from a host-authorized command when adding it to the terminal, then resolve a
named terminal palette style such as `host_command` in the renderer. Ordinary
client output, errors, success messages, and server chat keep their own
independent styles. Style metadata must survive line wrapping and scrollback;
it must not be inferred from translated or user-controlled message text.

The exact light-purple RGBA value should be selected against the terminal
background and checked for readable contrast. The requested color is for host
command output lines; whether command echo/input also changes color is
`NEEDS_SPEC_DECISION`.

## Content conventions

- Keep each tip self-contained, accurate, and useful without requiring a
  particular notification before or after it.
- Use stable IDs that describe topic and purpose, not the current wording or
  list position. Keep prose separate from metadata and styles.
- Use categories for the main subject and tags for cross-cutting concepts,
  game modes, audience, or prerequisites. Do not encode topic solely in IDs.
- Use shared named styles for common presentation. Inline overrides are for
  genuine one-off needs, not a second inconsistent palette.
- Do not use visual effects to communicate required meaning by color alone.
  Rainbow text is decorative; the text must remain legible against its card.
- Empty/disabled content is not selectable and should be surfaced by the
  authoring validator rather than becoming a silent blank notification.

## Validation and acceptance criteria for a future implementation

- Existing tip content and tip enablement/interval behavior are retained after
  migration, apart from excluding empty entries.
- Plain legacy-style content and fully styled content both render and wrap
  correctly; typewriter, fade, scroll, and panel clipping remain functional.
- A whole-message rainbow tip visibly cycles through the configured palette;
  static color, size, per-run style, and card background overrides work
  independently and inherit defaults when omitted.
- Invalid records and styles identify the exact source record and fall back
  safely without preventing valid records from loading.
- A 100,000-record library can load, filter/select, and report validation
  results within agreed startup and memory budgets. Those budgets are
  `NEEDS_SPEC_DECISION` and should be measured before implementation is
  considered complete.
- Host command lines render light purple because their output metadata marks
  the host-command role; unrelated output does not change color. Verify wrapped
  and scrolled host output as well as non-host output.
- Source/configuration validation, runtime behavior, visual review, and human
  acceptance are reported separately.

## Open decisions

- `NEEDS_SPEC_DECISION`: final typography bounds, named palette values, and
  rainbow speed/phase defaults.
- `NEEDS_SPEC_DECISION`: whether rich text is limited to whole-message style
  plus simple runs, or needs additional effects such as wave/shake.
- `NEEDS_SPEC_DECISION`: the initial shard size and measurable startup/memory
  budgets for 100,000 tips.
- `NEEDS_SPEC_DECISION`: whether tips should be filtered by game mode, player
  experience, platform, or other eligibility metadata in the first release.
- `NEEDS_SPEC_DECISION`: whether the command text entered by the host shares
  the light-purple style, in addition to host-command responses.

## Original author notes

The notes below are retained as context; the decisions above clarify their
current scope without treating the notes as implementation evidence.

> 2026 10 19 1824 jorj todo
>
> like a event queue likem inecraft so u can see whats going on
>
> but
> just for tips for now like this is how todo insert thing or that or this
> likeinfo about the game
>
> but need to dlike
> figure out how to sort this
