# Damage volume diagnosis

## Scope

Investigated `docs/specs/entity-editor/entity-editor.md` and the runtime journal
`logs/2026-10-10/20261010_231213/events-000001.jsonl` after a report that walking
into a damage volume did not reduce player health.

## Findings

- The journal is a combined preflight/server/client log. The server executable is
  `C:\mimita-v9\.dev\builds\1945\mimita.exe`, and the server loaded
  `config/maps/zombietower4.json` successfully.
- The journal contains no `damage-volume.damage` event. It therefore does not
  prove that an authored volume ever contained the authoritative player and
  applied damage.
- The server runtime calls `mapEntityRuntimeTick` only during
  `DUEL_PHASE_ACTIVE`. The journal includes a client state sample with
  `match_active: false` and an empty mode, so match-phase activation must be
  captured explicitly in the next run instead of inferred from the visible map.
- The authored entries use small five-unit spheres: `lava_1` is centered at
  `[-1255,-840,384]`, and the later `damage_volume_1` is centered at
  `[-1263.483,-820.257,379.640]`. The journal's later rocket-damage positions
  are not evidence that the player was inside either sphere at the time.
- The damage-volume branch calls the existing ownerless environment damage
  function, which is the correct shared damage owner. It does not use Spy Knife
  contact geometry, because a volume is an occupancy test rather than an attack
  sweep.
- The damage-volume branch does not currently enqueue the normal
  damage-confirmed packet after applying player damage. That is a separate
  user-visible synchronization gap even if containment succeeds.

## Documentation

Expanded `docs/specs/entity-editor/entity-editor.md` with explicit `sphere` and
`box` values, box dimensions, legacy empty-shape behavior, and examples.

## Validation boundary

No gameplay code was changed in this investigation. Build, runtime replay, and
human acceptance remain outstanding. The next implementation slice should add
bounded server diagnostics for phase eligibility, entity settings, authoritative
player position, containment result, damage result, and confirmation enqueue;
then it should queue the shared damage-confirmed event and validate the real
walk-into-volume scenario with a newly identified executable and journal.
