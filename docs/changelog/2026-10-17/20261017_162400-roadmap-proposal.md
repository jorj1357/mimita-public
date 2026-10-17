# Roadmap proposal review copy

- UTC: 2026-10-17T16:24:00Z
- Local time: 2026-10-17 12:24:00 EDT
- Branch: `20261011zt`
- HEAD before this documentation change: `7510918a164aafe4be1eb5d5a119a24f39f396b1`
- Scope: create a separate proposed roadmap based on the user's current
  direction. No existing roadmap, vision, spec, code, or runtime state was
  changed by this session.

## Changed files

### `ROADMAP-PROPOSAL.md` (new, 212 lines)

There was no prior file or prior text.

Added at lines 1-3:

> `# MiMITA Roadmap — Proposed Review Copy`
> `**Status:** Proposal for discussion. This file does not replace `ROADMAP.md` or `VISION.md`.`

Added at lines 54-58:

> `## Proposed next release: v2.1.0 — Zombie Tower`
> `**Goal:** deliver one complete Zombie Tower experience that players can join, understand, play through, and want to replay. This is the release's center of gravity; other modes and platform ideas should not expand this scope by default.`

The new document includes v2.1.0 proposed scope, dependency order, observable
completion criteria, proposed later candidates for Infinite Dungeon Slayer and
Payload, an unassigned long-range backlog, and decisions to settle before
adopting the proposal. Candidate releases are explicitly not commitments.

### `docs/changelog/2026-10-17/20261017_162400-roadmap-proposal.md` (new)

There was no prior file or prior text. This file records the proposal and its
review evidence.

## Source and rationale

- The user asked for a separate clone/proposed version of the roadmap and
  supplied current intent. `ROADMAP.md` already had a pre-existing uncommitted
  addition at its top (`2026 10 17 1150 jojr todo make this better idk hwo to
  even bro`); it was preserved and not edited.
- `VISION.md` was already present as an untracked user-authored file; it was
  read and left untouched.
- GitHub's release page lists v2.0.7 as a pre-release and marks v2.0.6 as
  latest. The proposal states that distinction rather than describing v2.0.7
  as the latest official release:
  https://github.com/jorj1357/mimita-public/releases
- `docs/specs/gamemodes/zombie-tower.md` calls itself a draft with authoritative
  target behavior pending human edits. The proposal therefore makes spec review
  the first gate rather than claiming the Tower scope is already settled.
- The detailed open issue index already includes evidence and acceptance for
  the reported map/spawn and weapon/equip issues. The roadmap links to that
  index instead of duplicating its technical detail.
- Existing persistence, animation, avatar-editor, and NPC movement documents
  are referenced as owners. The proposal asks to verify/reuse them before
  adding new systems.
- Recommendation: keep v2.1.0 centered on one complete Zombie Tower run;
  treat Infinite Dungeon Slayer and Payload as suggested future releases;
  leave vehicles, destructible worlds, broader tools, distribution/social,
  decentralized storage, and economy without release numbers until their first
  slices and decisions are known.

## Focused review

- Followed `AGENTS.md` and `docs/ROUTER.md`.
- Read `docs/skills/documentation-checker-v1.md` and
  `docs/skills/spec-behavior-review-v1.md`.
- Documentation review: the proposal states its purpose, audience, and
  boundaries; candidate work is distinguished from promises; internal Markdown
  links were checked and all resolve.
- TODO scan: directly relevant existing TODOs remain untouched:
  - `docs/specs/gamemodes/zombie-tower.md:5363` —
    `2026 10 06 1603 jorj todo - explain zombie tower`. Suggested replacement:
    clarify which Tower behavior needs explanation and link shared behavior to
    its owner spec.
  - `docs/architecture/player-npc-systems/npc-movement.md:22` —
    `9 8 2026 1019 est jorj todo - explain`. Suggested replacement: describe
    the intended reusable NPC movement/authoring behavior and link its
    specification.
  - `docs/specs/avatar/avatar-editor.md:5` —
    `also todo clean this doc!!!!!! cant have so much stuff inthis doc just spam`.
    Suggested replacement: identify duplicate or historical sections to move
    and define the current Avatar Editor spec boundary before reorganizing it.
  - `docs/specs/monetization/monetization.md:1` —
    `2026 10 17 1141 jorj todo explain  tix and`. Suggested replacement:
    define the intended meaning and use of tickets before adding them to a
    release plan.
- The repository-wide text scan also found TODO notes elsewhere in docs;
  unrelated historical and feature TODOs were not changed. Existing indexes
  include `docs/changelog/2026-10-16/20261016_082206-gameplay-spec-issue-map.md`.
- Validation: relative Markdown link check passed (212 proposal lines scanned).
  No tests, build, game session, or runtime scenario was run because this was a
  documentation-only proposal.
- Human review still needed: approve or revise the proposed v2.1.0 boundary,
  Tower completion criteria, exact persistence requirements, networking gates,
  and whether v2.2.0/v2.3.0 are useful version placeholders.

## Workspace preservation

The workspace contained other pre-existing modifications and untracked files,
including source, dev-loop, networking, issue-index, release-note, and vision
work. They were not changed or attributed to this session. `ROADMAP.md`'s
existing user edit was preserved.
