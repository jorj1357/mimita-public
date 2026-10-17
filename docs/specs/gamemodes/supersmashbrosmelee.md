2026 10 08 1228 jorj todo explainbetter

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

Status: `PROPOSAL — NEEDS_SPEC_DECISION` for the rules still listed below.

## Purpose and boundaries

This is a 3D, tick-synchronized arena-fighting mode inspired by the movement
and readable attacks of Super Smash Bros. Melee. It is not a 2D fighter and is
not intended to copy every Melee rule. The existing force-punch weapon is a
possible movement/combat prototype, not proof that the full mode already
exists. Weapon-specific contracts remain in the weapon specifications.

## Proposed core rules

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

## Decisions still needed

- What exact movement behaviors are essential to the Melee feel, and which
  existing movement systems should they reuse?
- Is hitstun absent, very short, or replaced by a recoverable knockback state?
- Are shields, grabs, dodges, cancels, ledge recovery, and aerial recovery in
  the first version?
- How do percent, knockback, ring-outs, stocks, and respawns work?

Do not resolve these open questions by silently choosing implementation
defaults. Keep this mode separate from generic Spy Knife, Godball, HAFS, and
Swordsword damage contracts.

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

**DECIDED:** Moves have explicit windup, active, and recovery phases with state lasting a known number of simulation ticks. The example **10 windup / 5 active / 15 recovery ticks** comes from the initial request and is a starting example, not a universal move duration. The later **8 / 10 / 17** example is also an illustration, not a selected punch definition.

## 8. Attack phase model

**PROPOSED:** Represent a move instance with move identity, actor generation, start tick, current phase/tick, and any move-specific movement and contact parameters. Define phase intervals unambiguously, for example as offsets from a start tick, so server and clients agree on the first active tick and the first recovery tick. The exact wire schema and interval convention remain open.

## 9. Instant attack restart and cancellation

**DECIDED:** A new attack input can immediately override the currently playing attack, including before its active phase. The user's example: issuing another attack at tick 7 while a move is approaching active restarts into the newly selected move at its beginning. There is no attack-input buffer: the new input acts now or is rejected by a clearly defined rule; it is not queued for later. This restart behavior overrides any ordinary recovery lock for attack-to-attack transitions. **OPEN:** whether non-attack actions (shield, jump, dodge, grab, hit response) can interrupt every phase, and whether same-move re-input restarts or repeats under a separate rule.

## 10. Animation versus action authority

**DECIDED:** Animation must not be the authority that makes a player helpless. Action phase timing is simulation state; pose playback presents it. A new legal action can replace the prior pose and phase immediately. Any transition restriction must come from an explicit action rule, not an animation clip finishing.

## 11. Movement feel

**DECIDED:** Ordinary WASD movement should be straightforward, while skilled combinations and timing tricks can produce much faster movement, evoking techniques such as Yoshi's or Luigi's movement. This is a feel and capability requirement, not a selected speed formula or a claim about exact Melee physics.

## 12. Movement model

**OPEN:** Whether to reuse a current MiMITA movement preset, introduce a dedicated mode, or build a separate movement kernel. The user is uncertain whether Source-like movement will fit. Compare acceleration, immediate directional response, air control, momentum retention, and predictable rollback before selecting an owner. Do not silently set acceleration to an extreme value or claim input-to-velocity is instantaneous.

## 13. Dash and dash dancing

**PROPOSED:** Support quick direction changes and repeated dashes as deliberate movement tools. The user floated double-tapping a direction (including an illustrative 30-client-tick window) and holding Shift as alternatives. Neither binding nor timing is decided. **OPEN:** choose one control scheme, define tap window, dash duration, reversal behavior, cooldown or resource if any, and accessibility options after playtesting.

## 14. Wave-dash and technique combinations

