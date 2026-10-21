# Task

- Task ID: swordsword-spec-clarity
- Summary: Clarified the desired Swordsword contact, damage, animation, and player/NPC behavior in its owning spec.
- Status: PASS_WITH_HUMAN_REVIEW
- Date, time, timezone: 2026-10-21T00:22:39Z (2026-10-20 20:22:39 EDT, America/New_York)
- Branch: 20261011zt
- Base commit: f6d6eeb2ce409153c8d23933b32c77eee39a5b48
- Final commit: f6d6eeb2ce409153c8d23933b32c77eee39a5b48 (not committed by this work)

# Pre-existing changes

- Before this edit, `docs/specs/weapons/swordsword.md` already contained the user's request and a newly drafted SwordSword behavior contract in commit `f6d6eeb2` (`colision stuff npcs plrs etc`). This work added the audience, scope boundary, and compatible-avatar retarget requirement; it preserved all existing content.
- `config/accounts/default.json`, `config/analytics.json`, and `config/audio/music-settings.json` were already modified and were not changed.

# Specification alignment

- Changed: `docs/specs/weapons/swordsword.md`, Purpose and Weapon animations and input sections (current lines 12-19 and 49-51).
- The audience and `does not define` statement satisfy the documentation purpose/scope requirement. The retarget note preserves weapon identity as the animation selector while allowing compatible rigs to fit the clip.
- Related sources reviewed: `docs/specs/weapons/weapons.md` §9/§17-19, `docs/specs/networking/networking.md` actor identity in authoritative damage, `docs/specs/animations/animations.md` clip availability and format, `docs/specs/weapons/melee-weapons.md`, and `docs/architecture/collision/actor-vs-actor-collisions.md`.
- Focused skills: `docs/skills/spec-behavior-review-v1.md` — PASS_WITH_HUMAN_REVIEW; `docs/skills/documentation-checker-v1.md` — PASS_WITH_HUMAN_REVIEW. The behavior contract is explicit; numeric force/balance tuning and authored clips remain open.
- Source/spec gaps are described as source-review findings, not runtime facts: SwordSword contact is currently attack-timeline-gated on the server, local attack starts add wielder velocity, NPC SwordSword uses a hitscan approximation, and the weapon currently references HAFS poses.

# Exact change

- Before at `docs/specs/weapons/swordsword.md` after the Purpose paragraph: no audience or scope boundary text.
- After: added `**Audience:** gameplay and networking implementers, animation authors, and reviewers validating Swordsword behavior.` and `**This document does not define** the generic collision solver, network packet formats, animation file schema, or the general damage/team policy. It defines how Swordsword uses those shared systems.`
- Before in Weapon animations and input: no compatible-avatar retarget requirement.
- After: added `Avatar rigs may use a retarget map to fit a shared weapon clip to their body, but avatar identity does not choose a different weapon move set or prevent a compatible actor from using Swordsword animations.`

# Documentation review notes

- `docs/ROUTER.md` routes documentation review to `docs/doc-review-09-03-2026.md`, which is absent from this checkout; this task proceeded with the available documentation and focused skills.
- The repository-wide TODO scan found existing unrelated explain-this notes. Relevant ones: `docs/specs/weapons/weapons.md:1` (`2026 10 20 1655 jorj todo explain`) — expand the general weapon-system introduction; `docs/architecture/collision/actor-vs-actor-collisions.md:1` (`2026 10 08 1600 jorj todo explain how actors vs actor collisions work`) — add a short overview of shared actor collision ownership. Neither was edited.

# Validation and human review

- Reviewed final Markdown and verified all four relative specification links point to existing files.
- No tests, build, or runtime scenario were run; this task changed specification text only.
- Human visual/gameplay acceptance remains required after the clips are authored and implementation is validated. Force curve, damage/knockback bounds, sound pitch/volume ranges, and the shared double-click window need tuning decisions.
