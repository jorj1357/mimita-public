<!--
Purpose: Record the observed loss of Horde role identity and inaccurate NPC fire.
Audience: NPC, gamemode, combat, and hot-reload maintainers.
Does not: Claim DLL reload as root cause or claim a fix is proven.
-->

# Juggernaut Horde team-role and NPC-aim regression

Time created: 2026-11-22T15:27:21Z
Time last updated: 2026-11-22T15:27:21Z

Status: OBSERVED; FIX NOT APPLIED; DLL CAUSE NOT ISOLATED

Related specifications: `docs/specs/gamemodes/juggernaut-horde.md`,
`docs/specs/npc-behavior/npc-behavior.md`,
`docs/specs/weapons/weapons.md`
Related issues: `issue-b95574b63f29`, `issue-1b2463da5666`
Related changelog: `docs/changelog/2026-11-22/20261122_151340-juggernaut-horde-target-and-fire-investigation.md`

## Regression occurrence 1

### Observed

Time: 2026-11-22; active run `20261122_144609_01`, started at
`2026-11-22T14:46:09.574Z`. Mode `juggernaut horde`, map
`dust2cyberiav4`, server PID 4540. Run manifest build ID:
`2026-11-22T08:00:31.376619Z:4fa387f46e02a3a9d136abff3849b879306ecd354f07e145b35b10d20a8d2136`.
NPC aim-policy events identify active game-DLL generation 499; generation 500
was published after this run began.

### Expected behavior

The active Horde gamemode's ordered team descriptors map team 0 to the Fighter
role and team 1 to the Juggernaut role. A `team_pick 2` host override used for
this reproduction should retain the Juggernaut role/loadout and present the
player as hostile to Fighter NPCs. NPCs should aim and fire using shared weapon
and damage rules, and a server hit trace that intersects the player's valid hit
volume should register damage.

Scope note: `docs/specs/gamemodes/juggernaut-horde.md` says the human is a
Fighter, and `config/gamemodes/jugger_horde.json` sets `players: false` for the
Juggernaut team. The user's `team_pick 2` case is a host override/debug
scenario. This record does not authorize changing normal Horde roster rules.

### Actual behavior

At `server-01/events.jsonl:15541`, the module command probe logged
`team_pick 2 accepted=0 authoritative=0 status=probe: policy not migrated`.
At line 15542 the host accepted the override but logged player 1 as team 1,
`team_name=BLUE`, with empty `role` and `weapon_set`. The current config instead
defines team display names `Fighters`/`Juggernauts` and their role IDs.

The same run demonstrates that NPCs did acquire and fire at player 1: there
were 189 `npc.attack-gate` records with `target_id=1` and 303 accepted shots
with `target_id=1`. None recorded `hit_entity=true`; 260 ended on world geometry;
the health mirror stayed at 1,567. Mean measured shot-to-target angular error
was 36.85 degrees and the maximum was 174.60 degrees, while the module aim
policy reported a requested error of 2 degrees.

### Specification

`docs/specs/gamemodes/juggernaut-horde.md` requires Fighter NPCs to target
Juggernauts and Juggernaut NPCs to target Fighters, with friendly fire off. It
requires a same-run trace from team assignment through target selection,
attack gate, firing, confirmed damage, and client receipt. NPC behavior and
perception are shared hot subsystem owners; combat still uses ordinary legal
weapon and hit validation.

### Wrong code / first observed divergence

- `src/network/server-gamemode.cpp::roundTeamName` falls back to
  `gm.teamNames` (which contains legacy RED/BLUE labels) when ordered
  `gm.teams` is unavailable; `roundTeamRoleId` returns an empty role in that
  case. `handleTeamPick` assigns the role from that lookup. The runtime event
  proves the handler saw no ordered team entry, but does not log why.
- `src/npc/npc-combat.cpp` uses `npc.currentFacing` for the horizontal aim
  direction while the module's target aim point only sets pitch. The recorded
  target-to-shot angle confirms the resulting yaw mismatch in this run.
- `src/combat/weapon-fire-raycast.cpp` tests player body-part boxes for
  hitscan; this run does not prove a projectile crossed the body and was
  rejected. Its high aim error and 260 world hits prevent attributing all misses
  to a hit-volume defect.

### Confirmed cause

Confirmed runtime states: the host team-pick path had no ordered team/role
descriptor at assignment time; and the sampled NPC shot direction was far from
the selected target despite a 2-degree requested aim error. The reason the
team descriptors were absent is unresolved. The DLL reload is temporally
correlated, but the run does not isolate it as the cause. The reported loss of
hostility after individual NPC deaths was not captured in a per-NPC
kill/respawn timeline.

### Fix status

No code correction has been applied. Proposed next change is generic: preserve
or re-resolve the active gamemode's ordered team/role descriptors when the
host/module command path resumes, and make shared NPC firing aim use the chosen
target direction for yaw as well as pitch while preserving turn behavior,
weapon cadence, ammo, and authoritative collision. Do not add a Horde-specific
aim/hit exception.

### Proof

The failure is reproduced in the live server journal above. Prior issue evidence
from run `20261121_224058` (generation 436) recorded the correct Juggernaut
role and 14 subsequent target changes to player 1, supporting a role/target
behavior regression. That earlier run did not prove the full firing/damage
chain. No fixed build or post-fix runtime proof exists; human gameplay review is
still required.

### Acceptance for a fix

On one pinned current EXE and explicit game-DLL generation, record team and role
before and after live DLL reload, run the `team_pick 2` override, and verify the
host reports team 1 with role `juggernaut_mode_juggernaut`. Track hostile NPCs
through target, actual aim vector, accepted shot, hitscan/projectile impact,
server-confirmed damage, and client receipt. Include player death/downed state,
connection state, each killed NPC's respawn/replacement, and team/role changes.
Repeat the shared combat path in another team mode to prove the fix is generic.
