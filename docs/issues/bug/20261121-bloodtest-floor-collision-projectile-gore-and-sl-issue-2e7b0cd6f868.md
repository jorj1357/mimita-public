# issue-2e7b0cd6f868: Bloodtest floor collision, projectile gore, and slow movement behavior

## Human report (verbatim, immutable)

> how cmoe bloodtest i fall thru the ground acn u make a fix is there enoguh info
> 
> also the hit effect is not explsiion for bloodtest rveovler and shotgun&#x20;
> 
> so the focus for bloodtest is to make it so u shoot and it has skin , meat, bone
> 
> liek a gore/viscera  experimentation&#x20;
> 
> so the guns should be preojectile all thigns are projectile, and the damage should do blood and stuff and based on how much force it does more damage and shows meat or bone, or explodes the body part in big blood effec tihng
> 
> but cant fall thur the floor,
> 
> also the movement mode for this is aslow, we need to use like a differnet one like, movement-heavy but a clone that is differnet and its for this mode and need somethign for slopes bc slopes are slow like u cant walk up slopes in the hevay mode but need to be slow and slppes still work idk


## User quote

### 2026-11-21T23:48:03.829Z — bloodtest-mode-expansion-plan

Via bloodtest-mode-expansion-plan; supports ctx-010793c65100; verbatim multiline rendering correction:

> can u also mka e a issue for the bloodtest mode and expand it to be like&#x20;

mix of counter striek and realistic idk&#x20;

its  a test for blood visuals and stuff and prob shoudl expand later into being like
first VR mode later so need vr suport

with that mode we prove vr support and also

the main focus is idk it whoudl jsut start off as a sandbox, but itd be&#x20;

u can shoot weapons and weapons have projecitles, not hitscan projectiels

projecitles do dmaage to the world phsically, and actors
liek their limbs take damage and on specific sections of the limbs u taek damage&#x20;

need to fix th limb health bug bc its like  if u damage someone enough then the bod jsut disdappers and the dnt actually die thats nto good, limbs need to be liek spearate helth/strength aaprt from the actor hp

but the  flow should be spawn in, a npc is there with me

much slower more deliberate mode, prob could do like
bc we donthave liek stairs or steup or  climbing uet we can also put that stuff in&#x20;

and hten sandbox its just &#x20;
aim at haed and hit super hard = head explodes in big blood mist, and the actor bod falls over ragdoll sttle bt more slower and realsitic

actor bod parts shoudl idk we shoudl reuse the same old mimita character, just make it so the maps are smaller nad mkae more sense, the movemnt needs to be a loittle slower and more deliberate&#x20;

sprint and stamina sstem, first person fov forced,&#x20;
etc
slower walkspeed an dsutff&#x20;
phsical gun aiming, not jiust locked to the corsshair wherver ur pointing&#x20;

aim the gun phsicallt at the actor and shoot adn then it shoudl do liek

itshoudl be ahrd to control as a result of  the mehcanicsm of how it behaves not just random aim punch o rrandom recoil no random recoil nothin grandom

shoot in torso = blood impact effect, sound, the person getting shot = effect of blood liek we ahve rigt now

if u die, it shoudl belike fallign over and screen turns black or fades ro black somehow but might conflic tiwht the instant respawn i want to keep that , we coudl do a fade between lives like&#x20;

death at tick 123, respawn at tick 124, but at tick 184f the fade from the previous life is done fading and then now u can full see the current game its not like the old liek flashbang overlay almost

this shoudl expand to multiple guns like, add rifles, single shot weapons, burst fire

we shoudl also expeirment with melee and grenades here , this is total influence from retrograd like that game needs to be big big credit that that is where these ideas are cominf gfrom&#x20;

so grenades shoudl explode bod parts like, a person lauing on a grenade should have like , explode into all blood mist, like, all libms head torso left/right arm adn left/right leg are gone bc the froce is so much, guts and like orgnas shoudl explode out and lots of blood red mark all over, bones, etc&#x20;