**DECIDED:** The desired system should permit movement tricks and combinations that reward player skill and create speed. Wave-dashing is explicitly of interest. **OPEN:** exact inputs, physics, timing window, momentum transfer, whether the technique requires an airborne/ground contact sequence, and how it is taught or surfaced. Do not label a mechanic “wave-dash” until its behavior is specified and accepted.

## 15. Air movement and recovery

**PROPOSED:** Include aerial control and recovery options that let a player attempt to return after a launch. In the prior discussion, one air jump and an up-special recovery were selected as a preferred direction, while an upper bound on repeated recovery success was described as a goal. **OPEN:** whether those are firm first-version requirements; exact number of jumps, recovery actions, resource/cooldown, and skill curve. The illustrative “about ten” repeated recoveries is not a firm cap.

## 16. Per-move movement restrictions

**DECIDED:** Each attack has its own movement restrictions; attacks should generally allow some movement instead of freezing the actor by default. **PROPOSED:** Make movement permission, steering, root displacement, and momentum effects explicit per move and per phase. **OPEN:** units, allowed ranges, and whether restrictions apply identically on ground and in air.

## 17. Camera and body orientation

**DECIDED:** Physical positioning matters. A player should need to align the character and attack volume with the target; looking at a distant target must not make a punch home or hit it automatically. The user considered both camera-directed attacks and fixed forward-facing attacks, then emphasized physically positioning to connect. This specification resolves the tension by distinguishing aim intent from contact: camera input may orient a pose or select a direction, but confirmed hits still require the authored/simulated attack volume to overlap the target in world space. **OPEN:** whether camera pitch changes attack direction in the first version.

## 18. Directional attack choices

**PROPOSED:** Resolve attack variants from move input and actor-relative direction (for example neutral, forward, back, up, down, and aerial variants), while keeping exact mapping configurable. The user wants WASD movement to influence attack selection, citing neutral/side/back/air attacks, and wants left click for normal attacks and right click for specials. **OPEN:** the precise mapping, whether direction is sampled on button-down or throughout windup, and how conflicting camera and movement directions are resolved.

## 19. Mouse and camera aiming

**OPEN:** The user discussed punches following camera pitch and limbs physically aiming with mouse/camera, but also expressed preference for fixed, forward-oriented attacks where spacing and alignment determine hits. A first implementation should not silently select unrestricted look-to-hit targeting. Decide whether to support: horizontal-only attack families, limited up/down variants, or continuous camera-directed pose/contact. Any option must retain physical range and collision checks.

## 20. Inputs and controls

**DECIDED:** Left mouse is the normal attack and right mouse is the special attack in the preferred PC control direction. **PROPOSED:** Movement plus attack chooses variants; Q was suggested as a possible shield binding and E as a possible grab binding, but neither is approved. Shift dash/sprint and double-tap dash are competing proposals. Publish a complete control map only after all required actions and conflicts are decided.

## 21. Move definition data

**PROPOSED:** Define each move as data with stable ID, input/variant mapping, phase lengths, animation references, movement policy, one or more attack shapes, per-tick shape transforms, damage/percent contribution, knockback parameters, and sound/effect references. Keep data declarative and validated; do not create arbitrary executable JSON scripting. Exact schema and hot-reload policy remain open.

## 22. Attack geometry

**DECIDED:** Use shapes and sweeps over multiple active ticks, rather than a single one-tick sphere check for every attack. The current Force Punch sphere behavior was cited as a useful starting idea: a spatial volume can contact targets and contact location may matter. **PROPOSED:** Support move-authored capsules, spheres, boxes, or other already-supported collision shapes, sampled along the attack's tick timeline. **OPEN:** supported shape set and whether distance-to-center scales damage, knockback, or only hit location.

## 23. Active-window hit sweep

**PROPOSED:** For each active tick, sweep the authored attack volume from its prior transform to its current transform, test against eligible target hurt volumes, and produce candidate contacts. This reduces tunneling and makes multi-tick active windows meaningful. Deduplicate repeated overlap according to an explicit per-move hit policy. **OPEN:** one-hit-per-activation versus multi-hit, per-target re-hit interval, and collision ordering when multiple targets overlap.

