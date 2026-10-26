The opening informal notes below are historical context from early design
discussion. The numbered sections that follow contain the current requirements;
when a note and a numbered requirement differ, the numbered requirement is
authoritative.

captain failcon sounds are in the rerpo 
cuz 

i wanna do like
bc its 60 hz tick rate right now
we can just do like

10 ticks = windup
5 ticks = active attack
15 ticks = recovery
for a bunch of diffrent moves
and do animatins for each attack and stuff
and do jigglpuff and  captin falcon and  fox and stuff
but do it like 3d
not just 2d
and do it like  arena knockbout with percents not hp
and 
todo smth to do iwth hitstun?
bc hitstnu sucks i dont want that 
but it should be super tick based like snchornized u are in state X for N ticks 
and it replciates across server/client 
so its like u can fight anuone anywhere
and need movement to be like ssbm 
with anim cancels 
and matbe shields and grabs?
but grab is i dont liek tehtat
that
i dont like it for same reasoni don tlik ehitstun bc it juts stops u from moving
like what do i even do 
other than spamming to move around
idk 
soidk wew could do grabs but whatevre

---

# Melee-inspired arena fighter: clarified proposal

Status: Historical proposal summary. Where it differs from the current
decisions and requirements below, the numbered specification below is current.

## Purpose and boundaries

This is a 3D, tick-synchronized arena-fighting mode inspired by the movement
and readable attacks of Super Smash Bros. Melee. It is not a 2D fighter and is
not intended to copy every Melee rule. The existing force-punch weapon is a
possible movement/combat prototype, not proof that the full mode already
exists. Weapon-specific contracts remain in the weapon specifications.

## Historical proposal summary

- Simulate gameplay on the shared fixed 60 Hz tick. An action has explicit
  windup, active, and recovery phases; the existing example of 10/5/15 ticks
  is a starting proposal, not a universal duration.
- Replicate the action identity, start tick, and authoritative hit/result state
  so clients show the same move and do not independently award damage.
- Prefer percent and knockback-driven ring-outs over ordinary health as the
  main match win condition. The exact percent-to-knockback curve, stage bounds,
  respawn rules, and scoring are still undecided.
- Keep actors responsive and mobile. Do not add long forced hitstun or grab
  locks without an explicit design decision. Defensive actions, cancels,
  shields, grabs, and air recovery are open choices rather than requirements.
- Author attack and movement poses through the shared animation system and
  preserve a stable actor-root origin while limbs and weapon poses animate.
  Blender authoring/import requirements belong to
  `docs/specs/animations/blender-animation-authoring.md`.
- NPCs participate in the first playable scope. They use the same move,
  animation, contact, hit confirmation, and result path as players; profiles
  choose their decisions, while the input source remains the distinction.

## Required scenario coverage before implementation is considered complete

1. Two remote players perform the same move and agree on start/end ticks,
   confirmed contacts, percent, knockback, and ring-out result.
2. A predicted visual contact rejected by the server does not become damage.
3. A player can move, attack, recover, and re-enter the arena without an
   unintended input lock.
4. Force Punch remains independently usable outside this mode.

The resolved choices and remaining questions are in sections 1–55 below. This
historical summary is context only; it is not a second active decision register.

# MiMITA 3D Melee-Inspired Arena Fighter

**Status:** Design specification. This document records user decisions, identifies proposals, and keeps unresolved details explicit. It does not claim that any feature described here is implemented.

**Purpose:** Define MiMITA's own 3D arena-fighting mode, inspired by the movement, readable attacks, and knockback play of Super Smash Bros. Melee. It is not a 2D fighter and is not a promise to reproduce Melee exactly. The mode should make players feel able to move and act at nearly all times, with skillful combinations producing speed and advantage.

**Status labels:** **DECIDED** means directly selected or clearly stated by the user. **PROPOSED** means a useful working direction that still needs confirmation. **OPEN** means the spec must not choose a firm answer yet. Tick examples are design examples unless expressly marked decided.

## 1. Product intent

**DECIDED:** Make a distinct MiMITA arena fighter: three-dimensional, arena-based, tick-synchronized, and centered on percent and knockback rather than ordinary health depletion. Preserve MiMITA's own identity; Melee is inspiration, not a requirement to copy every rule.

## 2. First playable character

**DECIDED:** Start with one Captain Falcon style humanoid character and a focused set of recognizable punches, kicks, and a Falcon Punch style special. Existing Captain Falcon sounds are available in the repository according to the user; their current files, licenses, and integration are not verified here. **OPEN:** exact move roster, naming, sound mapping, and original versus newly authored assets.

## 3. Broader character and move-set scope

**DECIDED:** Treat moves as reusable move sets, not a fighter-only code path. The same attack capabilities should be usable by NPCs and potentially by actors in ordinary MiMITA modes—for example, a melee NPC fighting a gun-carrying player. **PROPOSED:** Keep fighter match rules as a mode layer while allowing the move/action package to be reused elsewhere.

## 4. Mode boundary and existing systems

**PROPOSED:** Create an explicit arena-fighter ruleset and simulation owner. Reuse generic movement, animation, collision, logging, and networking contracts where they fit. Keep this mode's percent, ring-out, attack, and recovery policy separate from unrelated weapon contracts. The existing Force Punch is a possible prototype or reference, not evidence that this mode exists.

## 5. Decision and evidence discipline

**DECIDED:** This is a specification, not an implementation report. Code, configuration, builds, and live behavior must be inspected before claiming a feature is implemented. User reports and prior conversation express intent; they do not prove runtime behavior.

## 6. Simulation clock

**DECIDED:** The intended simulation is fixed-tick and the referenced MiMITA server currently runs at 60 Hz. **PROPOSED:** Use that shared tick for attack state, combat resolution, movement state, and authoritative replication. Verify the actual tick owner and cadence before implementation; this document does not assert every existing subsystem is deterministic at 60 Hz.

## 7. Tick-counted action state

**DECIDED:** Moves have explicit windup, active, and recovery phases with state lasting a known number of simulation ticks. The selected action animation asset owns its total duration and phase boundaries; these values are authored together with its poses. The example **10 windup / 5 active / 15 recovery ticks** (30 total) comes from the initial request and is an illustration, not a universal move duration. The later **8 / 10 / 17** example is also illustrative, not a selected punch definition.

## 8. Attack phase model

**DECIDED:** Represent a move instance with move identity, actor generation, start tick, current phase/tick, selected animation clip ID, and the immutable content revision used for that instance. Derive phase intervals from the clip's ordered `phase.windup`, `phase.active`, `phase.recovery`, and exclusive `action.end` markers using half-open intervals `[start,end)`. The server and clients resolve the same revision; the server derives authoritative hit eligibility from elapsed fixed ticks and those markers. Exact packet field names remain open.

## 9. Instant attack restart and cancellation

**DECIDED:** Any new legal move/action with an animation can interrupt the current move animation at any tick, including windup, active, or recovery. This applies to player input and to the NPC's next selected attack; NPC attacks must not wait for the previous move's animation to finish when their configured next-attack tick arrives. The new move starts at its own tick 0 immediately; no attack input is buffered. Example: if move A lasts 50 ticks and move B is selected at A tick 7, A ends immediately, B starts at tick 0, and B's 5-tick example then returns to idle/locomotion unless another move interrupts it. Movement input also remains available unless the current move's explicit movement policy restricts steering. A move does not trap the player or NPC until its animation finishes. Re-entering the same move also starts a fresh action instance. **OPEN:** which simultaneous player inputs win if more than one new action is pressed on the same tick.

## 10. Animation versus action authority

**DECIDED:** The selected move's animation drives the visible body-part path and attached hitbox path, and its duration/phase markers own the action clock and active hit window. The clip does not decide whether a hit is confirmed; the shared combat/referee rules do. A new legal move can replace the current clip immediately. When the clip reaches its exclusive end marker without interruption, the shared state machine selects idle or locomotion.

## 11. Movement feel

**DECIDED:** Ordinary WASD movement should be straightforward, while skilled combinations and timing tricks can produce much faster movement, evoking techniques such as Yoshi's or Luigi's movement. This is a feel and capability requirement, not a selected speed formula or a claim about exact Melee physics.

## 12. Movement model

**DECIDED:** Melee uses the dedicated `config/movement/movement-smash.json` profile through the shared movement and collider owners. It must not inherit the global `config/movement.json` preset by accident. For this mode, legacy freeze and down-dash abilities are disabled; they are not substitutes for Melee controls or animation states. The profile and its mode-specific input policy must be selected and observable on both client and server. Reuse current movement/collider code where it matches this contract; change the shared owner only where necessary to implement the selected profile. This does not claim the current config already matches: the file currently enables dash, down-dash, and freeze, so those settings need an explicit Melee-safe profile before implementation can satisfy this requirement.

## 13. Dash and dash dancing

**PROPOSED:** Support quick direction changes and repeated dashes as deliberate movement tools. The user floated double-tapping a direction (including an illustrative 30-client-tick window) and holding Shift as alternatives. Neither binding nor timing is decided. **OPEN:** choose one control scheme, define tap window, dash duration, reversal behavior, cooldown or resource if any, and accessibility options after playtesting.

## 14. Wave-dash and technique combinations

**DECIDED:** The desired system should permit movement tricks and combinations that reward player skill and create speed. Wave-dashing is explicitly of interest. **OPEN:** exact inputs, physics, timing window, momentum transfer, whether the technique requires an airborne/ground contact sequence, and how it is taught or surfaced. Do not label a mechanic “wave-dash” until its behavior is specified and accepted.

## 15. Air movement and recovery

**DECIDED for the first version:** Melee uses `hold_scaled` Space: jump begins on press and gains upward strength for up to 10 held simulation ticks; release stops further boost. This mode-specific policy belongs in `config/movement/movement-smash.json`; it must not silently inherit a different setting from `config/movement.json`. Reuse the shared jump, gravity, and collider owners. Ceiling contact resets the available air jump. Left Ctrl moves downward; it does not activate the legacy down-dash, and Melee does not use freeze. Do not add separate Melee base jump, gravity, speed, or collider constants. Exact minimum/maximum boost tuning remains open for playtesting; see section 55.

