2026 10 20 1733 jorj  todo the avatrs need better coliiosnssssss
like big avatars just have ehtier arms and stuff go int ehground that cant happen
npc avatars need like 
idk
matbe like a bounding box around them
or something
idk how tod o it we need betterplanning for it 

2026 10 20 1635 jorj todo : NPCS NEED COLISIONS 
npc = monsters
actor 
its shouldbe actor vs actor collisions
right now, monsters can just walk thru me 
not good
npc need to colide with each other
npc vs npc 
plr vs npc
plr vs plr
and for now, jsut do capsuels vs capsule 
and flat top, so u can stand on them, anyone can stand on anyone 

C:\mimita-v9\config\npc-difficulty.json - also we need to make this like
generalized? per actor?
the jump is a issue 
like jump cooldown , make that a jump cooldown max vs min not just 1 number 

we also need npc collisions, and zombie tower need the npcs to actual attack me, like, monsters just use like 
the force punch maybe? not sure 

2026 10 19 1814 jorj  
- "
npcs need to have a new like allied mode, follow me/follow each other/go more nad more thru to the checkpoints 
then when someone is attacking then attack that attacker 
and stuff " C:\mimita-v9\docs\specs\gamemodes\zombie-tower\zombie-tower.md writing abt this right now 

<!--
Purpose: Define desired combat and reusable movement behavior for NPCs.
Audience: Designers and developers authoring NPC roles, profiles, moves, and
shared actor behavior.
Does not: Define navigation implementation, weapon physics, movement formulas,
or runtime ownership; those remain in their existing specifications.
-->

# NPC Combat and Movement Behavior Specification

- Status: Desired behavior; implementation is partial and described below.
- Scope: Profile-selectable NPC combat cadence, weapon rotation, and reusable
  movement patterns that use the same legal actor actions as players.
- Related references: `docs/specs/weapons/weapons.md`,
  `docs/specs/movement/movement.md`, and
  `docs/architecture/player-npc-systems/npc-movement.md`.

## 1. Goals

NPCs should be able to sustain a visible firefight by firing the weapons in
their loadout, changing weapons promptly when a firing segment is exhausted,
and selecting varied movement techniques from an authored repertoire. These
behaviors are profile choices: a designer can opt an NPC role or game mode
into them without changing every NPC globally.

NPCs use the same weapon definitions, ammunition, cooldowns, movement abilities,
collision, and fixed-tick actor execution rules as players. A behavior profile
chooses when and how to request an action; it does not grant extra shots,
invent ammunition, bypass cooldowns, or bypass movement ability availability.

## 2. Sustained-fire weapon rotation

A selectable combat profile may request a sustained-fire cycle over the
weapons available in that NPC's resolved loadout.

The cycle must:

1. Fire the current weapon at its configured attack cadence while the NPC has a
   valid target and the weapon can legally fire.
2. Track shots fired during the current visit to each weapon. A profile may
   specify a shot count for a visit; absent an override, the weapon's usable
   magazine is the visit length.
3. After the revolver visit reaches six accepted shots, treat it as empty for
   this cycle and request the next available weapon on the next 60 Hz
   simulation tick. If the revolver's actual configured magazine is smaller,
   its real ammunition still governs and the NPC must not create extra rounds.
4. Allow a profile to set the shotgun visit to two accepted trigger shots.
   Each trigger shot uses the normal shotgun definition, including its pellet
   pattern, ammo cost, and cooldown. The two-shot count means two attacks, not
   two pellets.
5. Continue through other configured and usable loadout weapons, then begin
   another visit only when at least one weapon can legally fire. Skip weapons
   that are unavailable, empty without reserve, disallowed by the role or mode,
   or otherwise unusable.
6. Preserve each weapon's own fire cooldown, reload rules, authoritative ammo,
   and normal attack validation. Cycling must not reset cooldowns merely
   because another weapon was equipped.
7. Keep the cycle moving when a visit is exhausted: do not wait for an empty
   weapon to reload if another usable weapon is available. Weapon selection
   may still pause or change when there is no valid target, an attack is
   blocked, or the NPC is interrupted by a higher-priority state.

For the requested revolver/shotgun example, the intended order is six accepted
revolver attacks, switch on the following fixed tick, two accepted shotgun
attacks at the shotgun's normal cadence, then continue to other available
weapons. Switching is prompt; firing remains cooldown-limited.

Weapon-cycle policy and per-visit counts belong in an NPC behavior profile (or
an explicitly selected combat sub-profile). They do not belong in a global
weapon definition because other actors and profiles may use the same weapon
with different tactics. Resolved role/mode loadout remains the authority for
which weapons exist.

## 3. Authored movement repertoire