## 24. Contact and hit result

**PROPOSED:** The client may predict visual contact immediately, but the authoritative simulation decides whether a hit is valid and applies gameplay results once. A contact result should identify attacker, target, move instance, attack tick, contact point/normal, and outcome sufficiently for reconciliation and debugging. Exact packet/event names and contact tolerances are unselected.

## 25. Percent meter

**DECIDED:** Percent replaces ordinary health as the primary fighter damage display and contributes to knockback. The user proposed a linear relationship, describing 300 percent as about three times the knockback of 0 percent. **PROPOSED:** Keep percent as persistent match state and make each move's contribution distinct. **OPEN:** whether percent increments equal a move's damage value, and the precise formula, clamps, display precision, and reset conditions.

## 26. Knockback calculation

**DECIDED:** Knockback should combine move-specific force, percent, and physical properties such as character heaviness; it should not be only a canned hitstun reaction. A heavier character may attack with greater knockback but may have slower attacks. The user explicitly prefers this hybrid physical/percent/move-specific direction. **OPEN:** formula, mass semantics, scaling curves, caps, and whether heavier characters trade speed for force by default or only when authored.

## 27. No hitstun and immediate recovery

**DECIDED:** No hitstun. A player must be able to recover from knockback immediately and regain control; hits must not put the actor into a forced helpless animation/state. **PROPOSED:** Apply an impulse or velocity change while continuing to accept movement and action input on the next simulation opportunity. **OPEN:** how simultaneous directional input, defensive actions, collision constraints, and repeated hit impulses combine. Do not reintroduce hitstun under another name.

## 28. Hit reaction presentation

**PROPOSED:** Play a brief readable reaction pose while preserving control, with animation interruption semantics matching Section 9. The pose may be blended or overridden by the chosen action. **OPEN:** whether the reaction uses impact direction, magnitude, body part, or hit tier, and whether it is an overlay or a replaceable clip. Presentation must never silently delay movement or attack acceptance.

## 29. Shields

**PROPOSED:** Include a shield with durability and possible break behavior. The user wants shielding to remain an active choice rather than a passive wait state, and suggested that a few normal attacks might break it. **OPEN:** shield input, activation latency, mobility while shielding, durability, regeneration, break penalty, and whether shield damage is based on percent or a separate meter. “A couple hits” is qualitative intent, not a chosen number.

## 30. Grabs

**PROPOSED, strongly qualified:** A short grab could exist, with an illustrative maximum around 60 ticks; earlier examples mention 60 or 66 ticks, so no duration is selected. One example proposed a 120-tick re-grab cooldown. The user dislikes helpless locks and wants escape through movement/momentum, such as looking or moving away from the grabber's direction. **OPEN:** whether grabs ship at all, their start/capture/escape/throw rules, cooldown, and maximum restraint. If implemented, grab must be escapable through active play and cannot contradict the no-helplessness requirement. Camera-swing throwing is PC-only proposal; console/mobile behavior is later unresolved work.

## 31. Ring-out and arena bounds

**DECIDED for the first version:** A ring-out happens when knockback carries a fighter past the stage's lethal blast zone and they die in the void. This is the only way to lose a life. **PROPOSED:** Define the lethal blast-zone bounds in stage data and let the server decide when the fighter has crossed them. Hazards do not cause death in this version. **OPEN:** bounds, warning cues, and the time/recovery policy before crossing the lethal boundary.

## 32. Match scoring and respawn

**DECIDED for the first version:** Each fighter starts with four lives. A fighter loses one life only by being knocked out of the stage and dying in the void beyond its blast zone. When lives remain, the fighter respawns; losing the fourth life eliminates them. Ordinary health damage, hazards, or another cause must not kill a fighter in this version. **OPEN:** respawn delay and location, spawn protection, and whether percent resets on respawn. Do not carry the earlier “first to 20 kills” idea into this first-version ruleset.

