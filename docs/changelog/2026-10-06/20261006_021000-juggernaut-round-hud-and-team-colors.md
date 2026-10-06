# Juggernaut endless rounds and team outline correction

- Scope: make Juggernaut rounds repeat indefinitely, expose the round lifecycle in the GUI, and correct friendly/enemy outline resolution.
- Added an explicit endless-rounds rule. The server now treats a mode with endless_rounds: true as a valid round mode, never declares the match complete from round wins, and advances results into the next countdown.
- Added Juggernaut GUI layout content for intermission, 3-2-1-GO, round number, round-over text, score, and timer.
- Fixed outline team resolution to use the replicated community-match local team instead of the unsynchronized local Player field. Same-team remote actors resolve green; every opposing team resolves red.
- Fixed role resolution so the presentation-only `juggernaut_arcade` preset cannot replace explicit team roles. Juggernaut actors now retain `juggernaut_mode_juggernaut`, including its authoritative `5000` health, heavy movement, behavior profile, and loadout.
- Enabled the teammate outline layer and set its default color to green. Juggernaut mode still overrides opposing actors to red.
- Added a visible Fighter weapon picker during intermission/countdown/GO. Keys 1-4 choose Revolver, Shotgun, Spy Knife, or Rocket Launcher through the existing equip command path; the selection is locked for that life.
- Fixed the renderer to use the replicated actor team for both outline-layer selection and outline color, preventing stale local `matchTeam` data from making teammates red.
- Added the host-only `juggernaut_skip` command. It forces the active Juggernaut round to a draw, marks participating actors dead, shows the round result, and lets the normal next-round flow respawn everyone into the next weapon pick and countdown. Non-host use is forwarded as a future vote request but is explicitly not applied yet.
- Validation: Juggernaut and GUI JSON parsed successfully; explicit endless-round and HUD assertions passed; all touched translation units compiled. The gamemode self-test passed. Final linking was blocked by an already-running `mimita.exe` locking the executable, so no fresh executable or connected-client visual acceptance is claimed here. The actor-preset self-test still fails on pre-existing Counter-Strike preset expectations (reported revolver damage 35 versus expected 100), unrelated to the new Juggernaut preset.
