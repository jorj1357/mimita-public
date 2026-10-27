# MiMITA Vision and Mentality

**Purpose:** record the long-term product and project values that should guide
architecture, game design, creator tools, governance, and future experiments.

**Audience:** the project owner, contributors, players, and community members
who shape MiMITA's direction.

**Does not:** promise that every idea below will ship, replace feature
specifications, settle technical feasibility, or establish a final governance
or monetization policy.

## MiMITA as an open, persistent universe

MiMITA is not only a game. The aspiration is for it to become almost its own
universe: a place to play, socialize, make things, learn how they work, and
remix them. People should be able to inspect a thing and reach toward its
underlying rules and building blocks, gaining more control and room for
meaning, purpose, and expression.

The long-term ambition includes experiences and tools such as:

- a multiplayer hangout and VR experiences;
- 2D, 3D, 4D, and potentially higher-dimensional game creation;
- software, art, and music production tools, including a DAW;
- editable games and systems, where a player can pause a Counter-Strike-like
  match, enter an editor, and add something like a vehicle or rocket launcher
  while keeping the existing style;
- a Rocket-League-like game whose networking can be inspected and improved;
- a Roblox-like creation ecosystem where games are open source in the main
  official MiMITA experience and can be forked or remixed;
- live collaboration for music and other creation, in the spirit of FL Studio
  collaboration; and
- eventual operation across PCs, mobile devices, consoles, older computers,
  Linux, Mac, Apple devices, and other platforms where practical.

These are direction-setting examples, not a delivery list or compatibility
promise. A genuinely new domain must be allowed to have the right coordinates,
time model, interaction model, and runtime owner; for example, a four-
dimensional or node-based system may not use XYZ transforms or a fixed tick
rate. Follow the preferred implementation order in
[`../code-ownership/code-ownership.md`](../code-ownership/code-ownership.md):
remove unnecessary constraints, strengthen existing owners where their
contracts fit, and add a new owner when the current architecture cannot
represent the idea honestly.

## Durable foundations

The owner wants MiMITA to persist for an extraordinarily long time, ideally
indefinitely. Hydrogen and helium lasting since the Big Bang are a metaphor for
that ambition: MiMITA should become durable and composable enough to support
future purposes not yet imagined, from entertainment and creative tools to
other useful systems. “Atomic” means clear, reusable building blocks that can
be assembled into larger capabilities, not needless fragmentation or an
assumption that literal indefinite operation can be guaranteed.

Design for practical durability: understandable ownership, replaceable parts,
documented interfaces, compatibility decisions, recoverable data, and
maintainable operation. Avoid making the project dependent on one opaque
service or one person where a healthy alternative is feasible. New systems
should make it easier to extend or replace their parts without losing the
useful work built on them.

## Creative work and public value

MiMITA should be fun and useful enough that people want to share it because of
the value they experience, not because they are pressured by referral rewards
or growth tricks. The project should help people create and connect, and give
them meaningful control over what they use and contribute.

The owner is concerned about economic pressure and platform enshittification.
If MiMITA gains attention, users, or commercial offers, the project should
lead by example and keep its core values rather than changing direction simply
because growth or money makes that attractive. This requires transparent
tradeoffs and accountable decisions, not a claim that the project has already
solved those problems.

## Community governance and forkability

The intended direction is to make moral principles and monetization
understandable and public, and to involve as many affected people as practical
in important decisions. Major changes should have a clear proposal, visible
rationale, and an agreed community decision process. People who disagree
should be able to clone or fork MiMITA and build a version that reflects their
own choices, subject to the applicable licenses and law.

An early, unresolved governance sketch proposes a protocol, a role ladder
(owner/starter, an unclear “chronologically” role, administrators, super
moderators, moderators, trusted and active members, regular members, and a
punished state), a daily random-number event, and group agreement before
changing established rules. The meaning of the chronological role and the
purpose of the daily random number are not defined. The owner also imagines
asking the community to weigh in on consequential changes. These are prompts
for design, not adopted roles or voting rules. Define participation, privacy,
security, accessibility, abuse resistance, decision thresholds, emergency
changes, and how rules can evolve before implementing governance. Avoid making
people unable to play until they answer a proposal unless that is later
explicitly chosen through a clear and inclusive process.

## Sustainable stewardship

MiMITA should remain community-oriented while also allowing its human creator
to earn enough money to live and spend more time on the project. The owner is
still deciding whether to write monetization principles before implementation
or develop them alongside the product. Either way, make values, incentives,
ownership, and material tradeoffs public as they arise; do not let monetization
become opaque merely because the creator leads the project.

The specific funding model, community decision rights, and relationship
between official and forked versions remain open. This vision calls for
transparent deliberation rather than prescribing a business or legal
arrangement.

## Art direction: intentional roughness

MiMITA does not need to pursue realism or smooth minimalism as its default
measure of quality. The owner likes its rough, crunchy, sometimes janky
character, including dense text and a small window, and wants to preserve that
identity intentionally. Models and graphics may become more capable while
remaining expressive and handmade rather than realistic for realism's sake.

Icons may be designed as large, busy community-made images that become
crunchy, odd, and recognizable when reduced to a small app icon. Polishing an
asset should support MiMITA's personality and readability, not erase it.
Specific choices belong in the relevant visual and interface specifications.

## Possible future directions

The following are exploratory examples for future prototypes, not commitments:

- music or MIDI, story experiences, and generated live voices in the game;
- long-form video workflows, including scripts, character introductions,
  edits, and movies;
- cozy or trippy modes, disaster survival, voice-chat social spaces, and
  fractal exploration;
- small social games such as chess, checkers, Connect Four, boxing, and
  possibly wrestling;
- a space or relativity-inspired experience, such as firing a huge weapon at
  the Moon and simulating its fragmentation;
- infinite-world generation with strong inventory and building systems, using
  Minecraft-like play as a benchmark for how complete those systems should be;
- a 2012-style RPG loop where defeating monsters earns gold for better items,
  which would depend on robust progression storage; and
- exploring whether MiMITA could eventually grow into an operating system.

Prototype these in bounded slices and learn from actual use. Their feasibility,
priority, safety, moderation, licensing, and relationship to the core game
remain to be decided.

## How to use this vision

Use this document to explain why the project values openness, composability,
durability, creative control, community participation, and an intentional
visual identity. It can guide architecture proposals and help choose
experiments. Each feature still needs its own owner, behavior specification,
technical design, and acceptance evidence. Revisit this vision with the
community as MiMITA and its users grow.
