# Blender animation exporter specification

- Local time: 2026-10-18 14:17:04 EDT (America/New_York).
- UTC: 2026-10-18T18:17:04Z.
- Branch: `20261011zt`.
- HEAD before this documentation change: `f2e91866`.
- Result: `PASS_WITH_HUMAN_REVIEW`.

## Request

Write the full specification for getting Blender animation keyframes into
MiMITA and ask five questions to make the workflow more detailed. The user is
considering a Python script that asks for inputs or an in-Blender tool and
wants animation offsets authored relative to `plrOrigin`.

## Spec state and result

Expanded `docs/specs/animations/blender-animation-authoring.md` into a full
proposal. It now recommends a Blender add-on for the first workflow, with a
shared Python export core that could later support a Blender-background/CLI
entry point. The add-on selects the root, named Action, body-part map, clip ID,
marker/timing policy, and output. It reviews the choice and validates before
writing one shared-library animation JSON file.

The spec now explicitly says limb transforms are relative to `plrOrigin`, not
world space; removes actor root placement before deriving local pose; and
compares each sample against a normalized rest transform. The exact rest-pose
source and transform-delta convention remain open because they must agree with
the avatar GLB/runtime normalization contract. It follows the existing
`config/animations/library/` shared clip layout and leaves server/client clip
discovery open. Requirements, proposals, and open decisions remain distinct.

Before this change, the document was already a proposal and said: “**OPEN:**
exact file discovery/manifest owner; exporter implementation form (Blender
add-on or external project tool); Blender version support; basis and pivot
conversion; interpolation equivalence; frame/subframe/endpoint mapping;
whether live edits replace a running action's revision or apply on its next
start; scale policy; future armature/skinning and root-motion support.” After
this change it describes the UI, inputs/outputs, root-relative sampling model,
validation/failure behavior, report, acceptance sequence, and implementation
boundary. The exporter and runtime timeline integration are still not
implemented.

## Changed files

1. `docs/specs/animations/blender-animation-authoring.md`, lines 1–334.
   - Earlier wording (from the document before this turn): “**OPEN:** exact
     file discovery/manifest owner; exporter implementation form (Blender
     add-on or external project tool); Blender version support; basis and pivot
     conversion; interpolation equivalence; frame/subframe/endpoint mapping”.
   - New wording (line 72): “**Recommended first implementation: a Blender
     add-on.** A `.py` add-on installed in Blender can read the open `.blend`,
     selected rig, named Actions, evaluated scene, and timeline markers
     directly.”
   - Added audience/scope, shared library path, UI workflow, selection controls,
     `plrOrigin` relative transform contract, rest-pose open item, first-version
     inputs/outputs, validation/failure handling, and staged acceptance.

2. `docs/changelog/2026-10-18/20261018_181704-blender-exporter-spec.md`.
   - New file; no prior text. Records this request, the expanded proposal,
     instruction/review findings, validation, and remaining decisions.

Pre-existing dirty changes in this session include the earlier animation,
weapons, and Melee spec updates plus
`docs/changelog/2026-10-18/20261018_181131-animation-timeline-source.md`.
They were not modified by this follow-up. Other unrelated dirty source/config
files were preserved.

## Instruction and focused review

- Read `AGENTS.md`, `docs/ROUTER.md`, `docs/specs/animations/animations.md`,
  `docs/specs/avatar/avatar-editor.md`, `docs/specs/animations/blender-animation-authoring.md`,
  `docs/skills/documentation-checker-v1.md`,
  `docs/skills/spec-behavior-review-v1.md`,
  `docs/architecture/time-and-formatting/time-and-formatting.md`, and
  `docs/operations/task-completion/task-completion.md`.
- The router asks for `docs/doc-review-09-03-2026.md`, but that file is missing
  in this checkout. Recorded as an instruction/documentation warning; no
  substitute document was invented.
- Documentation Checker: `PASS_WITH_HUMAN_REVIEW`. Reviewed document purpose,
  ownership, cross-links, path alignment, and decided/proposed/open wording. The
  full-docs TODO scan found only the router/skill's own search instructions and
  historical changelog mentions, no actionable current spec TODO.
- Specification and Behavior Review: `PASS_WITH_HUMAN_REVIEW`. The exporter
  remains a proposal and is not claimed to work. Runtime basis/rest conversion
  and clip-registry details still need decisions and implementation evidence.
- No tests/build/runtime checks were run; this was documentation-only work.

## Validation and human review

- `git diff --check` passed for the documentation edits.
- Human review is needed on the five questions asked in the assistant response:
  initial UI form, rig-name mapping policy, normalized rest-pose source,
  authoring method for phase markers, and sparse versus per-tick sampling.
- Before implementation, confirm the exact Blender version, transform basis,
  runtime rest-pose math, clip discovery for server/client, and live revision
  behavior. No exporter or game-side marker timing is claimed implemented.
