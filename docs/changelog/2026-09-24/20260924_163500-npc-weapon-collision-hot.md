# NPC weapon collision hot slice

- EST timestamp: 2026-09-24 16:35:00
- Branch: current working branch
- Result: PASS_WITH_HUMAN_REVIEW
- Focused skill: `docs/skills/spec-behavior-review-v1.md`

## Scope

Used `afad20a` as the behavior reference for physical body-part/tool contact
and edited only the existing hot actor movement module. Pre-existing working
tree changes were preserved and not attributed to this session.

## Change

File: `src/hot-reload/modules/actor-movement-system.cpp`,
`resolveActorCollisions()` after the NPC limb collider loop.

NPC hot movement now submits an authoritative weapon collider to the existing
`collision.main` ABI. It reads `rightArm` and `weapon_edge` sockets, creates an
oriented weapon capsule when both are available, and falls back to an
authoritative grip sphere when the edge socket is unavailable. The collider
uses `COLLISION_PART_WEAPON`, `COLLISION_POLICY_WEAPON`, and the existing
fixed-tick collision solver/contact-reset path.

## Evidence

- `git diff --check`: passed; only line-ending warnings for pre-existing files.
- `python devscripts/live-build.py`: produced a new hot DLL
  `build/hotreload/mimita-live-g000030.dll`.
- The running EXE was not closed, restarted, relinked, replaced, or unlocked.
- Source/build activation evidence does not prove visible gameplay parity.

## Still required

Human live acceptance must verify NPC traversal, limb contact, weapon contact,
root correction, grounding, dash/down-dash reset, weapon poses, and rollback
after an intentionally invalid hot candidate. Full NPC/player `afad20a` parity,
exact mesh-part sampling, and all weapon families remain follow-up slices.