## 16. Per-move movement restrictions

**DECIDED:** Movement remains active during an attack unless that move explicitly suppresses new steering input. When a move suppresses steering, preserve the actor's velocity from immediately before the move and continue normal physics from that inherited momentum. A different legal move can still interrupt the attack at any tick and replaces its movement policy. Store steering permission and momentum handling per move/phase; exact tuning values remain open.

## 17. Camera and body orientation

**DECIDED for the first version:** At the attack input tick, capture the camera's horizontal forward vector as the attack direction and lock it for that move instance. A later camera turn does not rotate an attack already in progress. Camera pitch does not aim ordinary attacks vertically. The animation and attached hitbox follow the captured facing; contact still requires real world-space overlap. A later camera-unlocked mode may aim attacks along the direction the actor is moving; that is explicitly out of scope for this first version.

## 18. Directional attack choices

**DECIDED for the first version:** WASD changes movement only. Adding W/A/S/D to a left- or right-click does not choose another attack animation or change its direction; the move is selected by the attack button and faces the captured camera-forward vector. **LATER DIRECTION:** a camera-unlocked control mode may make attack direction follow movement direction. Its bindings and rules remain open and must not leak into this first version.

## 19. Mouse and camera aiming

**DECIDED for the first version:** Attack direction is camera horizontal-forward captured at move start; it stays fixed until that move is interrupted or ends. Attack limbs do not aim continuously at the cursor or home toward targets. Vertical moves such as up-B get their direction from that move's authored animation and move data. Later camera-unlocked aiming is a separate future mode.

## 20. Inputs and controls

**DECIDED for the first control proposal:** Left mouse = A/normal attack; right mouse = B/special attack; Space = the profile-selected `press` or `hold_scaled` jump mode; Left Ctrl = existing grounded crouch/airborne-down behavior; Q held = shield; E = grab. WASD is horizontal movement only and does not select attack variants. A new move/action can interrupt the current animation immediately. Conflicting modifiers use most-recent press order; same-tick ties use the move-set JSON priority. **OPEN:** controller/mobile mappings and accessibility settings.

## 21. Move definition data

**PROPOSED:** Define each move as validated data with stable ID, input mapping, animation clip, tick windows, movement/steering policy, entry-momentum policy, one or more limb attachments, hitbox shape/size/offset, percent and force parameters, shield damage, knockback parameters, and sound/effect references. The hitbox is a separate move-owned volume attached to a named animated body part; it may extend beyond the visible limb so a short arm can reach farther, while its path still follows the animation. Keep data declarative; do not create arbitrary executable JSON scripting. **DECIDED:** validated move-set JSON hot reload takes effect immediately for newly started actions after atomic fixed-tick publication; in-progress actions retain their starting revision. Exact schema remains an implementation design detail.

## 22. Attack geometry

**DECIDED:** Each attack has a JSON-configured, move-specific hitbox attached to an animated body part. The hitbox may extend beyond the visible limb and moves only with that part's animation; it cannot teleport to a target or point independently. A hit requires swept hitbox overlap with the victim during an active tick; there is no second visual-limb proximity test. Sweep every active tick to avoid tunneling. **PROPOSED:** Start with supported capsule/sphere/box shapes and per-move dimensions/offsets. **OPEN:** exact supported shape set and how contact location affects the force multiplier.

## 23. Active-window hit sweep

**DECIDED:** For each active tick, sweep the move's hitbox from its prior limb-attached transform to its current animated transform and test it against eligible target hurt volumes. The client predicts contact; the server uses the referee/lag-compensation policy to accept a plausible overlap or reject an impossible one. Apply one accepted result per configured hit policy. **OPEN:** one-hit-per-activation versus multi-hit, per-target re-hit interval, and collision ordering when multiple targets overlap.

## 24. Contact and hit result

**DECIDED:** Players and NPCs use the same contact-result path for every attacker/target pairing, including NPC-to-player. The client immediately predicts contact and submits a candidate hit/grab result. The server applies the bounded referee checks in the networking spec, then accepts and publishes the same candidate result or rejects it with a reason. For an accepted hit, it resolves the target actor, adds percent, calculates/applies knockback once, and publishes the percent delta, resulting percent, impulse, and contact point to every peer. An NPC hit on a player must update that player's Melee percent and movement exactly as a player hit on that player would; ordinary health damage is not used. Include attacker, target, move instance/revision, actor generation, mapped tick, contact point/normal, percent, impulse, and result so all peers can reproduce it and diagnostics can show the first divergence. A hit requires the move's animated hitbox overlap, not a second limb-proximity check. Visible hitbox overlap alone is not a confirmed hit or percent award.

## 25. Percent meter

**DECIDED:** Percent replaces ordinary health as the fighter damage display and contributes to knockback. Each move's `base_percent` lives beside that move in `config/weapons.json`. Every accepted, unshielded hit adds exactly that configured amount, regardless of the target's current percent: a move configured for 20% adds 20 points at 0%, 100%, 500%, or 999%. Do not increase percent added because the target already has more percent, because time has passed, or because contact speed, proximity, or directness changed. Those contact qualities affect knockback only. Shield protection may reduce received percent under section 29. Shared speed/directness scaling and the percent-to-knockback curve live in `config/melee.json` under `force`. The curve uses the target's percent before the hit; accumulated percent increases later knockback, never the move's percent addition. Keep the reusable calculation with shared weapon/combat behavior in [`../weapons/weapons.md`](../weapons/weapons.md), not in collision geometry code. Its control points, minimum weak-hit output, and cap are editable in `config/melee.json`; do not bury them in C++ or duplicate them per move. Percent resets to 0 for both human and NPC fighters on every respawn.

## 26. Knockback calculation

**DECIDED:** A confirmed hit applies an immediate, strong physical impulse up and away from the contact, using any authored move bias. Its magnitude uses the move-configured base impulse, the shared percent curve, pre-hit target percent, and contact angle/speed/proximity. Predict locally and use the shared networking correction policy. Configure a cap high enough that it does not flatten the 100% and 200% reference targets in section 60. Reconcile the impulse once; never add it again when the accepted result arrives.

Melee's instant WASD control must not erase or instantly decay this knockback. Treat knockback as persistent motion combined with player-controlled movement (or use an equivalent approach): WASD can strongly steer the fighter on the ground and in the air, but cannot replace/cancel the already-applied impulse on the next tick. The impulse continues to move the fighter and follows the configured natural decay/collision rules while input remains responsive. New hits add their impulse once. This is a Melee-specific rule over the default `smash` movement preset's instant horizontal overwrite; do not add hitstun or an input lock to preserve momentum.

## 27. No hitstun and immediate recovery

**DECIDED:** This mode has no hitstun, stun, forced helpless state, or input lock after a hit or shield break. Apply the impulse while continuing to accept movement, jump, attack, shield, and recovery input on the next simulation opportunity. Impact animation/effects may play only as interruptible presentation and cannot delay control or replace the actor's gameplay state. Do not reintroduce hitstun under another name. Simultaneous input, collision constraints, and repeated hit impulses use the shared movement/combat ordering; they must not create a hidden forced-recovery period.

## 28. Hit reaction presentation

**DECIDED:** A confirmed hit immediately gives the attacker, target, and observers readable impact feedback at the contact point: a directional burst/sphere and spatial hit sound, plus a floating percent number. Reuse the existing world-space damage-number renderer in [`../gui/guiv2.md`](../gui/guiv2.md); supply text such as `+3%` from the accepted percent delta instead of ordinary health damage. Do not create a second number system or label percent as health damage. Update the target's percent meter from the same accepted result. Predict the popup and hit response immediately, deduplicate by confirmed contact ID, and reconcile/retract it if the server rejects the candidate. The knockback effect communicates direction and strength but remains interruptible and cannot add hitstun or delay input.

## 29. Shields

**DECIDED for the first version:** Hold Q to shield. At activation the shield has 50 durability points; while held, durability decays linearly to zero over 300 server simulation ticks. Its remaining fraction is `clamp(currentDurability / maxDurability, 0, 1)`. For an incoming hit, calculate both received-percent and knockback multipliers from the shield immediately before that hit: `clamp(1 - remainingFraction, 0.01, 0.99)`. Thus a hit on held tick 1 applies about 0.01x percent and knockback; a hit on held tick 299 applies about 0.99x. Damage dealt to the shield also lowers its durability, so later hits are less protected. Subtract the hit's configured shield damage from durability; a 25-point shield hit breaks a shield with 25 points remaining. Configure shield durability, decay, multipliers, and per-move shield damage in JSON.

**Shield presentation:** While active, show a distinct cyan, vertically elongated ellipsoid centered around the actor: 2 m tall, 1 m wide, and 1 m thick. This shield-only visual is separate from attack hitboxes and the confirmed-hit effect; it does not change the actor's body collider. Its alpha and color track the same remaining-durability fraction as gameplay: a full shield is brighter/more visible (initial example alpha 0.5, meaning 50% opacity); a nearly depleted shield is darker/more transparent (example alpha 0.1, meaning 10% opacity, near held tick 299). These example colors/alpha are editable tuning defaults, not fixed rules. Expose shape size, color endpoints, alpha endpoints, and interpolation in `config/melee.json` under `shield.visual`; changes must be data-editable without a rebuild where the existing configuration reload contract permits.

When shield durability reaches zero from a hit or natural exhaustion after 300 held ticks, immediately show the configured shield-break effect/sound and apply the fixed upward impulse from `config/melee.json` (`shield.break_upward_impulse`, current field [`config/melee.json:11`](../../../config/melee.json), starting value 5.0 m/s). If an incoming hit breaks it, also apply that hit's pre-break shield-scaled percent and knockback; if natural decay breaks it, there is no invented attacker hit. The upward impulse is velocity in meters per second, not a guaranteed travel distance. Shield damage does not spill into fighter percent. While shielded, a move's base percent is used as shield damage unless it explicitly configures `shield_damage`. After break, recover from 0 to 50 durability over 120 server ticks and become usable when full. Releasing Q before break starts a JSON-configurable tick-based recovery; movement remains available. Shield break and recovery never create hitstun or an input lock. All shield timers advance on server simulation ticks.

## 30. Grabs