getting shot wihta shotgun is like &#x20;
Like point blank shoudl be a instant kil or near instant kill bc its like a lot of pellets at super close range&#x20;
and it shoudlb e super feel powerful to shoot, and also powerful to get hit with it&#x20;
and be more phsical like bieng able to walk up to someone and shoto them by putting hte gun to their face and then shooting liek that is what relates to the vr mode&#x20;

and in also we should be able to climb in this mode and maps should be designed aroudn climbing things lik ebeing able to ragdoll if u can ragdoll u have a advantage and can use the map to full adnvatnaget

no hitscan it must all e porjectile phsical nohitscan

we shoudl do the realsitic collsiion mode here matbe? idk, test using the arcade collision mode for starters bc it might not be that big of a deal but if it is a big deal then make it so wepaosn and limbs collide but we cna keep like idk, just dont wan to make the game run slow bc doing the super detaield collisosn wehn ewe dont need to&#x20;

can u add all this to the issue and expand it anf furhter the plan for implemetning full bloodtest mode

## Context

### 2026-11-21T23:48:53.833Z — bloodtest-mode-expansion-plan

reproduction: Original observed problems remain: actor falls through floor and revolver/shotgun feedback does not look explosive. New user direction asks for a staged full Bloodtest sandbox: spawn with an NPC, use physical projectile weapons against world and actors, test body-part damage/gore, slow deliberate movement, and expand to traversal, more weapons, melee, grenades, and eventually VR. No new map/spawn/tick/journal reproduction was supplied.
expected_behavior: Bloodtest begins as a compact single-player-feeling multiplayer test sandbox using the existing MiMITA character/rig, a nearby NPC and smaller purposeful maps. All guns use real physical projectiles and no hitscan; projectiles physically affect world and actors. Actor HP and per-part health remain distinct and consistent: destroyed limbs follow explicit lethal/nonlethal rules and cannot silently desynchronize rendering, hit validation, and actor death. Damage outcome scales deterministically with impact force and body hit region, with understandable torso blood and sound, large head/part destruction mist, bones/meat/skin layers, and slower physical ragdolls. Movement is slower/deliberate with sprint/stamina and functional steps/slopes/climbing; first-person FOV and physical weapon aiming support the intended close-range feel. Instant respawn remains while a separate short death fade clears between lives. Add rifle, single-shot, burst, shotgun, melee and grenade experiments in stages. Start with arcade collision and measure; add detailed weapon/limb collision only when scenario evidence shows it is needed and performance budget allows. Preserve architecture room for a future first VR mode and credit Retrograd as inspiration.
acceptance: Stage 0: write a Bloodtest mode contract that separates confirmed requirements from decisions, preserves the existing mode as baseline, credits Retrograd, defines 60 Hz/performance and event evidence, and records open tuning choices. Stage 1: reproduce floor-fall and limb-health/death mismatch in a pinned runtime; actor global HP, per-part health, destroyed part state, death lifecycle, and render/hit validation agree for nonlethal limb damage and lethal head/torso damage. Stage 2: playable vertical slice starts in a small Bloodtest map with the existing character rig and one nearby NPC, forced first person, measured slow preset, sprint/stamina, and current physical revolver/shotgun; no hitscan path. Stage 3: verify deterministic physical aiming, projectile/world contact, force-scaled damage by actor body region, shotgun close-range impact, torso blood/audio, head/part destruction, replicated skin/meat/bone and larger gore. Stage 4: verify slow ragdoll and instant respawn coexist with a death fade that completes on the selected ticks without flashbang-style overlay. Stage 5: add tested step/slope/climb routes and design compact maps for climbing/ragdoll traversal; benchmark arcade collision first and make detailed limb/weapon collision a measured opt-in only if required. Stage 6: add rifles/single-shot/burst, melee, and grenades incrementally; grenade acceptance includes force-based multi-part destruction and authored gore/organ presentation while preserving actor lifecycle correctness. Stage 7: document and validate VR-ready input/pose/weapon interfaces, then schedule actual VR support only after a device/runtime target is available. Each stage uses current dev-loop artifact, exact per-process journals, 60 Hz/performance evidence, and separate visual/human acceptance; an unvisited map or event is NOT_COVERED.
paths: ['src/network/server-damage.cpp', 'src/network/server-projectiles.cpp', 'src/network/multiplayer-projectiles.cpp', 'src/entities/actor-body.cpp', 'src/entities/player-render.cpp', 'src/ragdoll', 'config/gamemodes/bloodtest.json', 'config/movement/movement-bloodtest.json', 'config/weapons/bloodtest_revolver/config.json', 'config/weapons/bloodtest_shotgun/config.json', 'docs/specs/gamemodes/bloodtest.md', 'docs/specs/movement/movement.md']
evidence: {'new_context': 'ctx-010793c65100', 'existing_mode_spec': 'docs/specs/gamemodes/bloodtest.md', 'confirmed_related_regression': 'docs/regressions/gameplay/healthall-body-part-lethal-REG-2026-11-07.md documents a distinct body-part-health/global-health mismatch that can kill at nonzero displayed HP; it is evidence to retest, not proof it is the same disappearing-limb report.', 'runtime': 'No matching Bloodtest run/journal supplied for the new expanded direction.'}
risk: Body destruction, actor death, projectiles, physics collision, ragdoll, respawn, and visual effects share owners; parallel rules could cause body/death divergence or regress other modes. Keep behavior mode-scoped, fixed-tick, reuse current actor/damage/projectile owners, start with arcade collision, measure hot-loop and journal overhead, preserve unrelated modes, and do not claim VR support without a real VR target.
proof_expectation: Pinned live Bloodtest player/server/client run with matching build/DLL/run identities and exact journals for spawn/NPC, movement support, projectile contacts, damage by part/force, part layer/destruction, death/respawn fade timing, ragdoll, and grenade/multiweapon outcomes; measured 60 Hz and bounded performance; visual/human review separately. VR remains planned until device/runtime evidence exists.
context_id: ctx-010793c65100