## 33. Stage and hazards

**PROPOSED:** Reuse existing arena maps where suitable; a large enclosed arena or a stage like the train map was called promising, and Final Destination was given as a shape/reference. These are examples, not map approvals. **OPEN:** first map, hazards, moving platforms, blast-zone dimensions, safe spawn points, and whether hazards are disabled in the initial mode.

## 34. Player and NPC participation

**DECIDED:** NPCs should use the same move-set capabilities and combat path as players. The user's desired abstraction is a reusable attack set that can work against an NPC or a player using another style of combat. **PROPOSED:** Distinguish only the decision/input producer (human controls versus NPC policy); share move resolution, animation, sweep, percent/knockback where the mode supports it, and authoritative contact handling. **OPEN:** how fighter percent and ring-out translate into ordinary modes that use health or different scoring.

## 35. Cross-mode reuse contract

**PROPOSED:** Package reusable move definitions and presentation separately from mode policy. A move can specify attack behavior; a consuming mode decides whether a confirmed hit applies percent, health damage, stagger, or another allowed result. Reuse must pass through existing combat authority and weapon/action identity contracts. Do not assume that fighter percent rules globally replace ordinary health or weapon behavior.

## 36. Animation system relationship

**DECIDED:** Use the shared avatar/actor animation system rather than a fighter-only animation runtime. Existing animation specification describes rigid multipart actors, a shared registry/state machine/runtime, fixed-tick evaluation, reusable clips, and a live-edit path as a specification; it explicitly does not define Blender action import. This fighter spec adds move/action synchronization requirements but does not claim that the described animation features are all implemented.

## 37. Blender authoring direction

**DECIDED:** Blender is the intended authoring tool for attack and movement poses, with exported data used by the game. The user specifically favors authoring orientation as quaternions and evaluating arm/body rotations as quaternions in-game, avoiding accumulated Euler rotation. **OPEN:** Blender exporter, supported rig profile, action naming, channels, coordinate conversion, export validation, and whether curves are baked to fixed-tick samples or retain interpolation metadata.

## 38. Quaternion pose representation

**PROPOSED:** Store local part orientation as a normalized quaternion relative to the normalized rest pose and part pivot; interpolate orientations with spherical interpolation, with deterministic normalization and a defined antipodal-sign rule. Preserve root origin and express part translation offsets relative to that origin/rest frame. This is a design recommendation, not an already-approved JSON format. The existing animation spec's Euler channel contract would need a versioned extension or migration; do not silently reinterpret old Euler data.

## 39. Pose offsets and root motion

**DECIDED:** The player-origin point should provide a stable reference, with animation using offsets from it rather than repeatedly accumulating transforms. The user also wants the approach to be general enough for weapons/tools and other animatable objects. **PROPOSED:** Keep gameplay root motion owned by movement/action simulation; treat authored part/object transforms as local offsets unless a move explicitly defines deterministic root displacement. **OPEN:** object attachment schema, pivot conventions, scale handling, and whether any attack uses authored root motion.

## 40. Animation phase alignment

**PROPOSED:** Bind a move instance's start tick to its attack clip start tick. Author or map poses for windup, every active contact interval, and recovery. A cancel/restart begins the replacement clip at its defined initial pose on the same action-start tick. Animation duration need not equal action duration, but the relationship must be explicit and must not affect combat timing.

## 41. Effects and audio

**PROPOSED:** Trigger sounds and effects from deterministic move events or tick markers so predicted and authoritative playback can be reconciled and duplicate effects avoided. Existing Captain Falcon sounds are user-reported as present in the repo; inventory and rights are unverified. **OPEN:** per-move sound/effect mapping, predicted playback policy, cancellation behavior, and asset readiness. Cosmetic effects do not confirm a hit.

