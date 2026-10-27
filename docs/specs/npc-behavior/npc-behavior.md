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

**Desired behavior:** players, NPCs, and monsters are all actors. There is no
special “local player” collision rule. Every actor uses the same actor-pair
collision path and can collide with every other actor:

The table describes pairs of living actors. Dead-body contacts follow the
corpse rule below.

| First actor | Second actor | Required collision |
| --- | --- | --- |
| Player | Player | Capsule pair |
| Player | NPC or monster | Capsule pair |
| NPC or monster | NPC or monster | Capsule pair |

Living actors must not pass through one another. The capsule is the first,
simple body shape; add a flat, standable-top support rule so any actor can
stand on any other actor. A supported actor must move along with its moving
support. This does not replace actor-vs-world collision.

### No-pinning movement rule

In plain language: touching an NPC or another player may push the actors apart,
but it must not glue either actor in place. When a moving actor reaches another
capsule, remove only the part of the remaining movement that points into the
contact surface. Use the rest of that same fixed tick to move along the surface,
and keep velocity along it. Do not discard the whole remainder of the tick or
zero all horizontal movement because of one actor contact. The client and server
must use the same rule so prediction does not repeatedly push the player back
into a collision that the server resolves differently.

Repeated actor contacts must not keep an actor stationary when it has a clear
sideways route. NPC navigation may steer around living actors, while the shared
collision solver remains the final contact owner. Collision response must not
add movement substeps or run outside the fixed 60 Hz gameplay tick.

The dedicated server owns the final collision result for every actor pair.
Resolve collisions in the authoritative fixed 60 Hz simulation and send the
resulting actor state so clients converge to it. Client prediction may help
nearby contact feel responsive, but it must reconcile to the server; a
client-only collision path is not sufficient. Keep pair ordering stable so
the same server tick produces repeatable results.

**Performance for the first version:** do not compare every actor with every
other actor. Use a simple spatial grid or reuse an existing suitable spatial
broadphase to find nearby capsule pairs. Only nearby candidate pairs receive
the more expensive capsule check. Include each actor's movement for the
current tick when finding candidates, so fast movement or a grid-cell edge
cannot make actors pass through one another. Process each pair once, avoid
per-pair heap allocations, and keep collision work on the fixed tick rather
than the render loop. Start with this bounded simple approach; profile the
70-actor Juggernaut scenario before adding more complex optimization.

**Dead actors:** a corpse may keep one ground-level capsule for contact and
support, but it must not become a horizontal wall that traps a living actor or
blocks an NPC route. A living actor may move past a corpse; a valid top contact
may still support an actor standing on it. The corpse remains a visual/ragdoll
body and does not gain separate arm, leg, or torso movement blockers from this
rule. Weapon hit detection remains owned by the weapon collision path.

**Collision diagnosis:** when an actor contact changes movement, bounded
structured events should name both actors and their kinds, whether either actor
is dead or static, the contact direction and time within the tick, incoming and
outgoing movement/velocity, and the response (slide, support, corpse pass-through,
or push). NPC stuck records should include the movement intent and current
velocity so a repeated actor contact can be correlated with a stuck episode.

**Later:** body parts may get their own collision shapes that follow animated
or physical limbs. That is out of scope until the shared root-capsule path
works for all actor pair types.

**Collision data and live tuning:** use `config/actor-collision.json` as the
source of truth for actor-pair capsule sizes and push-response tuning. Store
capsule dimensions per actor/collision profile so a large monster can differ
from a player while both use the same solver. Resolve each actor's profile
through its actor preset or another existing stable actor identity; inspect
current loaders first and avoid a duplicate owner. The file must validate as a
whole, keep the last valid settings if a reload is invalid, and apply a valid
reload at a fixed-tick boundary so designers can tune values during a live
session without changing half a collision step. This file is proposed new work;
it is not claimed to exist or load today.
It must own, at minimum:

- per-actor capsule radius and standing height;
- the ground-level ragdoll capsule length; and
- push tuning for weak-hit distance, minimum and maximum impact speed,
  maximum knockback speed, impact-angle scaling, and the hard-hit-versus-
  landing support cutoff.

Put units in each field name. Reuse existing actor-preset identity for profile
lookup when the loader can support it; do not create a second identity map.
Read existing capsule values before assigning initial profile dimensions. Do
not put these settings in `npc-difficulty.json`.

**Push response:** actors push one another according to impact speed and
contact angle. The push points away from the actor that hit: a hit from the
left pushes to the right; a hard hit from above can push downward. Stronger and
more direct impacts push more, using a continuous response rather than a binary
“push/no-push” rule. Keep the response curve and limits in
`config/actor-collision.json` for live tuning. Initial tuning targets, not
locked balance values: a weak hit moves the target about 0.1 m total; a direct
impact around 100 m/s or above can reach a maximum knockback speed around
5 m/s. Glancing impacts push less than direct impacts at the same speed. Use
the existing physics impulse/velocity path to produce the movement; do not
teleport actors to meet the distance target. The implementation should record
how its configurable response curve maps these targets to the current physics
units, so the designer can tune it in-game.