**DECIDED for the first version:** E selects a configurable grab action. Its animated hand/limb-attached sphere hitbox defines its range; a valid grab requires only swept hitbox overlap. The initial radius target is 1 meter (2-meter diameter), measured in game meters; do not copy the legacy Force Punch value until its unit is verified. A shield blocks grabs at every shield strength. The defender remains fully controllable and can avoid the hit/pull through movement and actions; there is no held, frozen, or button-mash escape state. On a confirmed hit, pull the victim toward the animated grab anchor over the move's JSON-configured tick duration, resolving world collision and transferring configured attacker velocity. Grab strength scales with attacker movement speed and target distance from the sphere center: closer/faster contacts are stronger. The inherited-velocity limit starts at 20 m/s at 0 attacker percent, 40 m/s at 100 percent, and 80 m/s at 300 percent; define the curve and 999-percent limit in JSON. After a successful grab, that target cannot be grabbed again for 300 server ticks. **OPEN:** exact pull duration, the strength falloff/velocity-cap curve between and above those example points, the cap at 999 percent, and what the pull does when the victim's movement collides with a wall. The victim is never immobilized.

## 31. Ring-out and arena bounds

**DECIDED for the first version:** There are no Melee-specific rectangular or stage-authored blast-zone bounds. The existing world void-death rule is the only ring-out boundary and the only way to lose a life. Knockback can carry a fighter into the void; hazards and ordinary health damage cannot kill them. Reuse the current world void-death owner and settings rather than adding a second Melee death boundary. **OPEN:** whether this mode needs a warning cue before the existing void-death threshold; do not invent a new threshold as part of the first implementation.

## 32. Match scoring and respawn

**DECIDED for the first version:** Each fighter starts with four lives. A fighter loses one life only by being knocked into the void. When lives remain, respawn immediately at 0 percent using a random eligible enabled spawnpoint from the selected map's JSON list; if only one is enabled, use it. Spawn protection lasts exactly 60 server ticks, then ends immediately. Losing the fourth life eliminates the fighter. Ordinary health damage, hazards, or another cause cannot kill them. Do not carry the earlier “first to 20 kills” idea into this first-version ruleset. **OPEN:** whether protection blocks hit effects only or also shield drain, grabs, and all knockback; whether the spawn impulse is a physical push or a visual/effect cue (the protection itself is the 60-tick state).

## 33. Stage and hazards

**DECIDED for the first version:** Use the normal game-mode GUI map picker before the match starts. The chosen map is fixed before the 15-second intermission begins; map selection is not part of intermission. Use that map's server-resolved editable spawnpoints for initial spawn and respawn; choose randomly among enabled eligible points, and use the one point directly when it is the only eligible point. The existing world void-death rule is the only ring-out boundary. Hazards may add percent or knockback but cannot kill fighters. See the shared spawnpoint rules in [`gamemodes.md`](gamemodes.md) and JSON override proposal in [`../entity-editor/entity-editor.md`](../entity-editor/entity-editor.md).

## 34. Player and NPC participation

**DECIDED:** NPCs and players use the same move-set, animation, collision, prediction, hit confirmation, percent, and knockback path; only the input/decision producer differs. **First playable match:** exactly one human player versus exactly one NPC, both assigned the `falcon` role and both using the same `captain_falcon` profile, weapon-set ID 12 (`captain_falcon`), and equipped definition ID `captain_falcon_moveset`. The NPC starts with that moveset equipped and must not retain or fall back to revolver, shotgun, rocket launcher, or another ordinary weapon. The roster count includes every startup or runtime-spawned NPC: entering Melee must reconcile the room to one NPC, and further NPC-spawn requests while the first-version roster is active must be rejected or otherwise prevented from adding another fighter. Reapply the same role and moveset on every NPC and player respawn; neither fighter should need to re-equip it. The first playable scenario is exactly one human plus exactly one NPC. This is the first test scope, not a limit on later fighter selection. **OPEN:** how reusable moves map into non-Melee modes.

**DECIDED: hot-reloadable NPC attack behavior.** The Melee NPC reads its attack settings from its resolved NPC `behavior_profile` in `config/behavior-profiles.json`; `config/roles.json` selects the profile for the `falcon` role. Do not put attack cadence or move-choice policy in the actor preset: actor presets describe an actor recipe, while behavior profiles own NPC decisions. Every decision to begin a new Melee action reads the current profile revision, so edits affect the next decision without a rebuild or NPC respawn; an action already underway keeps its original move data until interrupted. The profile exposes `preferred_range`, `melee_attack_gap_min_ticks`, `melee_attack_gap_max_ticks`, `melee_attack_range_meters`, and `melee_move_weights` keyed by exact action IDs from the equipped move set. When the NPC is in attack range and has a live target, it picks an inclusive integer start-to-start interval from the configured minimum and maximum, each clamped to 1–30 fixed 60 Hz ticks. The picked interval is used for that attack; the profile is read again at the next attack decision. When that tick arrives, the NPC immediately starts its newly selected move even if the previous move is still in windup, active, or recovery; this interrupts and replaces the prior action and animation. Missing or invalid range values use the legacy `melee_attack_gap_ticks` fixed interval when supplied, otherwise a 20-tick fallback; if only one range endpoint is valid, it is used for both, and if maximum is below minimum, maximum is raised to minimum. **NPC combat behavior:** While its target is alive and reachable, the Melee NPC must keep closing distance until it can attack, then repeatedly choose and perform attacks on this interval. It must not deliberately run away, orbit, or strafe away from the target as a combat tactic. When an attack is out of range, it should approach the target rather than abandon the attack loop. A close-combat profile with `preferred_range` at or below 2 m implements this behavior by excluding Circle, Strafe, and Retreat choices. `melee_attack_range_meters` controls how far away the NPC may begin an attack; the initial profile uses 4.5 m. Weights for actions absent from the selected moveset are ignored. The initial `melee_aggressive` profile uses a 1.5 m preferred distance, a 10–30 tick start-to-start interval, and mostly quick attacks with low-weight specials. Edit `melee_attack_gap_min_ticks` and `melee_attack_gap_max_ticks` to change the interval range, and edit `melee_move_weights` to change which configured attacks it prefers. The active config location is `config/behavior-profiles.json`, field names `preferred_range`, `melee_attack_gap_min_ticks`, `melee_attack_gap_max_ticks`, `melee_attack_range_meters`, and `melee_move_weights`.

**DECIDED: NPC move animation and sounds for every character.** Every NPC move uses the animation clip and configured `audio.on_start`/phase `audio.events` belonging to that NPC's currently equipped character and move-set action definition in `config/weapons.json`; do not hard-code Captain Falcon animation or sounds for all NPCs or add a parallel Melee-only sound table. The server-replicated action identity and start tick let each receiving client play the matching move animation and spatialized sound at the actor's world position, using that action's configured sound ID, pitch, volume, and offset. Playback is deduplicated per actor/action instance. When a move is interrupted, stop scheduling its remaining phase sounds and immediately start the replacement move's animation and start sound; its own configured phase sounds play if and when their event ticks are reached. Local-player prediction continues to use the existing local action animation/audio path. Animation and sound are presentation only and never decide contact or damage.

## 35. Cross-mode reuse contract

**PROPOSED:** Package reusable move definitions and presentation separately from mode policy. A move can specify attack behavior; a consuming mode decides whether a confirmed hit applies percent, health damage, stagger, or another allowed result. Reuse must pass through existing combat authority and weapon/action identity contracts. Do not assume that fighter percent rules globally replace ordinary health or weapon behavior. Crouching is also shared actor behavior: the Left Ctrl crouch input and grounded crouch animation established for Melee must be wired through the shared actor input/animation path in every gamemode, rather than being gated on Melee or Counter-Strike.

## 36. Animation system relationship

**DECIDED:** Use the shared avatar/actor animation system rather than a fighter-only animation runtime. Existing animation specification describes rigid multipart actors, a shared registry/state machine/runtime, fixed-tick evaluation, reusable clips, and a live-edit path as a specification; it explicitly does not define Blender action import. This fighter spec adds move/action synchronization requirements but does not claim that the described animation features are all implemented.

## 37. Blender authoring direction

**DECIDED:** Blender is the intended authoring tool for attack and movement poses. Exported orientation may be Euler XYZ degrees or quaternion; the shared animation loader converts either to one normalized quaternion runtime representation, as specified in [`../animations/animations.md`](../animations/animations.md). The exported Blender basis conversion must be explicit and validated against the game rig; implementation must not guess axes. **OPEN:** supported rig profile, action naming, and whether curves are baked to fixed-tick samples or retain interpolation metadata.

## 38. Quaternion pose representation

**DECIDED:** Regardless of source encoding, runtime part orientation is a normalized quaternion relative to the normalized rest pose and part pivot; interpolate with shortest-arc SLERP. Version-2 JSON can explicitly use Euler XYZ degrees or WXYZ quaternion fields, and version-1 Euler data remains readable under the compatibility contract in `animations.md`. Blender's coordinate-basis conversion is still open and must be explicit and applied once at import, not guessed by the runtime.

## 39. Pose offsets and root motion

**DECIDED:** The player-origin point should provide a stable reference, with animation using offsets from it rather than repeatedly accumulating transforms. The user also wants the approach to be general enough for weapons/tools and other animatable objects. Melee root motion and impulses are action-owned JSON applied by shared movement/collision systems; the first airborne specials use configured phase impulses rather than changing gravity. **OPEN:** object attachment schema, pivot conventions, scale handling, and exact impulse-event schema.

## 40. Animation phase alignment

**DECIDED:** Bind a move instance's start tick to the selected action clip's tick zero. The clip's `durationTicks` and `phase.*` markers define its total duration and windup/active/recovery windows with half-open intervals; for example, markers 0/10/25/45 define 10 windup, 15 active, and 20 recovery ticks. A cancel/restart begins the replacement clip at tick zero on that action-start tick. There is no separate action duration to tune in `weapons.json`.

## 41. Effects and audio

