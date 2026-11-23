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

## Evidence update — within-run hot-reload differential and shot outcomes

The same live server run now provides a direct before/after DLL-reload
comparison. This confirms the wrong team/role mapping occurred after the
generation-500 DLL was handling commands; the exact state-transfer or config
lookup defect that leaves the ordered team descriptors unavailable is still
unresolved.

Pinned run `20261122_144609_01`: EXE build
`2026-11-22T08:00:31.376619Z:4fa387f46e02a3a9d136abff3849b879306ecd354f07e145b35b10d20a8d2136`;
generation 500 DLL SHA-256
`907486df17b9ed036abfeeb830a262626b42587321a513d9438e0c5cc2decbdc`.

- Before reload, `server-01/events-0001.jsonl:866-867` records
  `team_pick 2` assigning player 1 to team 1 `Juggernauts`, role
  `juggernaut_mode_juggernaut`, and weapon set `juggernaut_mode`; the command
  result says `switched to Juggernauts (host override)`. The module probe at
  line 858 already reported `policy not migrated`, so that probe failure alone
  was not the regression.
- After the generation-500 DLL path was active, the same command at
  `server-01/events-0003.jsonl:15541-15543` was accepted by the host but
  assigned team 1 as `BLUE`, with empty role and weapon set. The adjacent
  `game.transport.path` event records generation 500 and `migrated=true`.
  Therefore the user-visible change is a wrong role/team mapping after reload,
  not a command that was rejected. The source route into
  `roundTeamName`/`roundTeamRoleId` is identified above; why the active
  gamemode's ordered `teams` were absent at this point remains open.
- After generation 500 became ready at `2026-11-22T14:50:23.447Z`, the
  server emitted 1,225 `npc.shot` events targeting player 1 from 40 distinct
  NPC IDs in `events-0001.jsonl` between 14:50:29.158Z and 14:51:15.539Z, and
  another 303 shots from 34 NPC IDs in `events-0003.jsonl` between
  14:51:15.619Z and 14:51:21.627Z. Across these 1,528 shots, `hit_entity` was
  false in every shot event. The first segment logged 1,031 world hits and 184
  shots without an immediate hit trace; the second logged 260 world hits and
  40 without an immediate trace. The latter are predominantly projectile
  shots whose impacts occur later, so their fire-time records alone cannot
  prove impact or damage.
- For those two segments, measured `actual_error_deg` averaged 35.74 and
  36.85 degrees (medians 7.37 and 6.59; maxima 179.4 and 174.6). Examples
  include `events-0001.jsonl:105442` (173.32 degrees, rocket, no hit trace) and
  `events-0003.jsonl:24-25` (27.98 and 41.22 degrees, shotgun, world hit).
  `events-0003.jsonl:91` and `:354` show visible target senses for player 1 at
  26.89 m and 4.93 m, so targeting did occur after reload even though many
  shots missed.
- This matches `src/npc/npc-combat.cpp:564-568`: the shot's horizontal vector
  comes from `facingPlanar` derived from `npc.currentFacing`; the selected
  target contributes pitch but not yaw. The log's `max_error_deg=2` is the
  requested error cone, while `actual_error_deg` measures the resulting
  target-to-shot angle. This is the first direct source/runtime match for the
  high angular errors.
- `src/combat/weapon-fire-raycast.cpp:358-360,403-404,454-457` skips a capsule
  fallback when a remote player's body parts are absent. The sampled
  hitscan shots generally had a trace and hit world, so this run does not show
  a bullet physically crossing the target body and being rejected. Conversely,
  `src/network/server-projectiles.cpp:520-533,881-904` builds swept player
  capsule candidates and validates projectile contacts through the shared
  damage resolver. Projectile impacts and client damage receipt still need
  correlated proof.
- The target health mirror was 3,500 in the first post-reload shot sample and
  1,567 by the first shot in the next segment, while each individual shot
  recorded no immediate target-health change. The intervening damage source
  has not been attributed. A later sample at
  `server-01/events.jsonl:37,1379` records `no_visible_target`/`target_id=0`
  for two NPCs; this is one later snapshot, not proof that all NPCs lost
  hostility or that player death caused it.

This update supersedes the earlier uncertainty only for the within-run
before/after team-role behavior and the shot/yaw mismatch. No fix has been
applied. The isolated launch and live command-control failures still prevent a
clean repeat after a source change.

### Damage and regression-history correction

- The delayed damage in the same post-reload window is now attributed: `server-01/events-0003.jsonl` contains 17 `server.damage.applied` records with `source=rocket_explosion`, from 14 distinct NPC IDs, between 14:51:16.533Z and 14:51:21.638Z. They total 1,568 damage and include player 1's health reaching zero with an NPC kill (`events-0003.jsonl:2389`). Examples include 24 damage at `:281` and a 150-damage explosion at `:405`. The run proves NPCs were targeting and lethal, through blast damage; it does not prove that the earlier hitscan bullets crossed the player capsule.
- The horizontal shot direction at `src/npc/npc-combat.cpp:564-568` predates live NPC DLL hooks: `git blame` attributes the facing-only vector to `7a253433` (2026-08-09), while module aim hooks were added in `9bfbaeff` (2026-11-11) and retained that vector. Therefore the specific yaw construction is not newly introduced by DLL loading; a hot-reload state issue remains independently supported by the same-run `team_pick` role regression. This history does not establish whether the hook altered timing/state in a way that made the pre-existing aiming weakness more visible.
- `src/npc/npc.cpp:951-984,987-1010` alternates face-target and face-movement modes; movement-facing can last up to 9.5 seconds while the NPC continues attack processing. The current `juggernaut_swarm` profile has no turn-speed override (`config/behavior-profiles.json:669-737`), so it inherits the difficulty turn behavior. This is a plausible shared contributor to the high observed shot-angle error, not yet isolated by a controlled runtime comparison.

The latest dev-loop candidate attempt did not produce a usable artifact: status reports game-DLL changed-source preflight failed at `src/npc/npc.cpp` and the presentation DLL phase exited 1 during linking. The previously pinned generation 500 runtime remains the only direct behavior evidence; no post-fix or fresh-build claim is made.