## Agent suggestion

### 2026-11-21T23:50:09.038Z — bloodtest-mode-expansion-plan

Expanded Bloodtest plan from ctx-010793c65100 — planning only; no spec/code behavior changed in this intake.

Product direction
- Start with a compact, purposeful sandbox that mixes Counter-Strike style readable firefights with physical, deliberate combat. Reuse the existing MiMITA character/rig and shared actor, projectile, damage, movement, ragdoll, and mode owners.
- Credit Retrograd as the inspiration for articulated bodies, expressive physical reactions, and gore. The mode is an experiment, not a claim to reproduce Retrograd.
- Initial play loop: spawn into a small map with one nearby NPC; use physical-projectile weapons; see the actual contact, body-region damage, skin/meat/bone state, blood/audio, part destruction and ragdoll; repeat quickly.
- Bloodtest guns never use hitscan. Their projectiles physically hit the world and actors. Physical gun pose/muzzle direction and close-range weapon-to-face use should make aiming hard because the weapon is physically controlled; do not add random aim punch or random recoil.
- Keep actor HP and per-part health distinct, authoritative and consistent. A destroyed nonlethal limb can be absent while its actor remains alive; lethal head/torso outcomes must run the real death lifecycle. Rendering, collision/hit validation, replication and corpse state must agree. Retest the old body/global-health mismatch; the 2026-11-07 healthall regression is relevant evidence but is not yet proven identical to the new disappearing-limb report.
- Ordinary torso hit: clear impact sound and blood effect on the target. Hard head/part hit: large blood mist and explicit destruction; skin/meat/bone progression remains readable. A close shotgun should feel exceptionally powerful, with its exact near-instant-kill threshold measured and tuned.
- Keep the player first-person with a mode-owned FOV, slow deliberate walk, sprint ramp and stamina. Death can ragdoll/fade while immediate respawn stays: fade between lives using fixed ticks, e.g. death tick 123, respawn tick 124, full view by tick 184. This is a proposed initial timing example, not a locked value; fade must not look or behave like flashbang overlay.
- Plan new rifles, single-shot and burst guns, then melee and grenades. Grenades should be capable of force-based multi-part destruction with blood, bone and authored organ/guts effects; keep whole-actor death, corpse and remaining parts coherent.
- Maps should be smaller and make spatial sense for physical aiming, climbing and ragdoll navigation. Steps, slopes and climbing are part of the full direction, but the first route and movement limits need measured scenarios.
- Start collision experiments with the existing arcade collision mode. Measure the 60 Hz loop, candidate/contact counts and performance. Add expensive limb/weapon detailed collision only when a named scenario needs it and measured budget permits it.
- Preserve an architecture path toward a future first VR mode: sampled head/hand pose, physical weapon/muzzle, close contact and comfort/input boundaries. This is VR readiness, not present-day VR support; actual implementation waits for a named device/runtime and stable non-VR sandbox.