**DECIDED:** Melee move sound events live in each `config/weapons.json` action alongside its animation reference and combat values; timing comes from the referenced animation asset. Use Captain Falcon sounds from `assets/sound/entity/falcon/`: `yeeeaaaaughhh.wav`, `tuaahh.wav`, `showmeyamoves.wav`, `falconyes.wav`, `falcontuaahh3.wav`, `falcontuaahh2.wav`, `falconswing.wav`, `falconpawnchhh.wav`, `falconkick.wav`, `falconho.wav`, `falconhitquick.wav`, `falconhit2.wav`, `falconhit1.wav`, `falcongroanquick.wav`, `falcongroan2.wav`, `falcongroan1.wav`, `falconfalcon.wav`, `falcondeath2.wav`, `falcondeath1.wav`, and `falconcomeon.wav`. Start with pitch multiplier 1.0, volume multiplier 1.0, and spatial offset `[0,0,0]` meters; map sounds to action/phase events by their asset names and intent. Starting or retriggering a move immediately stops the previous Melee move sound on that actor and starts the new sound from tick 0; later markers from canceled actions do not play. This is one Melee-move sound channel per actor; music and unrelated sounds continue. Sound never confirms a hit. Deduplicate events by actor generation, action instance, phase/tick, and event index. The audio/resource owner resolves the asset IDs.

## 42. Authority and replication

**DECIDED:** Combat input and movement use the shared client-predicting/referee-validated policy. The attacker predicts the move animation, captured camera-forward vector, animated-limb hitbox, contact, percent, and impulse immediately. The server checks the candidate against the shared historical state and legal envelope; accepted results are the canonical result replicated to all peers. The server continues to own match lifecycle, inventory, stock decrement, elimination, and match result. A correction replaces the predicted state without applying the hit impulse twice. Replicate actor generation, move/action ID and revision, clip/action instance and start tick, captured attack-forward, button press order, per-move phase/tick, candidate/accepted hit result, percent, shield/grab state, impulse, and life result. Evaluate poses locally from shared clips rather than sending each limb every tick. Sound events use stable IDs. Packet details follow the shared networking spec.

## 43. Client prediction

**DECIDED:** On an attack press, immediately start its animation at the client's predicted simulation tick and capture camera-horizontal-forward. Predict the hitbox sweep, contact, percent, grab pull, and knockback before a round trip. A historically plausible result is accepted by the referee and becomes the same replicated outcome for every peer; impossible outcomes are corrected. Stock decrement, elimination, and match result remain server-published. Reconcile positions and meters without duplicating an impulse; show the server-disagreement effect when a correction occurs.

## 44. Lag compensation and fairness

**DECIDED:** Use the shared networking lag-compensation and history policy. Reuse its mapped client/server tick, bounded history, and action-category rewind rules; do not invent a separate Melee rewind window. The attacker should see a plausible grab/hit become the shared result immediately, while the server rejects clearly impossible results. This is bounded prediction with referee validation, not unrestricted client authority.

## 45. Rollback and reconciliation

**DECIDED:** Use shared networking correction and history owners. Smooth small position errors, snap large or lifecycle corrections, and display the server-disagreement effect whenever an actual correction occurs. Accepted candidate contacts do not receive a second impulse on replication. Keep action/move revision and contact IDs in history so replay and duplicate suppression are deterministic.

## 46. Network state contract

**PROPOSED network state:** replicate actor generation, authoritative simulation tick, action/move identity and revision, action/clip instance ID and start tick, captured horizontal attack direction, phase, movement state/velocity, percent, shield meter/state, life count, and confirmed contact/knockback/ring-out result. Animation is re-evaluated locally from the shared clip and tick; do not send per-bone packets by default. Validate animation/move revisions so clients cannot interpret an action using different content. Sound event IDs are deduplicated by actor/action/tick/event. Exact packet shape and cadence remain owned by the networking spec.

## 47. Testing strategy

**PROPOSED:** Test the rules at three levels: deterministic unit/model tests for phase boundaries, cancellation, sweep geometry, percent/knockback, and rollback; multiplayer integration tests for server authority and correction; and live human play sessions for speed, readability, fairness, and whether no-hitstun combat feels controllable. A build or synthetic test alone is not proof of satisfying the feel requirements.

## 48. Required acceptance scenarios

**PROPOSED acceptance gates:** (1) two remote players select the same move and agree on start/end ticks and confirmed results; (2) a predicted visual contact rejected by authority does not become confirmed percent; (3) a player can move, attack, be launched, recover immediately, and re-enter without unintended input lock; (4) attack restart during windup replaces the current attack immediately without buffering; (5) multi-tick sweep catches valid crossings without duplicate unintended hits; (6) NPC uses the same move/action path; (7) reusable move set works in at least one non-fighter context without changing that mode's damage contract; (8) Force Punch remains independently usable outside the mode; (9) quaternion-authored clip round-trips and is stable on clients. These are future acceptance criteria, not completed results.

## 49. Open decision register

**OPEN:** The remaining tuning questions are collected in section 55. The first-version rules already decided include four lives, void-only deaths, 0 percent on immediate respawn, 60 ticks of spawn protection, map selected through the normal GUI before intermission, random eligible JSON spawnpoint selection, a 15-second intermission, animation-authored per-move phase timing, 30-tick editable double-click window, 300 held-tick shield decay, 120-tick shield recovery after break, 300-tick grab immunity, shared movement/collider values, and the listed Captain Falcon input/move candidates.

## 50. Implementation and rollout gates

**PROPOSED sequence:** (1) audit current movement, animation, collision, combat authority, networking, and mode lifecycle owners; preserve evidence and unrelated behavior; (2) settle remaining move, shield/grab, map, spawn, and physics values; (3) implement Euler/quaternion import, explicit Blender basis conversion, tick-sampled animation, and sound markers; (4) implement Melee lifecycle/countdown, shared player/NPC body collision, tick-based move/cancel semantics, and animated-limb sweeps; (5) add percent, predicted knockback, server confirmation, four-life void ring-out, and 0-percent respawn; (6) run first-slice scenarios locally and across clients with live journal evidence; (7) tune from human play. No stage is claimed complete by this document.

---

## Source alignment notes

- The initial conversation proposal establishes the 3D arena scope, fixed-tick phases, percent/knockback, responsive control, NPC participation, and separation from generic weapon contracts.
- The user's later answers establish the preferred fast movement-tech feel, move-specific movement restrictions, immediate attack restart without buffering, multi-tick hitbox sweeps, angle/speed-scaled contact force, no hitstun, a 50-point decaying shield, controllable pull grab, prediction-first referee validation, reusable NPC/cross-mode move sets, an always-active Captain Falcon move set, camera-forward attack orientation, and four lives with void-only death in the first version.
- The shared animation specification defines the desired shared rigid-part animation contract, including explicit Euler/quaternion source encodings and normalized-quaternion runtime conversion. Blender export remains a separate proposal, and the current runtime has not yet been shown to implement this rotation contract.
- A transcript expresses intent and decisions. It does not prove implementation or runtime acceptance.

## 51. First playable slice: Captain Falcon

**Status: SPECIFIED WITH OPEN DETAILS.** The user has decided that the move set
is always active, attacks are driven by authored animations, attack-forward
uses the camera's horizontal forward direction, and the first version has four
lives with void ring-out as the only way to die. Other controls and match
details remain open in section 49. This section does not claim that the mode,
attacks, exporter, or networking path already exists.

### Character and move-set ownership

- The first fighter profile is `captain_falcon`. It selects a character rig,
  animation clips, sounds/effects, and a `captain_falcon` move-set identifier.
  Later characters can select other move sets while reusing the same mode and
  combat pipeline.
- **DECIDED:** The Captain Falcon move set is one equipped item/set containing
  all A/B attacks. The fighter never equips or switches individual punches,
  kicks, or specials. The set remains active across the whole match and after
  respawn.
- Store the Captain Falcon set and its attack definitions together in one
  `config/weapons.json` definition, using the reusable `melee`/action
  components from [`../weapons/weapons.md`](../weapons/weapons.md). Each attack
  is a stable action ID inside that definition, not a separate weapon or
  hotbar slot. `config/weaponsets.json` may reference that one definition as
  the Melee loadout; it must not duplicate its attack values. The user wants
  input bindings, one animation clip reference (whose markers define phase lengths), hitbox, sound/pitch, percent,
  knockback, movement policy, and special movement all configurable there.
- The network identity is the equipped definition ID/revision plus action ID
  and actor generation. A hotbar index is presentation/input context only; it
  cannot authorize the move. A legal move remains available after respawn
  without re-equipping or changing a numeric slot.
- The human and the one first-version Melee NPC resolve to the same role and
  weapon-set reference, and equip the same `captain_falcon_moveset` definition
  ID/revision. The NPC must not receive a generic NPC weapon set as fallback.
  On mode entry and respawn, verify the effective equipped definition for both
  actors; if either cannot resolve it, fail the Melee setup visibly rather
  than silently starting a gun-equipped match. Log actor ID, controller,
  role, weapon-set ID, equipped definition ID/revision, and spawn/respawn
  result so the live scenario can prove both fighters received the same set.
- Reuse shared item, animation, collision, prediction, and authoritative combat
  owners. Do not create one C++ weapon-definition function or packet per attack.

### Initial input and move roster

The following is the first Captain Falcon A/B move list. Each attack references
one animation clip whose duration and phase markers define windup, active, and
recovery ticks. There is no independent 1–999 timing override in
`weapons.json`. The quick-punch
double-click window starts at 30 ticks and is JSON-editable. Each move has an
authored full-action clip and a move-owned hitbox attached to an animated body part;
percent and knockback; and sound/pitch/volume/offset data. WASD moves horizontally. Camera-horizontal
forward is captured when the attack begins. Ordinary attacks do not auto-aim
vertically: the authored limb/hitbox must reach the opponent, so fighters
usually jump or fall to meet an opponent above or below. Explicit up/down moves
use their authored hitbox and root-motion direction. The IDs below are
illustrative stable IDs; timings and detailed tuning remain open.

