# Duel runtime and presentation controls

- Completed the standard Duel rules and shared role/preset wiring for a two-slot RED/BLUE match, with an NPC filling the vacant competitor slot.
- Added JSON-owned forced camera/avatar restrictions and independent presentation switches/scales for blood, decals, transient world-hit effects, damage numbers, and actor hit effects.
- Routed avatar allow/deny patterns through selection and server manifest validation; Duel denies `monster*` by default.
- Added a repeatable Duel dev launch profile with weapon set 1.
- Built and launched matching build 2731 server/client through the canonical dev loop. The live journal confirms Duel configuration, the NPC opponent, 15-second intermission, and countdown completion into GO. Kill scoring, timeout/results cycling, visual tuning, and avatar-selection acceptance remain unverified.