## 42. Authority and replication

**DECIDED:** Combat state must be synchronized between client and server so players can fight remotely. The earlier proposal requires action identity/start tick and authoritative hit/result state to replicate; that remains the foundation. **PROPOSED:** Server owns confirmed percent, valid hit results, knockback state, ring-outs, and match score. Clients may predict movement, action animation, attack volumes, and tentative contacts. Exact ownership boundaries must be reconciled with current MiMITA networking contracts before implementation.

## 43. Client prediction

**DECIDED:** Attacks should be predicted locally with no perceptible wait for the server: on click, the local animation/action begins immediately at the client's current predicted tick. The user explicitly wants fully predicted attacks and low-latency-feeling contact. **PROPOSED:** Predict move selection, phase timing, local motion, and candidate hits; mark predicted outcomes provisional until authority confirms them. Prediction must not allow a client alone to award persistent percent or ring-outs.

## 44. Lag compensation and fairness

**DECIDED:** User wants lag compensation and says a hit that appears valid on the attacker's screen should count, but rejects hits from implausible distances and wants little leeway. These are goals in tension, not a finalized policy. **OPEN:** which historical state is queried, maximum rewind, clock/tick validation, target movement reconstruction, and fairness under differing ping. The user's “100 ms behind” and “500 ms or more” examples describe concern/experiments, not approved latency thresholds. Do not implement unbounded client-truth hit acceptance.

## 45. Rollback and reconciliation

**DECIDED:** Use prediction with rollback and replay after authoritative corrections (the user selected this direction in the prior question set). The user hopes reconciliation will be rare, but accepts the need. **PROPOSED:** Keep a bounded history of inputs and relevant combat state; on correction, restore an authoritative snapshot and replay. Define visible correction behavior for move choice, contact, percent, velocity, and ring-out separately. **OPEN:** history length, snapshot cadence, correction smoothing, and deterministic state coverage.

## 46. Network state contract

**PROPOSED:** Replicate enough stable state to reconstruct actors: actor generation, authoritative simulation tick, action/move identity and start tick, phase, movement state, percent, knockback velocity/state, and confirmed result/score changes. Animation can be re-evaluated from the shared clip plus tick where deterministic; do not add per-bone packets by default. Validate avatar/move content revisions so clients do not interpret an action with different data. Packet shape and cadence remain open.

## 47. Testing strategy

**PROPOSED:** Test the rules at three levels: deterministic unit/model tests for phase boundaries, cancellation, sweep geometry, percent/knockback, and rollback; multiplayer integration tests for server authority and correction; and live human play sessions for speed, readability, fairness, and whether no-hitstun combat feels controllable. A build or synthetic test alone is not proof of satisfying the feel requirements.

## 48. Required acceptance scenarios

**PROPOSED acceptance gates:** (1) two remote players select the same move and agree on start/end ticks and confirmed results; (2) a predicted visual contact rejected by authority does not become confirmed percent; (3) a player can move, attack, be launched, recover immediately, and re-enter without unintended input lock; (4) attack restart during windup replaces the current attack immediately without buffering; (5) multi-tick sweep catches valid crossings without duplicate unintended hits; (6) NPC uses the same move/action path; (7) reusable move set works in at least one non-fighter context without changing that mode's damage contract; (8) Force Punch remains independently usable outside the mode; (9) quaternion-authored clip round-trips and is stable on clients. These are future acceptance criteria, not completed results.

## 49. Open decision register

**OPEN before a firm implementation contract:** movement owner and acceleration model; dash/double-tap bindings; wave-dash mechanics; W/A/S/D-to-move mapping; exact Captain Falcon move list and phase timings; sweep shapes and repeat-hit policy; percent formula and mass interaction; shield controls/durability/break; whether grabs ship and their escape rules; recovery and air-jump limits; blast-zone bounds; respawn delay/location, spawn protection, and percent reset; Blender export tool and validation; lag compensation rewind/fairness limits; and exact data, packet, and logging contracts. The first-version rules already decided are four lives, void ring-out as the only death cause, and camera-horizontal-forward attack orientation. Work may proceed on specified contracts without pretending these open details are settled.