| State / input | Action ID | Required behavior |
|---|---|---|
| Any state: WASD | `move_horizontal` | Horizontal movement only; never changes attack direction/variant. Reuse existing movement values. |
| Space | `jump` | Uses the configured Melee jump input mode from `config/movement/movement-smash.json`; press mode jumps on key-down, hold-scaled mode jumps immediately with minimum strength and adds a tick-based upward boost while held for up to 10 ticks. |
| Grounded: left click | `a_ground_punch` | Play punch animation. A second left-click within the default 30-tick, JSON-editable window selects a distinct double-punch animation. |
| Grounded: right click | `b_falcon_punch` | Play the normal `punch_a` clip. Every new right-click starts the same animation again from tick 0. |
| Airborne: left click | `a_air_kick` | Play the normal `kick_a` animation; another left-click may restart the same move. A configurable forward/up impulse may be applied on the first active tick. |
| Airborne: right click | `b_air_forward_dash` | Play the normal `kick_a` animation with its configured forward/up impulse. |
| Grounded: Space + left click | `a_uptilt` + `jump` | Start the `uppunch_a` animation and jump in the same tick. This combination is explicitly allowed. |
| Airborne: Space held + left click | `a_air_up_kick` | Play the `uppunch_a` animation and upward hitbox/impulse. |
| Airborne: Space held + right click | `b_air_up_special` | Play `uppunch_a` with the stronger JSON-configured upward momentum and hitbox. |
| Grounded: Ctrl held + left click | `a_ground_sweep` | Play the authored `downpunch_a` animation. |
| Grounded: Ctrl held + right click | `b_falcon_punch_crouch` | Play the same `downpunch_a` animation with the stronger B-move combat values. |
| Airborne: Ctrl held + left click | `a_air_down_kick` | Play the authored `downkick_a` animation. |
| Airborne: Ctrl held + right click | `b_air_down_strike` | Play the same `downkick_a` animation with the stronger B-move combat values and configured forward/downward impulse. |
| Q held | `shield` | Hold-based 50-point / 300-tick shield described in section 29. |
| E | `grab` | Starts the configurable pull grab described in section 30; never freezes the victim. |
| Left Ctrl without attack | `crouch_or_fast_fall` | On the ground, enter crouch and play the crouch animation while held; release returns to standing. In air, preserve the downward/fast-fall behavior. This shared action and grounded animation must also be wired into every gamemode. |

Every move definition in the single `weapons.json` move-set entry needs a
stable action ID, input/chord and grounded/airborne condition, double-click
window (default 30 ticks, editable), one stable animation clip ID,
movement/steering and
momentum policy, per-move root displacement/impulse, hitbox shape/dimensions/
offset/animated-part attachment/active window, percent/knockback/shield-damage
values, tick-addressed sound IDs, pitch, volume and spatial offset, and content revision. The referenced clip asset alone defines total duration and phase boundaries. A shared
parser/runtime owns the reusable behavior; the JSON selects and configures it.
The 10/5/15 and 50-tick-canceled-at-tick-7 examples illustrate tick behavior,
not fixed Captain Falcon timings. Clip duration and ordered phase markers are
authored with the animation; `weapons.json` has no timing override. Phase
intervals use half-open `[start,end)` bounds on clients and server.

Grounded/airborne state and button edges are sampled on fixed simulation ticks.
Melee move selection and authored locomotion/crouch pose selection use
`stableOnGround`, so a brief missed ground-contact sample does not switch the
move category or visible pose to airborne. The collision system still owns raw
`onGround`; stable state only applies its existing short contact grace.
Space+grounded-left-click both jumps and starts uptilt. When held modifiers
conflict, the most recently pressed modifier selects the attack variant; the
input command must preserve press ordering so client and server choose the
same action. If two conflicting modifiers are pressed in the same tick, use a
JSON-configured tie-break priority. The 30-tick double-click window is a JSON
value. A second grounded left-click inside it starts the distinct double-punch
immediately; repeated grounded right-click restarts Falcon Punch at tick 0.

Each move may configure one or more impulses at a phase-local tick in the
captured attack basis: horizontal camera-forward, camera-right, and world-up
(world +Z in this engine). For example, an air kick can apply a forward/up
impulse on the first active tick. The downward special may instead author a
bounded displacement path; `+5 m forward / -10 m up` is an example, not a
hard-coded default. The action data owns impulse/displacement values and tick;
the shared movement/collision system applies it. Hitbox overlap alone confirms
contact; no separate visible-limb proximity test is required.

### Facing and third-person camera

**DECIDED for the first version:** third-person attack-forward follows the
camera's horizontal forward direction. At the action-start tick, capture that
horizontal direction as the move's facing basis and replicate it with the
action state; the animation then drives the limb path and attached hit shape.
The camera's vertical pitch does not directly tilt an ordinary punch hit
shape. An authored move such as up-B may use its own animation and move data
to travel vertically. **DECIDED:** W/A/S/D is movement-only and never selects
an attack variant or changes attack direction in this version. Movement and
body turning may use separate rules; only attack-forward is fixed by this
camera rule.

### Animation and limb-bound hitboxes

- Use the shared animation registry/runtime and the Blender authoring route in
  [`../animations/animations.md`](../animations/animations.md) and
  [`../animations/blender-animation-authoring.md`](../animations/blender-animation-authoring.md).
  Required state clips include idle, walk, run, jump, fall, land, punch,
  Falcon Punch, kick, knee, elbow, up-B, side-B, and down-B as the roster is
  finalized. Missing clips must use the shared fallback behavior, not a
  Melee-only animator.
- Every move supplies its own hitbox dimensions and local offset, attached to a
  named rig part such as `leftHand`, `rightHand`, `leftFoot`, or `knee`. The
  hitbox may extend beyond the visible limb so an attack can reach farther than
  a short avatar arm, but remains attached to that limb. At each fixed tick,
  derive its world transform from the authoritative actor root and animated
  part pose. Test
  contact only during the move's active windows; use the specified multi-tick
  swept-volume rule so fast limbs cannot tunnel between ticks. The hitbox is a
  separate move-owned gameplay volume, not the limb mesh or actor body collider.
  It may extend past the visible limb for reliable reach, while remaining
  anchored to and moving with that limb; it cannot independently aim, teleport,
  or scale with frame rate. The referenced rig part must provide a valid
  animated transform; it need not itself be a physical collision mesh. A
  visible pose alone is never proof of a hit.
- Blender export must preserve stable part names, pivots, actor-root offsets,
  and the 60 Hz integer-tick mapping. Validate orientation and scale at import,
  reject invalid mappings, and keep render pose separate from authoritative
  actor movement. Do not author large accumulated world-space Euler angles.
- The shared animation format now specifies explicit Euler-degree and
  quaternion input fields that both convert to one normalized quaternion
  runtime representation. See [`../animations/animations.md`](../animations/animations.md).
  Do not add a Melee-only rotation format or imply the Blender exporter is
  implemented.

### Percent, knockback, blast zone, and stocks

Moves do not deal ordinary health damage. A confirmed hit adds percent and
applies the move's knockback; accumulated percent contributes to later
knockback according to a still-open formula. There are no Melee-specific stage
blast-zone bounds. The server's existing world void-death rule is the only
ring-out/death boundary: when knockback carries a fighter into the void, the
server records one void death and removes one of the fighter's four lives.
Respawn occurs immediately while lives remain, resets percent to 0, and grants
60 server ticks of spawn protection; no other event may kill the fighter.

### Network and replay contract

Follow [`../networking/networking.md`](../networking/networking.md): capture
input and simulate gameplay at fixed 60 Hz; use the shared tick mapping,
history, lag-compensation window, and reconciliation policy. For Melee, the
local client predicts a hit/grab/percent/impulse immediately and submits that
candidate result. The server acts as referee: it checks action identity and
revision, actor generation, historical overlap, shield state, match membership,
hit limits, and movement/force plausibility. Plausible results are accepted as
the canonical replicated outcome; clearly impossible results are rejected with
a reason and visible server-disagreement correction. Replicate action ID and
revision, clip ID and immutable clip-content revision, action instance, start tick
(and derived phase/tick for diagnostics if needed), attack direction, input ordering,
candidate/accepted target and contact, and accepted result. Do not send per-limb
pose packets by default; both sides evaluate the shared animation locally.

Required first-slice scenarios:

1. Two clients with the same fighter/move-set revision select `a_neutral` and
   `b_neutral`; their action IDs, phase boundaries, confirmed contacts,
   percent, and knockback agree after reconciliation.
2. Change the visible hotbar selection or respawn the actor; a legal fighter
   action remains available without re-equipping a weapon, and a stale
   previous-generation action is rejected with a reason.
3. Sweep a limb through a target during an active tick, then repeat outside
   the active window; only the valid contact changes authoritative percent.
4. Launch a fighter into the void; the server records one ring-out and applies
   the selected stock/respawn rule. No separate stage blast-zone boundary is
   introduced.
5. Run the same move through a player and NPC and confirm both use the same
   move, animation, hit-confirmation, and result path.

This is a specification target only. Acceptance still requires the runtime
scenario and live journal evidence described by the router's workflows.

## 52. First-version match lifecycle: `Melee`

**DECIDED:** The selectable game mode is named `Melee`. Select the map in the
normal game-mode GUI before starting the match, as for other modes. After the
map has been selected and loaded, the Melee match enters its 15-second
intermission (900 server simulation ticks). There is no map picker during
intermission. The normal shared game-mode flow then shows `3`, `2`, `1`, and
`GO!`; fighters cannot move, jump, attack, grab, or shield during 3/2/1. On the
first simulation tick of `GO!`, the match becomes active and gameplay inputs
are accepted. Use the shared gamemode's countdown timing and GUI rather than a
Melee-only countdown implementation.

At active-match start, each fighter has four stocks (lives) and 0 percent. The first
playable configuration has one human and one NPC, both using the
`captain_falcon` profile and move set. A void death consumes one life; if lives
remain, the server respawns the fighter immediately at 0 percent and applies
60 server ticks of spawn protection. Both human and NPC percent meters reset to
0 on every spawn and respawn. Every stock loss is announced in global
chat with the fighter's name and exact stocks remaining. Losing the last stock
announces the elimination and the surviving fighter as the round winner. The
server ends the match when one fighter remains, shows the winner in chat and
on the results HUD for exactly five seconds, then returns to intermission
before the next match. The Melee mode's `results_seconds` is 5. A fighter with
no lives is eliminated. Protection does not disable movement; its exact blocked
effects follow the open question in section 55. No other death cause is
allowed.

