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
