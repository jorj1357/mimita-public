// 09 06 2026, 00 00
/* purpose
* define accessibility and portability goals for MiMITA
* keep controls, visuals, audio, and performance adaptable to different users
* make presentation and behavior configurable instead of hard-coded
* this file DOES NOT promise that every platform is supported today
* this file DOES NOT replace platform-specific build or certification documents
* this file DOES NOT require identical UI layouts on every screen size
*/

# Accessibility and Portability

## Core goal

MiMITA should be adaptable to as many users, devices, and platforms as
practical. The long-term portability target includes Windows, macOS, Linux,
iOS, Android, smart TVs, consoles, and legacy or constrained hardware where
the platform permits it. Platform support is an engineering goal, not a claim
that every target is currently built or certified.

Keep gameplay rules, configuration, input actions, rendering policy, and
platform adapters separated so the same game behavior can travel across
devices.

Resource efficiency is a core accessibility requirement. Use the lowest
practical CPU, GPU, memory, storage, and network resources for the behavior a
feature needs; avoid unnecessary work, allocations, retained data, and
transmitted data. Do not assume a player has a powerful GPU or CPU, abundant
RAM, a new device, or a fast and reliable internet connection. MiMITA should
remain responsive and useful on low-power devices and poor or unstable
connections, while scaling up on stronger systems. Optional detail and
nonessential work should yield before core gameplay and essential information.

MiMITA is one application with integrated gameplay, map, replay, video, and
future music-creation workspaces. This is a product-architecture direction,
not a promise that all platforms build the same interface today. Mobile and
console workspaces should use platform-appropriate input, navigation, storage,
and export paths while reusing portable project data where practical.

## Creator-tool accessibility

Creation workflows must have a quick path for common tasks and a discoverable
route to advanced controls. Selection feedback must make it clear which item
will be changed; clicking one timeline clip or property must not silently edit
its neighbor. Provide keyboard navigation, controller navigation where
supported, scalable readable text, visible focus, undo/redo, and clear save and
export status.

Video preview and export must degrade gracefully on lower-capability devices.
An export can take longer, but it must not make the editor appear frozen or
lose the project. Measure frame time, export throughput, memory, and storage
use rather than promising a universal frame rate.

## Input access

All important actions should use an abstract action layer rather than directly
depending on one device. Support, where the platform allows it:

- keyboard and mouse;
- controller and console layouts;
- touch and virtual controls;
- remapping and multiple bindings;
- hold, toggle, and timing alternatives;
- adjustable sensitivity, dead zones, and aim behavior;
- 9 6 2026 also VR support is huge huge huge i want that over time
- navigation without requiring a mouse.

The terminal command layer and the shared player/NPC action layer are useful
validation paths for this abstraction.

## Visual access

Visual presentation should be configurable through JSON and should support:

- color-blind-friendly palettes and selectable color filters;
- contrast, brightness, gamma, and readable text colors;
- adjustable UI scale, font size, spacing, and layout;
- non-color indicators for teams, damage, status, and warnings;
- subtitles and text equivalents for important audio cues;
- adjustable crosshair, hit markers, killfeed, healthbar, and notification
  visibility;
- reduced flashes, camera shake, motion, particles, and screen effects;
- disabling shadows, post-processing, lighting effects, and other expensive
  presentation features;
- quality presets plus individual settings for low-end hardware.

No important state should be communicated by color alone. Settings should be
editable, validated, saved per profile where appropriate, and inspectable from
the terminal.

## Audio access

Support independent volume controls, mute options, subtitles or text cues for
important events, and clear routing for music, effects, voice, notifications,
and accessibility sounds. Audio must not be the only way to understand a
gameplay-critical event.

## Performance and platform boundaries

Use capability detection and configuration rather than assuming desktop GPU,
resolution, refresh rate, filesystem, input, or window behavior. Keep gameplay
simulation deterministic and separate from rendering frame rate.

Choose designs and implementations for minimal practical resource use across
the supported device range. Measure CPU, GPU, memory, storage, and network
costs on representative low-power and high-power systems. Treat large or
unbounded resource growth, including during retries and reconnection, as an
accessibility and performance issue to investigate; do not assume high memory
or network capacity will hide it.

Settings should allow users to reduce or disable expensive features such as
shadows, post-processing, particles, lighting, animation detail, audio effects,
and cosmetic overlays while preserving gameplay information.

Platform-specific code belongs in adapters. Shared gameplay, configuration,
terminal commands, replay data, and test scenarios should remain portable.

## Validation requirement

Every new UI, input, audio, or rendering feature should answer:

1. Can it be used without a mouse?
2. Can important information be understood without color, sound, or motion?
3. Can its scale, visibility, intensity, or quality be configured?
4. Can its behavior be tested through terminal commands or a deterministic
   harness?
5. Does it degrade safely on lower-capability platforms?

Developer workflows are part of accessibility too. Common configuration
preset operations should be discoverable and should not require manually
editing several unrelated JSON files. A future shared preset interface may
provide list and load actions, but its exact commands belong to the terminal
command specification after the underlying preset ownership is established.

See the [Video Editor](../../specs/creator-tools/video-editor.md),
[Replay](../../specs/replay/replay.md), and
[Distribution](../../specs/distribution/distribution.md) specifications.
