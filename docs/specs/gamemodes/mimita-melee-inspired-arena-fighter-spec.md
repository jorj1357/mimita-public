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

**DECIDED:** Ring-outs are a core win path: knock an opponent beyond the arena boundary / blast zone. The user approved a configurable arena rules direction. **PROPOSED:** Define stage geometry and explicit ring-out boundaries in stage data. **OPEN:** whether falling below the stage, crossing invisible bounds, hazards, or a combination causes a ring-out; warning cues; and how recovery is judged before crossing the boundary.

## 32. Match scoring and respawn

**DECIDED:** Match rules should be configurable per match. The initial preferred match was described as ring-outs and “first to 20 kills,” then the user referenced stocks and agreed that the mode should support configuration. This is not a final scoring contract. **OPEN:** stocks versus score-to-target, meaning of “kill,” target default, tie handling, instant respawn versus delay, spawn invulnerability, and percent reset on respawn. Do not encode “20” as a firm default without confirmation.

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

**OPEN before a firm implementation contract:** movement owner and acceleration model; dash/double-tap bindings; wave-dash mechanics; attack direction mapping and camera pitch; exact Captain Falcon move list and phase timings; sweep shapes and repeat-hit policy; percent formula and mass interaction; shield controls/durability/break; whether grabs ship and their escape rules; recovery and air-jump limits; blast zones, hazards, score/stocks, respawn and defaults; Blender export schema and quaternion JSON version; lag compensation rewind/fairness limits; and exact data, packet, and logging contracts. Work may proceed on specified contracts without pretending these are settled.

## 50. Implementation and rollout gates

**PROPOSED sequence:** (1) audit current movement, animation, collision, combat authority, and network owners; preserve evidence and current unrelated behavior; (2) settle the open decisions needed for a minimal Captain Falcon move set; (3) define and validate tick-based move data and deterministic phase/cancel semantics; (4) connect shared animation and Blender-to-quaternion authoring without changing legacy Euler interpretation; (5) implement multi-tick attack sweep and authoritative contact; (6) add percent, knockback, immediate control, ring-out, and configurable match lifecycle; (7) add prediction, bounded lag compensation, rollback, and reconciliation; (8) run remote-player and NPC acceptance scenarios; (9) add shield/grab/hazards only after their contracts are decided. No stage is claimed complete by this document.

---

## Source alignment notes

- The initial conversation proposal establishes the 3D arena scope, fixed-tick phases, percent/knockback, responsive control, NPC participation, and separation from generic weapon contracts.
- The user's later answers establish the preferred fast movement-tech feel, move-specific movement restrictions, immediate attack restart without buffering, multi-tick shape sweeps, hybrid physical/percent/move knockback, no hitstun, tentative shield/grab direction, configurable ring-out rules, prediction plus rollback, and reusable NPC/cross-mode move sets.
- The shared animation specification defines an existing *target contract* for shared rigid-part animation, fixed-tick evaluation, state transitions, and fallback behavior; it says Blender action import is outside its scope. This document's quaternion Blender path is therefore an extension/proposal, not a statement that importer or runtime support already exists.
- A transcript expresses intent and decisions. It does not prove implementation or runtime acceptance.