## 50. Implementation and rollout gates

**PROPOSED sequence:** (1) audit current movement, animation, collision, combat authority, and network owners; preserve evidence and unrelated behavior; (2) settle the remaining decisions for the minimal Captain Falcon move set; (3) define and validate tick-based move data and deterministic phase/cancel semantics; (4) implement the shared versioned Euler/quaternion import and quaternion runtime contract; (5) implement multi-tick animated-limb sweeps and authoritative contact; (6) add percent, knockback, responsive control, and the four-life void ring-out lifecycle; (7) add prediction, bounded lag compensation, rollback, and reconciliation; (8) run remote-player and NPC acceptance scenarios; (9) add shields/grabs only after their contracts are decided. No stage is claimed complete by this document.

---

## Source alignment notes

- The initial conversation proposal establishes the 3D arena scope, fixed-tick phases, percent/knockback, responsive control, NPC participation, and separation from generic weapon contracts.
- The user's later answers establish the preferred fast movement-tech feel, move-specific movement restrictions, immediate attack restart without buffering, multi-tick shape sweeps, hybrid physical/percent/move knockback, no hitstun, tentative shield/grab direction, prediction plus rollback, reusable NPC/cross-mode move sets, an always-active Melee move set, camera-forward attack orientation, and four lives with void ring-out as the only death cause in the first version.
- The shared animation specification defines an existing *target contract* for shared rigid-part animation, fixed-tick evaluation, state transitions, and fallback behavior; it says Blender action import is outside its scope. This document's quaternion Blender path is therefore an extension/proposal, not a statement that importer or runtime support already exists.
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
- The mode owns the allowed fighter/move-set combination. A player's move
  inputs resolve to stable action IDs in that move set. A fighter does not
  switch through ordinary inventory weapons during a match.
- **DECIDED:** the selected move set is always active for the whole match; the
  player never equips or re-equips it. If the current input/UI layer requires
  a weapon slot, slot 1 may be a presentation/input binding only; it must not
  be the server's identity or authorization for an attack. The
  authoritative identity is the fighter profile, move-set ID/revision, and
  action ID. This follows the networking spec's rule that a hotbar index is a
  selection position, not weapon identity.
- Keep fighter actions separate from ordinary weapons and their damage rules.
  Reuse shared actor, animation, collision, and authoritative combat owners;
  do not create one C++ weapon-definition function per fighter move.

### Initial input and move roster

The following is a proposed first control map, not a final layout. An attack
starts an authored move animation; the animation moves the fighter's parts and
therefore moves the attached hit shapes. Mouse/key input chooses a move ID; it
does not directly place or aim a hitbox. Directional input is sampled with the
attack press at a simulation tick and cannot change that move instance later.

| Input | Proposed move | Required behavior |
|---|---|---|
| Left click, no direction modifier | `a_neutral` | Quick punch; short startup/recovery. |
| Left click + W | `a_forward` | Forward punch, as the user's current example suggests. |
| Left click + A/D | `a_side` variants | Side-directed punch; exact facing-relative mapping is open. |
| Left click + S | `a_down` or `a_back` | **OPEN:** user has not chosen between down and backward. |
| Right click, no direction modifier | `b_neutral` / Falcon Punch | Long windup and recovery; high knockback. |
| Right click + direction | `b_up`, `b_side`, `b_down` | Up-B, side-B, and down-B candidates; exact mapping and move designs are open. |
| Jump input | `jump` | Uses shared movement authority and the fighter's jump animation. |