Implementation sequence
1. Contract and baseline: append a Bloodtest-specific mode contract with desired outcomes, current implemented/target status, exact event evidence, Retrograd credit, no-hitscan invariant, performance guardrails and explicit open decisions. Do not replace shared owners or silently rewrite broader movement/combat specs.
2. Actor life-state and floor regression: reproduce player/NPC floor support and the reported destroyed-body-without-death case in the exact Bloodtest map/run. Correlate global HP, per-part HP, destroyed flags, render/hit validation, lethal policy, replication and lifecycle. Fix first incorrect owner and prove lethal head/torso versus nonlethal limb results.
3. Playable vertical slice: small map, existing character rig, a nearby NPC, physical revolver/shotgun, first-person FOV, slow measured movement, sprint/stamina, reliable ground/slope contact. This is the first end-to-end build/run acceptance point.
4. Projectile/aim combat: verify physical gun pose and muzzle path, no hitscan, physical world/actor contacts, force- and body-region-scaled outcomes, point-blank shotgun feel, predictable/non-random handling, and server/client agreement.
5. Body visuals and life transition: verify skin->meat->bone, blood direction/size, torso audio, force-scaled head/part burst, slower ragdoll, instant respawn and separate fixed-tick death fade. Keep visual review distinct from event proof.
6. Traversal and map design: author representative stairs/steps, slopes and climbing routes; design compact maps around those routes and ragdoll opportunities. Begin with arcade collision, profile the fixed-tick path, and only enable finer limb/weapon collision if a real route fails without it.
7. Weapon/melee/explosive expansion: add rifle/single-shot/burst profiles, melee and grenade trials incrementally. Prove grenade force thresholds and multi-part/corpse outcomes plus blood/bones/organ presentation.
8. Future VR readiness: specify head/hand tracking and gun pose seams now; schedule real VR input/render/device work once a target headset/runtime and hardware acceptance setup exist.
9. Full-mode acceptance: current pinned dev-loop artifact; exact process PIDs/start identity, executable hash, EXE/DLL/generation/run/mode/map, server and per-client journals; real routes for spawn/NPC, slopes/climbing, projectile impacts by region/force, part transitions, death/fade/respawn, ragdoll, melee and grenade; 60 Hz/performance result. Screenshots/human feel/visual acceptance remain separate, and missing route/event coverage is NOT_COVERED.

Open decisions to resolve through measurement before final spec tuning: slow walk/sprint speeds and stamina curve; allowed step/climb height and traversal interaction; shot/impact energy thresholds for meat/bone/destruction and point-blank shotgun lethality; per-part strength versus health presentation; fade duration; exact map count/shape; body/organ visual assets; acceptable 60 Hz CPU/contact budget; when detailed limb/weapon collision is necessary. User gave direction but not numerical acceptance for these.

## Agent suggestion

### 2026-11-23T17:09:15.246Z — bloodtest-bodycam-spec

