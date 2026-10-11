# Recoil specification plan

Date: 2026-10-11

## Result

Expanded `docs/specs/recoil/recoil.md` from an informal Heavy Shotgun note into a complete design specification.

The specification now defines:

- the Heavy Shotgun's intended anti-aircraft-cannon feel;
- per-shot impulse, accumulation, recovery delay, and recovery rate;
- separate camera, first-person viewmodel, physical-arm, and gameplay self-impulse channels;
- additive recoil composition during ADS;
- proposed weapon-owned JSON configuration;
- ownership boundaries and phased implementation;
- structured diagnostics and live-journal evidence;
- acceptance criteria and unresolved human tuning decisions.

## Evidence

- Source/document evidence: the specification was written to `docs/specs/recoil/recoil.md`.
- Build evidence: not applicable; no source code was changed.
- Runtime evidence: not collected; this was a documentation/design pass.
- Human visual acceptance: still required after implementation.

## Specification review findings

- The previous note was an unclear informal requirement. It did not define whether recoil was camera-only, viewmodel-only, physical-arm, or gameplay recoil.
- The new document separates those channels and explicitly prevents one channel from silently owning the others.
- Existing repository evidence confirms current camera and accumulation caps, plus a viewmodel fire-animation trigger that can remain true while accumulated recoil is nonzero. These are recorded as implementation inputs, not claimed fixes.

## Documentation checker result

- The changed document has a purpose, audience, explicit DOES NOT section, ownership, implementation phases, and acceptance criteria.
- `docs/ROUTER.md` routes documentation review to `docs/doc-review-09-03-2026.md`, but that file is absent in the current checkout. This is an existing routing/documentation warning and was not changed in this session.
- The repository-wide TODO scan found pre-existing TODO notes in multiple documents, including `docs/specs/weapons/weapons.md`. They were not edited because this task was limited to the recoil specification.

## Skill

Reviewed with `docs/skills/spec-behavior-review-v1.md` and `docs/skills/documentation-checker-v1.md`.

