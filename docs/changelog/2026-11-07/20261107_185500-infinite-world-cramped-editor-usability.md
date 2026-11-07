# Infinite World: Cramped and Map Editor usability

Date: 2026-11-07
Build: `2026-11-07T18:46:29.140437Z:aeca5bcbff24d6256448714fbd2d4d4c297ff1a51f8033d39cec9e20e3876f4a`

## Summary

- Fixed Infinite World: Cramped to load its own radius-10 worldgen profile
  instead of the legacy `connected_world` radius.
- Launch mode 52 now uses `infinite_cramped`.
- Added `infiniteworld list|status|load <world>`.
- Cached the desired cell set and install order; the client now reaches and
  maintains 441 loaded cells at radius 10.
- Added per-connector contact radius, typed chunk spawn anchors
  (`human`, `npc`, `any`), and JSON save/load for both.
- Added server canonical world identity logging and an initial authoritative
  chunk-collision/spawn footprint for the radius-10 neighborhood.
- Server and client logged the same descriptor hash
  `8036895348427122391` for seed `583294`.
- Map Editor: click threshold, box selection, F2 display-name rename, editable
  position/rotation/scale fields, spawn inspection, scrollbar thumb/track drag,
  JSON toolbar layout, per-category transparency alpha, and attachment records
  with a right-click context menu.
- Recorded the 2026-11-07 radius-10 performance run. It does not meet 240 FPS.

## Evidence

- Build: dev-loop `published_current`, source generation 98.
- Runtime: run `20261107_184735`, server pid 21424, client pid 15532.
- Client journal:
  `logs/2026-11-07/20261107_184735/client-01/events.jsonl`
- Server journal:
  `logs/2026-11-07/20261107_184735/server-01/events.jsonl`
- Client: `world-stream.profile` radius `[10,10,0]`, `world-stream.update`
  desired `441`, loaded `441`.
- Travel: client reached X=370.39, cell X=37, with desired still 441.
- Server: `world-stream.server.cells` 441 cells, 5304 triangles, 277 spawn
  anchors.
- Identity: `world-stream.identity` descriptor hash match on server and client.
- Editor command sequence: `mapeditor 1`, add/select/rename/rotate/scale/delete
  completed; `map-editor.rename.apply` was written to the journal.

## Limitations

- 240 FPS is not met. One travel window averaged 25.49 ms with a 131.73 ms
  maximum; a server fixed tick reached 130.36 ms.
- Server authority currently covers the initial radius-10 footprint. Live
  per-cell server streaming around moving actors and full wall collision
  authority beyond that footprint are not complete.
- The Map Editor crash was not reproduced. OGI-007 is not marked fixed.
- Click selection, box selection, scrollbar drag, GUI layout, transparency, and
  attachment visuals were exercised through commands/code paths but not
  human-visually accepted in this session.
- Attachments persist in map JSON and anchored children follow parent moves in
  the editor; runtime physics replication for unanchored attachments remains a
  follow-up.
- Git index is corrupt and was intentionally left untouched; no commit was
  created.