Observed from source/config on 2026-11-23 (static review only): Bloodtest currently selects physical/hybrid aim but its gamemode config still has no teams/score target, force_first_person=false, camera_fov=0, npc targeting disabled, npc profile beginner, avatar group disabled, player/friendly outlines enabled, and features.destructible_world=true. The Bloodtest movement file request is movement-slowdm.json, whose JSON name is heavy; the mode currently selects movement_preset=bloodtest. The revolver/shotgun definitions use physical projectiles and set explodeOnPlayerImpact/explodeOnWorldImpact to 1; their configs also have shooter_knockback=5 and damage-scaled victim knockback. Existing code has config/camconfig.json proximityFade and orientationMode=head_world; proximityFade fades nearby rendered surfaces and does not replace wall occlusion, and head_world camera pose currently follows the simulated head only while ragdoll/aim-body is active. config/postfx.json has chromaticAberration=0. The existing agent.screenshot command captures a visible client window BMP into logs; exact-tick screenshot_at_tick and reference comparison were found only in specs, not source. Proposed desired behavior (not implementation claims) is recorded in docs/specs/gamemodes/bloodtest.md and docs/specs/gamemodes/bodycam.md. No Bloodtest runtime/build test was run. Next proof should use the current dev-loop artifact on a pinned Bloodtest run and inspect matching client/server journals plus same-scene screenshots. Floor fall-through cause remains unknown; do not attribute it to destructible world or capsule contacts without a reproduction.

## Agent suggestion

### 2026-11-23T17:10:08.374Z — bloodtest-bodycam-spec

Additional requested scope recorded in docs/specs/gamemodes/bloodtest.md: mode-owned slowdm movement, fixed first-person competitive avatars, physical/hybrid aiming with live head/arm/camera smoothing, no player outlines/healthbars/damage numbers/hit markers, first-to-50 respawning TDM, mode-owned weapon overrides, projectile rifle, zero firearm knockback, bullet-hole/debris impacts, fall damage, and world destruction disabled. Camera findings: config/camconfig.json currently sets orientationMode=mouse and proximityFade start/end 2/3 m; proximityFade is surface fading, not wall collision. head_world follows the simulated head in the camera tick only when ragdoll/aim-body is active. config/postfx.json chromaticAberration is 0. Step-up/slope surface classification remains a shared movement/map-editor follow-on; ordinary steps/walkable slopes must not cause Bloodtest fall damage. The provider-neutral screenshot/reference comparison is tracked separately as issue-41f95565b65e. These are spec/intake findings, not implementation or runtime proof.

## Agent suggestion

### 2026-11-23T17:28:34.882Z — bloodtest-projectile-specs

2026-11-23 spec update: user specifies all Bloodtest firearms must use finite-travel physical projectiles with gravity drop and no hitscan/instant beams; Uzi 30/180 and ~0.5 s mag empty, Judgment 6 rounds, Glock 20/100, FOV starts at 70, results 5 s, fall damage starts only at >=5 m with provisional 6 m/30 m/s=10 damage and 20 m/60 m/s=90 (not instant kill), and per-weapon equip/unequip delay. Individual firearm and shared projectile/network specs are being added. Static source check: config/gamemodes/bloodtest.json currently camera_fov=0, force_first_person=false, intermission=15, results=8, no teams; masterlist declares camera_fov and force_first_person fields; postfx chromaticAberration=0; engine-tick-camera.cpp applies head_world orientation only when ragdoll or aim-body is active, so living bodycam requires extending that existing camera path. Sufficient spec exists for a first bodycam prototype (FOV70/FP/head attachment/aligned arms/lens/wall visibility), but screenshot iteration is still required for look acceptance. Open balance decisions: which subset of the planned AK-47/M4A1/FAMAS/Desert Eagle/Judgment/Glock 17/sawed-off/AA-12/Uzi/shotgun roster goes in the first playable slice; final damage/recoil/cadence/ballistics; fall curve and reload stages. Repo spec edits only; no runtime proof.

## Agent suggestion

### 2026-11-23T17:45:39.198Z — bloodtest-camera-tuning-plan

