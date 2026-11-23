# Fun Counter Strike NPC movement measurements

- Added one-second, per-life FCS movement telemetry for average 3D/planar speed, distance, displacement, local dwell, movement intent, blocked time, near-wall exposure, stuck state, and objective distance.
- Published and ran pinned game DLL generation 507 with EXE build `2026-11-22T17:39:49.763492Z:3e52717b7ea5919fb05f896124b6b5ecf44eb67c72724707c3686c48585ead2d` on `dust2cyberiav4` in Fun Counter Strike.
- At the 592.1 s checkpoint, the 19 NPCs averaged 22.1 m/s 3D and 20.2 m/s planar across latest lives; 2,792 stuck episodes and 1,603 failed-progress events were recorded. There were 4 NPC-v-NPC kills and 59 grenade throws.
- Two plants were followed by CT assignments in 0.037 s and 0.006 s, but assigned CT distances were 32.1 m and 212.7 m. No defuse-start occurred before either explosion.
- Exact wall contacts are not counted. A short-interval movement-derived speed sample reached 5,727.8 m/s against a velocity magnitude of 79.5 m/s, so that instantaneous field is invalid and flagged for correction.
- Evidence: `logs/2026-11-22/20261122_174842/server-01/events.jsonl`; detailed scope and limitations are in `docs/regressions/npc-movement/fcs-objective-speed-defuse-REG-2026-11-22.md`.