Each move definition needs, at minimum, a stable ID, character/move-set ID,
input mapping, animation clip ID, startup/active/recovery tick counts,
movement permissions by phase, per-tick hit-window definition, limb-bound
hit shape, percent/knockback parameters, cancel/interruption rules, and
content revision. The current `10/5/15` tick example is a starting tuning
value only; do not apply it to every move by default. A move's phase clock and
clip start on the same authoritative action-start tick. Active ticks are
inclusive/exclusive by one explicitly documented convention, so clients and
server cannot disagree at phase boundaries.

### Facing and third-person camera

**DECIDED for the first version:** third-person attack-forward follows the
camera's horizontal forward direction. At the action-start tick, capture that
horizontal direction as the move's facing basis and replicate it with the
action state; the animation then drives the limb path and attached hit shape.
The camera's vertical pitch does not directly tilt an ordinary punch hit
shape. An authored move such as up-B may use its own animation and move data
to travel vertically. Directional input may choose a move variant relative to
the captured camera-forward basis, but the exact W/A/S/D mapping remains
**OPEN**. Movement and body turning may use separate rules; only attack-forward
is fixed by this camera rule.

### Animation and limb-bound hitboxes

- Use the shared animation registry/runtime and the Blender authoring route in
  [`../animations/animations.md`](../animations/animations.md) and
  [`../animations/blender-animation-authoring.md`](../animations/blender-animation-authoring.md).
  Required state clips include idle, walk, run, jump, fall, land, punch,
  Falcon Punch, kick, knee, elbow, up-B, side-B, and down-B as the roster is
  finalized. Missing clips must use the shared fallback behavior, not a
  Melee-only animator.
- Attack hit shapes attach to named rig parts such as `leftHand`, `rightHand`,
  `leftFoot`, or `knee`. At each fixed tick, derive their world transforms
  from the authoritative actor root and that tick's evaluated pose. Test
  contact only during the move's active windows; use the specified multi-tick
  swept-volume rule so fast limbs cannot tunnel between ticks. Apply the
  existing rig rule that gameplay collision requires a declared physical part.
  A visible pose alone is never proof of a hit.
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
knockback according to a still-open formula. In this first version, the server
detects when knockback carries a fighter beyond the stage's lethal
`blast_zone`, then records one void death and removes one of the fighter's
four lives. Respawn occurs while lives remain; no other event may kill the
fighter. Blast-zone bounds, respawn delay/location, spawn protection, and
percent reset remain open details.

### Network and replay contract

Follow [`../networking/networking.md`](../networking/networking.md): the client
may immediately predict the selected move and its animation, but the server
validates fighter/move-set/action identity, actor generation, match membership,
and the mapped input at a validated tick. The server advances action phases
and hit shapes on the fixed 60 Hz simulation, confirms each permitted contact
once, and owns percent, knockback, ring-out, stock, and score results. Clients
reconcile provisional visuals/results to authority. Do not send per-limb pose
packets by default; replicate the action ID/revision, authoritative start tick,
and the state needed to reproduce the shared clip and confirmed result. The
networking spec's tick/history policy governs any lag compensation; do not let
a client pose or client-reported hit award percent.

Required first-slice scenarios:

1. Two clients with the same fighter/move-set revision select `a_neutral` and
   `b_neutral`; their action IDs, phase boundaries, confirmed contacts,
   percent, and knockback agree after reconciliation.
2. Change the visible hotbar selection or respawn the actor; a legal fighter
   action remains available without re-equipping a weapon, and a stale
   previous-generation action is rejected with a reason.
3. Sweep a limb through a target during an active tick, then repeat outside
   the active window; only the valid contact changes authoritative percent.
4. Launch a fighter through the configured blast zone; the server records one
   ring-out and applies the selected stock/respawn rule.
5. Run the same move through a player and NPC and confirm both use the same
   move, animation, hit-confirmation, and result path.

This is a specification target only. Acceptance still requires the runtime
scenario and live journal evidence described by the router's workflows.