Bodycam live-tuning plan (2026-11-23): Store Bloodtest-only values in config/gamemodes/bloodtest.json. Keep existing top-level camera_fov=70 and force_first_person=true as the only FOV/perspective owners; add the remaining settings as a validated presentation.bodycam block in the canonical mode JSON and masterlist. Shared camconfig.json, aimbody.json, and postfx.json remain defaults; absent/null fields inherit. Extend the existing GamemodeRegistry parser/schema and GamemodeRuntimeResolver applyClient/clearClient to compose a temporary mode overlay through shared camera, aim-body, and postfx owners; never mutate base singleton values or write other modes. GamemodeRegistry::pollReload already watches gamemode JSON, but mode presentation must be reapplied when the active mode revision changes. Apply a valid candidate atomically at next render/fixed aim boundary; invalid/incomplete JSON retains last-known-good values. Only the camera/aim/lens tuning is live; match-critical rules remain frozen for the current round. Fields: living head_world enable/eye offset; camera position/rotation response; head, torso, arms, weapon follow; sway; lens chromatic aberration/vignette/distortion; separate wall-collision probe (proximityFade is not wall collision). Use time constants in seconds, validate bounds, avoid snap on reload. Workflow: launch Bloodtest; edit/save only bloodtest.json in an external editor; observe validated/applied revision and effective values in structured log; capture screenshot/run identity/hash; tune attachment, body response, optics, then network/replay one group at a time; restore prior JSON revision to roll back. Implementation phases: (1) masterlist schema and canonical JSON sync; (2) parser/runtime overlay and safe revision apply/clear; (3) living head_world + wall collision using existing camera/body owners; (4) bodycam debug event and invalid-config feedback; (5) pinned dev-loop proof and screenshots across enclosed/open maps, validate muzzle alignment and no double interpolation; (6) tune defaults to references. Existing GamemodeRuntimeResolver is the shared owner; no second camera config subsystem. Current source evidence: gamemode registry polls JSON, resolver already owns client mode overrides, global camera/aimbody/postfx configs have reload paths, but head_world only applies while ragdoll or aim-body is active. Therefore enough direction exists for first prototype, while visual acceptance needs live screenshot iterations.

## Assignment

### 2026-11-23T17:57:40.682Z — bloodtest-bodycam-runtime

Owner: bloodtest-bodycam-runtime

## Agent discussion and suggestions

### 2026-11-23T23:49:43.979Z — resource-profiler

Message: `msg-d1f1e0fa6369` (evidence)

Subject: Compiler failure in projectile source during shared build

The root dev-loop attempt `game-dll-20261123T234301570308Z-6c7a9c` failed at 23:46:14.757Z after193.092s. Exact compiler error: `src/network/multiplayer-projectiles.cpp:443:9: error: 'projectileTrailConfigFor' was not declared in this scope; did you mean 'projectileVisualConfig'?` It had spent157.347s in changed-source preflight; this was a compile-time symbol visibility/declaration failure, not a runtime regression. The same attempt's inputs include projectile-tracers config/header and other current work. Please check whether you own this path and coordinate a correction before another root build. No edit by me.

## User quote

### 2026-11-24T22:04:51.204Z — bloodtest-quality-triage

Via bloodtest-quality-triage; supports unspecified:

> corpses shooting corpses on ground should be able to have limbs explode after enough damage; limb damage/destruction should emit a hurt/break sound at the predicted hit position and blood spray; align the projectile with the actual gun tip so point-blank shots at a head cause the expected damage and blood; make hit decals appear immediately on client prediction and reconcile instead of keeping stale effects; support walkable_dot tuning in collisions JSON for walkable, nonslippery slopes; verify meat/bone GLB orientation and Blender export axis

## User quote

### 2026-11-24T22:06:40.437Z — bloodtest-quality-triage

Via bloodtest-quality-triage; supports unspecified:

> meat and bone layersa re rotated  but might be a export diffrent in belnder liek +y up instead of not y up&#x20;`nand i want to get dynamic light for both ak and judgment recvolver bc not there rightnow&#x20;`ncorpses shootgin corpses on ground shoul dbe &#x20;`nlimbs shoudl explode and stuff`n`nalso need sounds for the exploding of limbs and damaging limbs like`nclient predicts as  hit, play a hurt sound from that pos in the world that i hit them at&#x20;`n`ni want to focus on getting it super aaccurate the gun tip has to be like&#x20;`nit has to emerge frmo the actual gun&#x20;`nand put the gun tip on someones head and fire = it does damage and blood and stuff&#x20;`n`nwe also need collison slope walkable dot  field to work in collisons json to makae it so some slopes are waklable and nnot slippery&#x20;