## 53. Shared actor collision and combat movement

**DECIDED:** Keep the existing capsule side/bottom body collider and its
dimensions/tuning. Add a flat standable support surface at the top for
actor-on-actor landing; it is a support contact, not a replacement for the
capsule collision shape. Players and NPCs share this owner, and several smaller
actors may stand on one actor when their support areas fit. Standing on an
actor does not itself cause damage. Reuse the same movement and collider
values for Melee; do not invent Melee-only jump, gravity, speed, or collider
constants. **OPEN:** exact top-support width/edge behavior and resolution order
when multiple actors and stage surfaces contact together.

All movement, support, overlap, and action timing uses the existing fixed-tick
movement/collision owners. Contact geometry supplies hitbox overlap; shared
weapon/combat logic converts relative speed and angle into force, percent, and
impulse. For networked combat, the client predicts immediately and the server
referees candidate outcomes according to [`../networking/networking.md`](../networking/networking.md).
On a correction, do not apply the same impulse twice. The scenario records
client candidate and server accepted/rejected state, both positions/velocities,
contact tick, percent, impulse, correction type, disagreement effect, and first
divergence. This is a desired contract, not a claim that current behavior is
already proven.

## 54. First-slice acceptance scenarios

1. Choose a map through the normal GUI, start `Melee`, and verify that the
   selected map is loaded before the 900-tick intermission begins. Confirm no
   map picker appears during intermission. During 3/2/1, attempt gameplay
   inputs and confirm actors remain frozen; on `GO!`, verify both release on
   the same server tick.
2. Start one human versus one NPC, both Captain Falcon. Use left click with and
   without WASD and verify the same move starts facing the camera direction
   captured at the attack tick; turning the camera afterward does not rotate
   that move. WASD still moves the actor.
3. Start a 50-tick move, interrupt it at tick 7 with a 5-tick move, and verify
   the second clip begins at tick 0 immediately, completes, then returns to
   idle/locomotion. Repeat while steering is suppressed and confirm the actor
   retains pre-move momentum; confirm Falcon Punch can be canceled the same way.
4. For each attack, compare animated limb transform, attached per-move hitbox,
   and active swept contacts. Verify the hitbox may extend past a short limb,
   still follows its animated attachment, and cannot hit outside its active
   window.
5. Verify both jump policies from the selected movement profile: `press` jumps
   immediately on the press; `hold_scaled` jumps at minimum strength and adds
   tick-based boost through at most 10 held ticks. Verify release stops further
   boost, ceiling contact restores the air jump, and all other jump/gravity/
   collider values come from existing shared movement owners.
6. Test every listed grounded/airborne input and modifier chord, including
   second-click punch, repeated Falcon Punch, simultaneous jump+uptilt, upward
   kick/special, ground sweep, and both downward kicks. Confirm the last-pressed
   modifier wins, same-tick input order/tie-break is identical on both peers,
   and every action uses its own JSON definition.
7. Hold Q for 0, 150, and 299 ticks and verify 50, 25, and near-zero shield
   durability respectively. Apply known hits and compare the 0, 0.5, and near-1
   received percent/knockback multipliers. Verify shield damage equals move
   percent by default, excess does not spill into fighter percent, a breaking
   hit applies both scaled hit knockback and 5 m/s upward impulse, natural
   expiry gives the same break, and recovery completes over 120 server ticks.
   Use E to test the 1 m radius grab sphere (2 m diameter), shield blocking,
   movement-speed/center-distance strength, configured multi-tick pull, no
   victim input lock, and 300-tick anti-chain-grab immunity.
8. Confirm weak/glancing/slow and strong/direct/fast hits add proportional
   percent and produce the configured knockback. Verify the response is
   predicted immediately, a plausible historical result is accepted and
   replicated identically, clearly impossible results are rejected, correction
   does not double-apply force, and any reconciliation shows its disagreement
   effect and logs the first mismatch.
9. Ring out each fighter through the existing world void-death rule; confirm
   exactly one life is removed and immediate respawn starts at 0 percent with
   the same move set active. Confirm the 60-tick protection ends exactly on
   time. The fourth death eliminates the fighter; no custom stage blast
   boundary, health, hazard, or ordinary attack can kill it.
10. Retrigger/cancel animation sounds rapidly. Confirm the new clip's tick-0
   sound starts immediately, cuts off the prior Melee animation sound on that
   actor, and later markers from the canceled clip do not play. Confirm
   prediction and replicated events are deduplicated.
11. Place a player on top of an NPC and an NPC on top of a player; verify the
    shared flat-top body collision supports them and that downward attacks use
    their own limb hitboxes.
12. Change a move-set JSON value during a match. Verify the validated revision
    activates at a fixed-tick boundary for new actions immediately, current
    action instances retain their starting revision, and clients receive and
    use the same revision before accepting those new actions.

These scenarios are specification requirements for future automation/runtime
proof. They are not implemented tests or observed results.

## 55. Remaining review decisions

The core behavior is now specified. These are the remaining product choices or
initial tuning values; an implementing agent can build the editable fields and
use clearly labeled provisional defaults where you prefer to tune in playtests.

1. **Phase tuning:** Windup, active, recovery, and total action length are
   authored in each animation clip and cannot be overridden by `weapons.json`.
   Selecting concrete first-pass Captain Falcon poses and marker positions is
   still an authoring task; the double-click window remains a separate editable
   30-tick input rule.
2. **Input tie:** The most recently pressed conflicting modifier wins, and
   Space+grounded-left-click both jumps and performs uptilt. If Space and Ctrl
   have exactly the same press order/tick, which one should the default JSON
   tie-break prefer?
3. **Jump strengths:** The profile selects existing `press` jump or
   `hold_scaled`; the latter launches immediately at its minimum, then adds
   tick-based boost through 10 held ticks and stops boosting on release. What
   minimum/maximum strength multipliers should `movement-smash.json` start with,
   or should the agent set provisional values?
4. **Move impulses:** Each move may author camera-relative impulses by phase;
   air attack impulses happen on the first active tick, and gravity is not
   changed for one move. What first forward/up impulse values should the air
   attacks use, or should those be provisional JSON values?
5. **Shield release:** How many ticks should a released, unbroken shield take
   to recover? Should recovery continue from its remaining durability or
   restart from zero? On spawn protection, should all damage, knockback, shield
   drain, and grabs be blocked for 60 ticks, or only some of them? Is the
   “spawn impulse” a physical push, a visual cue, or both?
6. **Grab tuning:** I interpreted “2 meter sphere, center to edge 1 meter” as
   radius 1 m / diameter 2 m. Confirm that interpretation. What pull duration
   should be the starting value? The velocity-limit examples are 20 m/s at 0%,
   40 m/s at 100%, and 80 m/s at 300%; should the agent use a straight-line
   curve from these points, and what should the 999% limit be?
7. **Force and damage curve:** The shared combat force combines relative speed
   and contact directness; slow/glancing is weak, fast/direct is strong, and
   force scales both percent and knockback. What starting curve/caps should be
   used, or should the agent add JSON-editable provisional values for playtest?
8. **Standable top:** Capsule sides/bottom remain the body collider; the top is
   a separate flat support contact, and multiple actors may stand on one actor
   when they fit. Should the implementing agent audit/derive its exact width and
   resolution from the current collider, or do you want to set dimensions?
9. **Held jump and auto-bhop:** If Space remains held through a landing in
   `hold_scaled` mode, should the existing movement profile's auto-bhop start a
   new minimum-strength jump immediately, or should the player release and
   press Space again?
10. **First NPC:** Should the Captain Falcon NPC use an existing profile or
    should the implementing agent choose simple provisional attack/defense
    behavior for the first human-versus-NPC slice?
11. **Void warning:** Do you want any warning cue near the existing void-death
    threshold, or should knockback and the void boundary speak for themselves?

Implementation audit rather than product decision: confirm the actual movement
and collider values, input event-order transport, current lag-compensation and
correction owners, arbitrary weapon JSON loader, and hot-reload publication
path before coding. The chosen map-before-intermission flow, random eligible
spawn selection, hazard percent/knockback without hazard kills, per-action
sound defaults/assets, shared lag-compensation policy, and immediate hot-reload
activation are already decided above.

## 56. Starting Melee from the terminal

`startmelee` has two explicit paths. While connected to a room, it requests that
room's server to switch to the stable `melee` ID and begins the usual mode
intermission. While disconnected, it starts the configured `melee_1` server
preset and joins the room after server registration supplies its room code. The
preset provides a default map; choosing a different map remains part of the
normal GUI map-picker flow. A failed preset load, server launch, room
registration, or join must produce a visible failure and structured event; it
must not silently start sandbox. A successful local command handler only means
the request or launch handoff was accepted. Server mode configuration and the
client's connected state are the proof that the requested match actually
started.

Acceptance requires one scenario for the connected mode-switch path and one for
the disconnected start-and-join path. Both verify the server's active mode,
room identity, client connection state, and a roster containing one human and
one Captain Falcon NPC. The scenario must not add an NPC unless it is testing
spawn behavior.

## 57. Authored pose policy and reported follow-up

**DECIDED for first-version Melee:** An actor's selected Melee animation clip is authoritative for its visible rig pose and action timeline. For the first match, both the human and NPC use the Captain Falcon animation profile, with action clips resolvable from the shared animation asset registry. Each fixed tick, the runtime samples the action clip at its elapsed tick, derives windup/active/recovery from its markers, and applies its authored part-local transform relative to the normalized rest pose. This exact sampled pose drives rendering and the corresponding move hitbox attachment.

While a Melee authored pose is active, do not add ordinary idle sway, locomotion/procedural pose, weapon/reload pose, dash/freeze pose, camera/aimbody limb-follow, or other animation overlays to the authored parts. The actor root still moves through the shared movement and collision simulation; animation-local offsets do not silently move the network-authoritative root. Root movement during a move must come from that move's explicit movement/impulse data. A newly accepted action replaces the old clip immediately and starts the new clip at tick zero. Outside Melee, existing animation composition continues unchanged.