NPCs should be able to select a movement move from a JSON-authored library,
with a profile controlling which moves are eligible and how likely they are.
The library describes sequences of shared actor inputs and conditions; it does
not implement a second movement or collision system.

Each move definition should be able to state:

- a stable move ID and human-readable purpose;
- selection weight and eligibility conditions, such as grounded/airborne,
  visible target, distance, available abilities, movement preset, or clear
  landing space;
- an ordered action sequence using existing movement inputs (forward,
  backward, strafe, jump, dash, down-dash, and facing/aim direction);
- waits or event conditions, including fixed-tick delays and contact/landing;
- repetition limits and completion conditions;
- interruption conditions, so danger, target loss, death, or an invalid path
  can cancel the move and return control to the normal actor behavior.

Selection may be randomized among eligible moves using the NPC's deterministic
per-actor random stream. Randomness selects an authored, context-valid
technique; it must not produce unbounded random key noise or bypass movement
rules. Geometry and current ability availability are checked before a move
starts and while it runs.

The first move library should include these examples:

### Contact-triggered down-dash and forward-dash chain

When enabled and the NPC has the necessary abilities, request a down-dash.
After the NPC touches the ground or another contact that restores abilities,
wait one fixed simulation tick, request a forward dash, then request another
down-dash. Repeat after the next valid contact. Contact, collision, and the
shared ability-reset rules remain authoritative; if an ability is unavailable,
the move waits for its stated event or cancels rather than fabricating a
reset.

### Rapid dash burst

An eligible move may request up to five quick dashes. Each request still
requires dash availability under the shared movement rules. Contact may
replenish the ability between requests. If the required reset does not occur,
the move must stop or wait according to its authored condition.

### Bunny hop

An eligible move repeatedly requests jump at landing or uses held jump only
where the active movement preset's auto-bhop behavior permits it. It does not
add a separate NPC-only jump rule.

### Air strafe

While airborne, the move changes camera/facing-relative directional input to
steer under the selected movement preset's air-control rules. The movement
specification owns the physics and grounded-versus-airborne distinction.

### Forward-only facing sweep

The move holds forward while turning the NPC's facing/aim direction. Since
forward is horizontal camera/facing-relative input, turning changes the
world-space direction represented by forward. The move may define its turn
direction, rate, duration, and termination condition in actor-relative terms;
it must not store map-specific world coordinates.

## 4. Authoring and selection boundary

The proposed data split is:

- role or game-mode selection chooses an NPC behavior profile;
- the behavior profile chooses combat-cycle settings and eligible movement
  move IDs or weights;
- a JSON move library defines reusable movement sequences;
- the existing weapon and movement configurations continue to own weapon
  statistics, movement presets, and physics values.

This is a desired data contract, not a claim that a move-library JSON schema or
runtime loader already exists. Future implementation should keep values and
move sequences data-authored while using the shared actor/movement owners to
execute them.

## 5. Acceptance scenarios

### High-challenge combat profile

Profiles should support a deliberately high-challenge opponent that wins
through accurate decisions, movement, defensive timing, and legal weapon use,
not through extra ammunition, hidden health, or hitbox advantages. Calibrate
the requested benchmark (a strong player struggling to land five hits while
the NPC can score around twenty kills in a comparable session) through
repeatable playtests; this is an aspiration, not a locked balance target.
Record player skill assumptions, mode, map, duration, kills, confirmed hits,
weapon use, and movement profile before adjusting difficulty.

### Actor-to-actor collision scope

For the first collision slice, every live actor pair uses the same root
capsule-vs-capsule solver: NPC-vs-NPC, player-vs-NPC, and player-vs-player.
Add a flat, standable-top support rule so actors can stand on one another; the
main collision shape remains a capsule. This is the first step, not the final
body-accurate shape. It does not replace the existing actor-vs-world collision
rules.

Later, actor body parts should have their own collision shapes that follow
their animated or physical limbs. That work should follow the shared actor
collision architecture after the root-capsule path works for all three pair
types.

TODO: decide the JSON owner and mapping for actor collision settings. Existing
avatar manifests have capsule dimensions, and `config/collision.json` owns
global collision settings. The actor-collision architecture draft proposes
`config/actor-collision.json` plus actor-preset overrides, but neither is a
confirmed active owner for actor-pair collision yet. Trace whether NPCs use
avatar capsule dimensions before choosing. Add a new profile/file only if the
current owners cannot express per-actor shape choices. Keep NPCs and players
on the same resolved collision contract. Do not treat `npc-difficulty.json`
as the owner for collision shape or solver rules.

First-slice acceptance: each of the three pair types blocks and separates
correctly, either actor can land on and stand on the other, and moving support
does not leave the supported actor behind. The server produces the agreed
result at the fixed tick and clients reconcile to it. Detailed per-limb
collisions are not required in this first slice.