**Support versus top impacts:** a gentle landing on another actor registers
support, and the actor on top remains standing and is carried if the support
moves. A hard downward impact pushes the lower actor down instead of treating
the impact as a gentle landing. Put the soft-landing versus hard-impact cutoff
and response in `config/actor-collision.json` so it can be tuned live. Do not
let an actor fall through a stable support.

**First-slice acceptance:**

- All three pairings in the table block passage using the same capsule-pair
  owner.
- Any actor can stand on any other actor; moving support carries the actor on
  top. The server's result is visible and clients reconcile to it.
- A dead actor remains a solid, ground-level, lengthwise capsule and cannot be
  walked through.
- Fast actors do not tunnel through actors, including across spatial-grid
  cells. Each candidate pair is resolved no more than once per tick.
- A real 70-actor Juggernaut run records actor count, candidate pairs checked,
  collision time, and render frame time. Confirm the server still simulates at
  60 Hz and compare rendering with the existing 240 FPS goal on the same
  machine. Report measurements; do not claim the goal is met without them.
- No limb-specific collision is required for this first slice.
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

Current source audit (2026-10-20/21): the first actor-pair capsule slice is
implemented in one shared owner,
`src/physics/movement/actor-pair-collision.cpp`, with
`config/actor-collision.json` as the validated, fixed-tick-hot-reloadable
capsule/push/support source. Players, NPCs, and monsters are all actors in the
same pass. The pass uses a swept spatial grid, resolves each candidate pair
once, supports dead actors as ground-level lengthwise capsules, applies
flat-top support/carry, and applies an impact-angle/speed external impulse.
The dedicated and listen servers run the authoritative pass in
`simulateSharedNpcs`; clients run the same solver for local prediction and
apply the server's `NET_STATE_ACTOR_COLLISION` result through local-player
reconciliation. The old client-only `resolveCapsuleVsCapsule` and the server
`resolvePlayerCollision` no-op are removed.

Runtime evidence from a one-client 68-actor Juggernaut run recorded 67-68
actors, roughly 24-139 unique candidate pairs per second, 0.02-0.09 ms
collision time, NPC-vs-NPC contacts/pushes, and support contacts. The server
remained near 60 Hz with 4-9 ms tick cost during that run. Two-client
player-vs-player runtime acceptance remains open because a manually launched
second client hit an unrelated ICE transport crash; the player-player pair is
covered by the same solver and pair-kind counters, but has not been accepted
visually in a clean two-client session. Per-limb/ragdoll-limb collision
remains later work.

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

## 7. Elite skill tier implementation — 2026-10-25

The default strong behavior is now a code-owned `skill_tier` baseline. JSON is
the tuning surface: `elite` starts from the strongest complete behavior set and
explicit profile fields override it; `advanced`, `intermediate`, `beginner`, and
`idle` scale the same capability set down. Profiles without `skill_tier` keep
their previous explicit values.

The elite baseline enables squad focus and swarm, Recast navigation, fast
target tracking, small aim error, target leading, persistent pursuit, and short
movement commitments in C++ rather than requiring JSON to opt in. Duel, TDM, and
Payload now select the `elite_squad` profile through their mode/team roles.

A visible legal target is sufficient to request attack in code, independent of
the current movement state; firing still requires the shared line-of-sight,
weapon, ammo, cooldown, and reaction gates. Bounded `npc.fire-blocked`,
`npc.attack-gate`, and per-shot diagnostics name the exact gate or shot state in
the runtime journal.

This is source and configuration behavior. Runtime acceptance still requires a
real Duel/TDM playtest and human review; the 50/50 top-one-percent Duel target
and the approximately 3-kill player result against 20 NPC kills remain
calibration aspirations, not verified outcomes. Team focus is movement
knowledge only: it never grants aim or fire permission through cover.

## 8. Weapon visit cycle implementation — 2026-10-25

The elite profile now authors a `weapon_cycle` and `weapon_visit_shots`.
Accepted attacks decrement the current visit; the next legal tick switches to
the next usable attack-capable weapon when the visit is complete, the magazine
is empty, or the weapon becomes unusable. Background reload continues while a
weapon is holstered, so the authored example works as:

```text
2 accepted shotgun attacks -> 6 revolver attacks -> 1 rocket attack
-> 2 projectile-rifle attacks -> 6 hitscan-rifle attacks
-> 1 big-shotgun attack -> 12 machine-gun attacks -> repeat when loaded
```

TDM NPCs use the same mode weapon set as players. The `elite_fighter` and
`elite_npc` roles no longer override the set, so both inherit the active TDM
loadout. The cycle includes the mode's melee weapons, and continuous-contact
melee weapons count as usable without a magazine. Their visit is consumed when
the shared server action starts or completes, not by a hitscan `fired` result.
Payload keeps its class-specific role loadouts. Non-attack items are skipped;
the `npc.weapon-cycle` journal record names the reason for any skip.