The shared animation owner must expose an explicit pose-composition policy so a mode can request this exclusive authored-pose behavior without a second animator. For Melee, the effective priority is: Melee action clip sampled by its elapsed clip tick, then that actor profile's configured idle/locomotion fallback when no action clip is active. A missing Melee clip uses a configured Melee fallback and emits a bounded diagnostic; it must never silently fall through to ordinary aimbody or weapon poses. Euler XYZ and WXYZ quaternion input both convert once to the shared normalized quaternion representation. The exact Blender basis conversion/exporter remains governed by [`../animations/blender-animation-authoring.md`](../animations/blender-animation-authoring.md).

**Live animation edits:** Saving an animation asset installs a valid complete set before actors sample poses on a fixed tick; a bad or half-written save keeps the previous valid set. New actions use the newly accepted clip revision. **OPEN:** whether an already-running action remains pinned to its starting revision (recommended for client/server timing agreement) or adopts the newly loaded timeline at a deterministic shared tick. The selected policy must preserve server/client agreement. See [`../animations/animations.md`](../animations/animations.md) and [`../../architecture/json-configuration/json-configuration.md`](../../architecture/json-configuration/json-configuration.md).

**Implementation audit (source review, 2026-10-18):** The Player runtime now has the exclusive authored-pose path, and the animation registry now checks animation files and includes at fixed-tick boundaries, replaces the cached set in place, and preserves the prior valid set on a rejected save. Build evidence confirms the code compiles; live save-to-visible-update behavior still needs a fresh runtime run. The Captain Falcon JSON remains short of the complete grounded/airborne/landing/shield/grab state set. Blender export is still marked proposal in the authoring spec.

## 58. Shared Force Punch contact and active-hitbox presentation

**DECIDED for Captain Falcon Melee:** Every attack in `captain_falcon_moveset` uses one shared, actor-neutral contact pipeline based on Force Punch's physical-contact behavior. Players and NPCs use the same move definition, active-tick contact query, hitbox visualization, contact-force inputs, and confirmed-result path whether they are attacking or being hit. Human input and NPC decision-making may choose actions differently; they do not select different hitbox, collision, or damage/knockback implementations.

`config/weapons.json` remains the single editable source for the move set. For every move and each active simulation tick, its hitbox data must allow editing the attached named limb, supported shape, shape dimensions (including sphere radius where applicable), and limb-local offset. A hitbox may set base values and provide optional `per_tick` entries keyed by active-phase-local `tick`; an entry overrides only that tick's attached limb, shape, size, or `local_offset`, while omitted entries use the base values. Tick-specific values may change as the animated attack progresses. The move's active phase determines which tick records are evaluated. Do not add a parallel Melee hitbox configuration file or hard-code per-move geometry in C++.

At each fixed tick, derive the hitbox world transform from the attacker's authoritative actor root and sampled animated limb pose, then sweep the configured shape from its previous to current transform. Use the same resolved transform and shape for contact testing and visualization. During each active tick, render the attack volume as a translucent red shape with 0.5 alpha for player- and NPC-originated attacks on observing clients. The visual follows the same limb and tick-specific geometry used by the contact query; a separate visual-only approximation is not acceptable.

**Attachment mode:** Melee hitboxes use `attachment_mode: "animated_limb"`. The server samples the action's animation clip at the same action/phase tick used by the actor pose, resolves the named limb from its normalized rest transform, transforms `local_offset` through that limb's full transform, then applies the actor root position and captured model yaw. This mode is camera-independent: camera position, pitch, and camera-forward vectors do not determine the hitbox center or orientation. The camera may still select action facing or movement impulses where those separate rules say so. A missing clip, part, or rest transform makes the hitbox unavailable for that tick and must produce a bounded diagnostic; do not substitute an estimated shoulder/foot point or camera-relative placement. `camera_basis` is an explicit migration-only mode for legacy definitions. Server contact and client visualization must use the same sampled clip tick, limb transform, and resolved per-tick geometry. The current implementation must be checked against the live runtime; the earlier implementation audit did not prove this contract.

The shared contact result uses Force Punch's normalized contact-center proximity and movement-through-volume force inputs, together with Melee's already specified approach directness. Those inputs feed one shared force calculation for the move's percent and knockback. Melee's result policy adds percent and applies impulse to either player or NPC actors; it does not substitute ordinary health damage. Outside Melee, a reused contact result must still pass through the active mode's damage/result policy. The initial percent-to-knockback tuning targets and the JSON edit location are specified in section 60; the curve remains editable and subject to playtest tuning.

Keep the existing hit-count decision open under section 23: adding per-active-tick geometry does not by itself decide whether one target can be confirmed once per activation or multiple times. All client prediction, server validation, actor generation, move revision, and accepted-result replication continue to follow sections 23–24 and the shared networking contract.

## 59. Confirmed Melee hit feedback and aim policy

**DECIDED:** A confirmed Melee contact immediately produces its single shared gameplay result for every actor pairing, including an NPC hitting a player: add percent to the target and apply knockback once. Predict the result locally, reconcile to the server result, and never apply a second impulse. Reuse the existing world-space damage-number renderer from [`../gui/guiv2.md`](../gui/guiv2.md) with text `+{percent_delta}%`; do not use the health-damage value or create another number system. The popup appears at the confirmed contact point for the attacker, target, and observers, and the target's persistent percent meter updates from that same result. Also show a clearly visible impact sphere, directional burst, and spatial hit sound; their size/intensity should reflect the confirmed hit strength so NPC hits read as hits. Use the reliable confirmed-impact route; do not play gun muzzle, tracer, or gunshot effects for a punch or kick. Presentation must not deal health damage, add a second knockback, or delay control. Hit effect strength and sound volume remain editable through existing hit-effect/audio configuration.

**DECIDED:** Captain Falcon Melee does not use RMB to zoom the camera or point the physical arm. The arm follows the exclusive Melee-authored pose in section 57. RMB remains available as the move-set's configured secondary attack input; disabling aim must not consume or remove that attack binding.

## 60. 2026-10-19 current reports and knockback tuning

**USER-REPORTED STATUS (2026-10-19 16:36–16:39 America/New_York; not independently runtime-verified in this update):** Attack hitboxes are visible when the player hits an NPC. Hit feedback still needs work: hits should immediately cause knockback, show the percent added as a floating number, and update the target's percent. NPC movement appears to use the Melee movement mode now, and RMB camera aiming appears fixed; treat those two reports as solved for now. The server still reportedly crashes when an NPC dies. An NPC hitting the player still does not increase the player's percent or otherwise damage/launch them. Knockback scaling also does not feel like the intended curve. These are current reports, not proof of the failing code path or a verified runtime fix.

**DECIDED initial knockback tuning targets:** Base knockback must increase with the target's current percent and must not collapse to zero for a weak hit. A weak hit at 0% must launch the target at least about 0.5 m. For comparison, use the same standard reference hit at each percent: about 10 m at 100% and about 50 m at 200%. Measure horizontal travel from the contact point through the first 60 server ticks (one second) on a clear, flat lane with the target giving no movement input; repeat the same reference move and contact force at each percent. These are approximate starting targets for playtesting, not a claim about measured current behavior. Contact-center proximity, approach directness, and movement-through-volume force still affect the result; the configured percent curve must be explicit, bounded, and shared for player and NPC attackers and targets. The curve controls launch strength/distance, not ordinary health damage.

**DECIDED percent rule:** Each accepted hit adds the move's configured percent amount unchanged at every pre-hit percent. For example, a move with `base_percent: 20` adds exactly 20 points at 0%, 100%, 500%, and 999%. Target percent scales knockback only. Shielding may reduce received percent as specified in section 29.

**DECIDED movement/knockback rule:** The current `smash` movement preset uses `movement_mode: "mimita"`, which resolves to instant horizontal velocity override. Melee must preserve knockback as continuing motion while still allowing strong WASD steering on ground and in air. On the hit tick and following ticks, input may redirect the controlled movement, but must not zero, replace, or instantly decay the knockback impulse. Keep the impulse and controlled movement composable and apply natural decay/collision rules. Acceptance: after an identical hit, hold and then reverse WASD; confirm the fighter continues to be pushed while input changes their steering, both grounded and airborne, with no hitstun/input lock.

**JSON edit locations:** The shared percent-to-knockback curve belongs in `C:\mimita-v9\config\melee.json`, under `force.knockback_curve`; the current insertion anchor is [`config/melee.json:19`](../../../config/melee.json), where the shared `force` object begins. Store points as `{ "target_percent": number, "distance_m": number }`, with required anchors `[0%, 0.5 m]`, `[100%, 10 m]`, and `[200%, 50 m]`; interpolate linearly between configured points. Keep the weak-hit minimum, reference-hit calibration, and maximum impulse editable in this same object, with a cap high enough not to flatten these anchors. The curve's distance output is a calibration target; the shared movement/physics owner converts it to an impulse and does not treat meters as velocity. Move-specific base percent and base impulse/force scale stay in `config/weapons.json` beside each action; the current move-set anchor is [`config/weapons.json:1690`](../../../config/weapons.json). Shield durability/decay/break values and `shield.visual` color/alpha/shape settings belong in `config/melee.json` under `shield`, whose current object begins at line 6. The JSON key paths are the durable locators because line numbers can move. This update defines required configuration paths; it does not claim the new curve or shield visual fields are already loaded by the runtime.

**Implementation acceptance for the reported failures:** In a live human-versus-NPC match, first hit an NPC with a player move, then have the NPC hit the player. For each server-accepted contact, verify the correct target's percent increases, the same shared knockback calculation moves that target immediately, and the client/server reconciliation does not double-apply the impulse. Verify that the NPC-to-player result is not filtered out or sent through a health-only path. Show the same `+{percent_delta}%` world-space number and hit effect for both directions, using the existing damage-number renderer. Then repeat one identical reference hit against targets at 0%, 100%, and 200%; tune the shared curve until the approximate launch-distance targets in section 60 are met without the cap flattening high-percent hits. During shield tests, verify held tick 1 uses about 0.01x received percent/knockback and bright alpha near 0.5, held tick 299 uses about 0.99x and dim alpha near 0.1, taking shield damage updates both visual color/alpha and remaining durability, and hit-break versus natural-expiry responses follow section 29. In every case, verify there is no hitstun and movement/action input is accepted on the next simulation opportunity. Separately reproduce the NPC-death server crash and capture the live journal/process evidence; a successful hit-result scenario does not prove that crash fixed.