- With a revolver/shotgun/other-weapon loadout and the sustained-fire profile,
  the NPC produces six accepted revolver attacks, switches on the next fixed
  tick, produces two accepted shotgun attacks at normal shotgun intervals,
  and continues to the next usable weapon.
- Empty weapons, reloads, cooldowns, role restrictions, mode restrictions, and
  weapon attack rejection never create free or duplicate shots.
- The contact-triggered chain waits exactly one simulation tick after a
  qualifying reset before the forward dash, then attempts the down-dash using
  current shared ability state.
- The rapid burst never performs a dash without availability; bunny hop and
  air strafe use the selected movement preset; forward-only facing sweep turns
  forward input with NPC facing.
- A blocked route, invalid landing area, target/lifecycle change, or higher
  priority threat interrupts the move without corrupting movement state.
- The same seeded NPC and same fixed-tick inputs choose the same randomized
  move sequence for deterministic reproduction.

## 6. Current implementation status

Source audit (2026-10-20): the local simulation calls
`resolveCapsuleVsCapsule` for the local player against each live NPC, including
a special flat-top support check. It does not resolve NPC-vs-NPC pairs. The
dedicated server's `resolvePlayerCollision` is currently a no-op, and the
server NPC update does not add an actor-pair collision pass. Therefore this
local path does not prove authoritative player-vs-NPC, player-vs-player, or
NPC-vs-NPC collision in multiplayer. Runtime multiplayer acceptance remains
open.

The repository already has pieces that can be used immediately:

- `config/roles.json` selects `behavior_profile`, `weapon_set`, and
  `starting_weapon`; game modes can also select `npc_behavior_profile`.
- `config/behavior-profiles.json` already exposes `fire_cadence_multiplier`,
  weapon range/damage/safety preference weights, and
  `weapon_switch_threshold`.
- `config/npc-difficulty.json` already defines the NPC loadout, starting
  weapon, `switchCooldown`, `fireDelayMin`/`fireDelayMax`, and dash/down-dash
  chances. Its current loadout includes revolver, shotgun, and rocket launcher.
- `config/weapons.json` currently gives the revolver a six-round magazine, so
  the requested six-shot first visit matches its current weapon definition.
- `config/weapons.json` currently gives the shotgun a two-round magazine and
  15 pellets per attack, matching a two-shot visit followed by its standard
  pellet pattern.
- `src/npc/npc.cpp` scores usable weapons by profile and uses a switch cooldown.
  It has a forced empty-magazine switch in the no-active-profile legacy
  distance-selection branch; the active-profile scoring branch is not the
  requested guaranteed next-tick, per-magazine rotation.
- `src/npc/npc-combat.cpp` fires using the equipped weapon runtime and normal
  weapon cooldown/ammo rules.
- `config/movement/movement-source.json` enables auto-bhop for the Source
  preset. `docs/specs/movement/movement.md` defines forward as horizontal
  camera-relative movement and air strafing as airborne-only.
- `src/npc/npc.cpp` produces NPC movement inputs and feeds shared movement
  execution. Dash and down-dash inputs exist, but current chance-based choices
  are not an authored, event-sequenced move library.

Therefore, designers can tune the current loadout, starting weapon, general
fire cadence, weapon preferences, movement preset, and existing randomized
dash/down-dash behavior now. They cannot yet author the requested guaranteed
six-shot/two-shot weapon visit cycle or the reusable conditional movement
sequences solely through JSON. Those require a profile/data schema and an
executor that requests existing shared actor actions.

### What can be configured today

- Choose an existing NPC role's `behavior_profile` in `config/roles.json`, or
  choose a mode-wide `npc_behavior_profile` in its game-mode JSON.
- Tune `fire_cadence_multiplier`, weapon preference weights, and
  `weapon_switch_threshold` in `config/behavior-profiles.json`. These alter
  cadence and scored weapon choice; they do not define a fixed weapon visit
  order or guarantee a switch on the tick a magazine empties.
- Choose the loadout and starting weapon through the role's `weapon_set` and
  `starting_weapon`. The global fallback is `weaponLoadout` and
  `startingWeapon` in `config/npc-difficulty.json`. The global
  `switchCooldown` can be lowered, but profile scoring still decides whether
  to switch.
- Select a movement preset in the role or NPC difficulty configuration.
  `config/movement/movement-source.json` currently enables auto-bhop. The
  global `dashChance`, `downDashChance`, and `jukeFrequency` can tune existing
  randomized behavior, but cannot express “on contact, wait one tick, dash,
  then down-dash” or a five-action move sequence.

These are useful approximations and tuning controls. The exact requested
weapon cycle and authored movement recipes need the profile fields, move-library
JSON, and sequence executor specified above.
