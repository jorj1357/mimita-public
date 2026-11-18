# Payload combat and performance intake

**EST timestamp:** 2026-11-18 01:27 EST  
**Branch:** `checkpoint/one-checkout-condensation-20261111`  
**Commits:** none created.

## Result

Recorded the live Payload report as OGI-035 in
`docs/specs/open-gameplay-issues.md` and added an initial measurement sequence
under “Payload live performance investigation start” in
`docs/specs/performance/performance.md`. No gameplay source, configuration,
or regression record changed.

The report preserves these user observations: Payload NPCs do not feel
aggressive/effective enough; NPCs get stuck in geometry, especially the large
cylinder on Fun World 3; direct shots seem to do too little damage, with cause
unknown; the rocket PNG impact/explosion mark floats on a person and should be
placed on the closest sensible surface; combat should center on deliberate,
lethal individual fights rather than indiscriminate explosives into groups;
and Payload still needs high performance, targeting 60 Hz server simulation
and 240 FPS client rendering.

## Documentation and review

- Followed the shared-checkout authority and documentation route. No builds,
  build requests, or gameplay edits were made.
- `docs/skills/documentation-checker-v1.md`: PASS for clear issue ownership and
  routing to NPC behavior, collision, weapons/effects, and performance.
- `docs/specs/performance/performance.md` already defines the quantitative
  acceptance gates. The added start sequence asks for one identity-matched
  Payload run, captures per-frame and server tick/NPC timings, then targets the
  first measured over-budget owner. It does not claim a new measurement or
  diagnosis.
- Human gameplay confirmation and runtime journals are still needed. No
  regression is asserted by this intake.

## Validation

- Reviewed the changed issue and performance sections and checked their routed
  specification paths.
- No tests or runtime validation were run; this pass only records the report
  and investigation start.
- Existing unrelated workspace edits were preserved.