**Still reported broken:** (1) NPC attacks do not apply the player's Melee percent/knockback result or the expected hit feedback; (2) the server crashes when an NPC dies; (3) knockback is not scaling as intended; (4) crouch keeps snapping between crouched and standing, held Space does not reliably produce a higher jump, and Ctrl does not reliably produce the intended faster descent per the 2026-10-19 16:35 note. Reproduce the server crash and NPC-to-player hit in a fresh runtime, capture the live journal and process/crash evidence, and identify the first failing owner before assigning a cause. The reports that hitboxes show, NPC Melee movement works, and RMB no longer changes the camera supersede the older raw reports below for current status; retain those older notes only as historical context.

**Status of raw session notes:** The notes previously pasted below this section were reports and investigation inputs, not requirements already implemented or proven. The authoritative requirements have been moved into the relevant numbered sections. Remaining reports are separated by owner:

- **Melee presentation to implement:** visible shield sphere/effect; the active-hitbox visualization specified in section 58; fuller Captain Falcon limb/leg clips and required idle, locomotion, jump/fall/land, attack, shield, grab, and respawn states.
- **Runtime issues to reproduce:** animation looked too subtle; server reportedly exited and/or stopped advancing near tick 5516; reconnect/leave behavior was unreliable. These require a fresh run journal and process evidence before assigning a cause.
- **Dev-loop issue to investigate separately:** the pasted dashboard snapshot for run `20261018_033521` (build `2518`; journal `logs/2026-10-18/20261018_033521/events.jsonl`) labels the run `mode=1(current)`, says the map is `cramped`, and reports server PID `26612` dead while client PID `21360` is alive. That text proves what the dashboard displayed at that timestamp, not the map actually rendered or the server's later state. See [`../dev-loop/dev-loop.md`](../dev-loop/dev-loop.md); do not treat this snapshot as proof that Melee selected the wrong map or that the server is currently dead.

These reports do not authorize a broad refactor by themselves. First compare the existing owners with the explicit requirements above; make the narrowest owner-level change that closes a demonstrated gap, then use runtime journal and visible gameplay evidence to verify it.

2026 10 18 1119 jojrtodo new issues

stuttering with the  like idle animation its super super chopping back and forth super quick
i want to see the hitboxes if me hitting the npc and npc hitting me
npc wont hit me
need to make sure that we are using the smash movement preset!!!!!!
make an actor preset, and make it so all actors in smash mode are forced to use the smash movement preset
hitboxes for each move arent showing, need to have them show
npcs and players both need to use the smash  movement preset

the ideal beahvior is like
for the punch: like a red 0.5 alpha spehre is wleded to my arm/limb whatever limb is attacking, and it is visible for the time that its active
and so i touch someone with it while its active and we reuse spyknife/force punhc logic so, if target is intersetcting that spehre then that means they are taking percentage knockback and  stuff, like, closer to center of spehre = more percentage knockback and damage, as wel as more dierct angle ,r resuing exisitng force logic

npc need to use the smahs movement preset, and need to be able to edit it live and see the npc use it live
C:\mimita-v9\config\npc-difficulty.json
this json file i think is messing with things  like 
i think we have too much things editing the state of npcs or  actors etc 
just npcs actuallt
but
idk what even to do about it differnetly
if i get hit as the captian falcon character nad i move like a little like i have the suttteringhappenign on me right now 2026 10 18 1122 
test=2561  match=True
Server   : alive  pid=27448  uptime=00:00:38  build=2561
Client   : alive  pids=[14004]  uptime=00:00:35
Run      : id=20261018_152210  mode=1(current)  visibility=visible  map=cramped
Last     : published build 2561 at C:\mimita-v9\.dev\builds\2561
Journal  : C:\mimita-v9\logs\2026-10-18\20261018_152210\events.jsonl
View     : launch mode 1 requested
Controls : [1] build/launch  [2] launch mode  [V] client visible/headless  [A] auto-restart  [Q] stop daemon  [X] close view  [P] freeze  [R] resumeop activity)
not sure if the events.jsonl would even show tho
butits like every frame back and froth almost 

cant hva that , just need to make it like
no input? e.g. no wasd no jump nonothing = idle
idle animstate persists thru momenutm tho so u can move and stop moving/wishing to move and the idle animation is playing, i like that, animations that  show ur intent and animations that continue even if ur not doing it etc

also in the smash movement mode, 
q = shield
e = grab
left ctrl = go down/crouch
space = go up, held space = more higher jump, tapped space e.g. onl pressing for 1 tick = minimum jump 
right now that behavior doesnt work, we need to like  rebind the  keys
maybe a binds system not sure ? 
thatd benice tho 

$ python build_agent.py 2>&1 | Select-Object -Last 3; python devscripts/dev-loop.py --request launch --json 2>&1 | Select-String -Pattern 'accepted' | Select-Object -First 1; Start-Sleep -Seconds 55; python devscripts/dev-loop.py --scenario-file C:\mimita-v9\devscripts\scenarios\melee_probe_v1.json 2>&1 | Select-Object -Last 1; $d=(Get-ChildItem 'C:\mimita-v9\logs\2026-10-18' | Sort-Object LastWriteTime -Descending | Select-Object -First 1).FullName; $f="$d\events.jsonl"; echo "run: $f"; echo "crouch clip: $((Select-String -Path $f -Pattern 'melee.authored_pose.*\"clip\":\"crouch\"').Count)"; Select-String -Path $f -Pattern 'anim.melee.pose_probe' | Where-Object { $_.Line -match '"actor":"NPC Dev"' } | ForEach-Object { if ($_ -match '"crouching":(\w+)') { $matches[1] } } | Group-Object | Select-Object Name,Count


None
[BUILD LOCK] released

  "accepted": true,
[DEV SCENARIO] events=C:\mimita-v9\logs\2026-10-18\20261018_152357\events.jsonl
run: C:\mimita-v9\logs\2026-10-18\20261018_152357\events.jsonl
crouch clip: 2

Name  : false
Count : 15


Name  : true
Count : 2

idk what  happened but this just happened and then i have a issue sauing Reconnecting attempt # 3 15.21 like  it just crashed the server and now i get no  packet updates frmo the server 

from gpt 6 luna: "The animation flips because it uses the raw grounded flag, which the movement code sets from that tick’s collision result. A separate stableOnGround flag has a grace period, but the Melee animation selector ignores it.
In the journal, crouch starts at tick 3150 with vertical velocity 0; on the next tick the animation switches back to fall, with vertical velocity -0.667. That fall/crouch alternation repeats. The snapshot labeled crouch_check shows on_ground=false, stable_on_ground=true, and the active animation is fall."

we should change it to  using stableonground flag 
its cuz  in the game it swithces super quick back and forth from grounded to nto grounded liek thats been athing for idk liek 2 or 3 weeks but i just didnt   think it was a big deal 

2026 10 18 1202 does  C:\mimita-v9\assets\avatars\captainfalcon\animations.json this update live?
needs to udpate live
cannot have it  be a rebuild 
bc its animations and doing numbers to visuals its not so  like 
it needs super quick feedbakc 

2026 10 18 1244 
C:\mimita-v9\config\movement\movement-smash.json 
this isnt  like
idk 
i dont htink this is being used 
animatiosn started updateing live tho we fixed that i think 
we also need to make it like
per gamemode or per actor mode like 
rmb down should not aim in  for melee mode
like, we need to have that disabled in the gamemodeperset 
C:\mimita-v9\config\gamemodes\melee.json
like here put like
aimbdoy.json fov enabled = false
like rmb down does not haev a effect on ur fov 

also crouching isnt working how its intended

hitboxes dont show, weneed to shwo htiboxes for each move every singel moveneeds to have a hitbox like on the limb that is moving and needs to be editable its offset at each tick of the move being active, its position  its size etc all editable per tick
so like tick 1 of a punch its like radius 0.5m and offset 0.5 meter from hand, then tick 2 its radius 0.7 offset 0.7 meter from hand, on adn on 

2026 10 18 1255 how do we make it so all the attacks for smash the captain falcon weapon is using force punch logic for all of it 
so like active ticks, sphere radius, offset, etc 

### 2026-10-18 13:09 America/New_York — Jorj Melee issue report

- Crouching does not work as intended. A server position error was also seen;
  its cause is unknown.
- Right mouse currently aims in during Melee. Desired behavior: disable aiming
  for this mode so right mouse does not change the field of view or move the
  right arm.
- Shield and grab need visible effects comparable to the freeze effect. The
  shield, grab, and every attack need a visible hitbox during their active
  behavior.
- Holding jump does not work reliably. The reported desired behavior is for
  held-jump input to be handled on the client because the server feels too slow;
  the implementation and authority details still need investigation.

### 2026-10-18 13:17 America/New_York — Jorj live Melee report

- During the current play session, attack hitboxes are visible while hitting an
  NPC, but attacks that appear to connect produce no damage, percentage, or
  knockback. The attack-result/contact path needs investigation so visible
  contacts produce the intended Melee result.
- Collision problems were also observed on `cramped`: getting caught in walls
  and phasing through them. The map may make the issue more noticeable, and the
  current collision behavior is no longer physical-mode collision. These are
  observations only; the cause is unknown.
- **Current priority:** focus on making attacks produce their intended results.
  Leave the wall and collision issues for later investigation.

2026 10 18 if the npc dies, then the servre crashes 

2026 10 18 1515 - npc punches me and i dont have percentage incereases or antthing, also npc damgee doesnt  mkae a sound iw ant ot hear the sounds from the npcs as well and other plaeyrs as well 

2026 10 19 1635 - ctrl down should  be like go down faster , smae as in smash, also, as of right now the  hold space = higerh jump isnt working
also crouch
crouch doesnt work it just snaps back and forht between standing not stnaindg etc 


2026 10 19 1837 - frmo deepseek v4.1 flash "Distinct knee, elbow, up-B, side-B, and down-B clips are still missing"
