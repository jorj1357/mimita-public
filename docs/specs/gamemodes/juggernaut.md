2026 210 20 2313  need to make it so  juggernauts are not inisilbe and also  have limimted avatars

like both teams have speicifc avatars u can use  but others not allowed
or
like body part ruels etc

2026 10 20 2308 like u shoudl bea ble to do it all thru the  C:\mimita-v9\config\gamemodes\juggernaut.json this 

C:\mimita-v9\docs\specs\gamemodes\juggernaut.md  at the top  but i wnana focus on  jugernaut presnetation how come friendl actors have outlines, like  red or green its liek doenst even matter, red suposed to be enemt team, green suppsed ot  be friednly, but i want friendlt team to have 0  and enemt team to ahve faint red outline  

shoudtn need to go to  oher config files just  controleld thru this
right now plr outlines not wokring not being followedcorrect either 
## Gameplay and presentation addendum (2026-10-20)

This section turns the latest player-experience notes into design targets. It
does not claim that the current mode configuration or runtime already provides
them. Firm targets, proposals, and decisions still needed are kept separate.

### Design targets

- **Role-appropriate avatars:** Both roles use the editable `avatar.allowed`
  lists in their actor-preset JSON. The first list permits the named normal
  boy/girl body avatars and excludes monster and oversized variants. Players
  may choose any listed avatar; future list changes remain data-only.
- **Readable teams:** `presentation.player_outlines` is the mode-owned master
  switch. Missing or false means no actor outline, regardless of local visual
  preferences. When true, unknown-team actors remain unoutlined, friendly
  outlines follow `friendly_outlines` (false by default), and hostile outlines
  use the configured enemy color and alpha. Juggernaut uses faint red at 25%.
- **Combat feedback:** Hide damage numbers. Retain blood and bullet-hole
  impacts. The notes also ask to suppress other extra combat presentation, but
  do not identify every effect or HUD element that should be disabled.
- **Avatar scale and physical fit:** A large avatar must not sink into the
  floor or have a collision body that is visibly unrelated to its model. The
  same avatar must remain visible and able to deal and receive authoritative
  damage. Collision and placement must be consistent for player and NPC actors.
- **Heavy shotgun audio:** Firing the Juggernaut heavy shotgun should use its
  own authored firing sound rather than silently falling back to the ordinary
  shotgun sound.
- **Responsive weapon feedback:** A Juggernaut should see firing/reload and
  cooldown feedback immediately; it must not wait for an NPC to render before
  local feedback updates. Server-side attack timing, hit validation, damage,
  and round outcomes remain authoritative under the shared weapon system.

### Proposals

- Add role-level avatar compatibility groups or an allowlist using the
  existing avatar and role owners. Define how creator-made avatars are
  classified and validated; do not add a second avatar registry to this mode.
- Derive or validate collision dimensions and ground placement from the
  selected avatar's authored physical bounds using the shared collision/spawn
  owners. Keep collision server-authoritative and fixed-tick. Do not choose a
  scale formula or per-avatar exception here without checking those owners.
- Predict local weapon presentation immediately while retaining server
  validation. This addresses perceived delay without making cooldown or damage
  client-authoritative.

### `NEEDS_SPEC_DECISION`

- Should each role have a different subset of the current human avatar
  allowlist? The initial list is editable in role actor-preset JSON.
- How should a large avatar pass through narrow spaces: scale its capsule,
  constrain its usable avatar bounds, or use another shared collision policy?
  Define safe spawn, floor placement, and navigation behavior with that choice.
- Which combat feedback switches are off beyond damage numbers and friendly
  outlines? Should hit markers, hit sounds, hit flinch, muzzle flashes, and
  world impacts remain off while blood and bullet holes remain visible? The
  existing presentation contract has no separate bullet-hole switch.
- What specific heavy-shotgun sound asset should be used?

The report of an invisible Juggernaut and blood effects without health loss is
not resolved by this presentation section. Avatar visibility, attack-event
delivery, authoritative damage, and visible feedback are separate steps; use
the existing runtime journal and the confirmed damage-attribution finding
below when investigating them. A spec edit, successful build, or configuration
parse is not proof of visible or multiplayer acceptance.

2026 10 20 1800 
 C:\mimita-v9\config\gamemodes\juggernaut.json 
presentation isnt being followed

damamge nubmers dont show
dont show  dmg nubemrs dont show like
idk

also the plr outlines
frinedly outlines shoudl be disabeld, enemy shoudl be anebled
also need collisiosn to be scaled liek how big ur aacatar is
big avatar = bigger collision capsule
smaller = smaller colisin capsule
how to fit into small spots? idk 

2026 10 20 1755

todo make it so some av
make a allowed avatars list
or
allowed bod parts avatars
so later when pl make their own avatars
idk
auto convert it to be like 
one specific avatar or 
IDKKKKK IDK IDK IDK 
idk
preserve plauer expression while also making  some avatars not happening bc too big ro small or  etc 

2026 10 20 1731 jorj todo: the issue is 
like
friendly people need to not have outlines
right now, friendlies  can ahve like green outline or red outline,not good, onl the enemy should have red outline. actuallt better is to just have it be like ez to see whos on ur team or not

also a bug is  the heavy shotgun doenst have  its own sound
nede to have the heav shotgun sound when juggernaut shoots it 

instead of just  like idk normal shotgun thts not good 

also 2026 10 20 1735 the juggernaut i think oen of themis just invisible? cant see them and the wont do damaeg to me
we nede to make it so  bigger avatar has actual collisions and doesnt just  sink into the ground and then make it so juggernaut team is forced to have an avatar 

also rocket launcher 
the inviisble juggernaut bug also the are n to doing dmg to me, i get blood effects but i dont take damage no view punch etc
avatar edits brkoe it?
idk

ROCKET LAUCNHER COOLDOWN NOT GOOD 
dont do 
cooldown issues bc the server is waitng and cant figuer out the cooldown
dont even 
idk
make the cooldown on the server like
idk
make it client sided i tihnk tahts good 
bc its just ur reload time or cooldown dont make it so u have ot wait for npcs to render o rosmething thats boring n  and sucks 
juggernaut hsooting at me and is invisible and doenst do damage ot me at all and so i dotn die and it snot fair not good win to a juggernaut round 
what do we   do abt avatars being bigger/smaller for npcs and mkae i tso  the collisions work and limbs dont go into the floor? 

2026 10 20 1749 
can u look at specs like  C:\mimita-v9\docs\specs\gamemodes\juggernaut.md  here or other related

is there enoguh spec  to figure out how come juggernauts  are invisible someitmes? 
likeim getting shot b juggernauts in the mode right now 

 map=zombietower4
Last     : AI launched last successful build 2680
Journal  : C:\mimita-v9\logs\2026-10-20\20261020_202850\events.jsonl
View     : live
Controls : [1] launch latest  [2] launch mode  [V] client visible/headless  [A

need to put stuff in the live journal to see like how come juggernaut is invisible 
do we ahve enough info tho or is thislike  understandable 

2026 10 11 1737  we  can do 
like
super aewsome 
make big big big avatars
fo the juggernaut
and mkae them super big and fat and slow
but HUUUUg
huuuuge
like
madness combat too
u can maekm it so   u can shoot a massssssssssssssssssive  revovler
s ke  i thougth wS brkoken
ok



# Juggernaut mode plan

Date file created: 2026-10-06

Status: Phase 1 configuration implemented; Juggernaut health, arcade presentation,
team-colored outlines, endless round cycling, and the Fighter weapon-choice UI
are implemented. Connected-client gameplay acceptance is still pending.

## Implemented first slice

The first playable configuration now exists in the active registries:

- `juggernaut` is available through `config/onlinemodes.json`.
- The roster is 16 Fighters versus 2 Juggernauts.
- Fighters use fast movement, 100 health, and choose one weapon per life from
  the visible four-choice popup with keys 1-4.
- Juggernauts use heavy movement, 5,000 health, a big shotgun/hitscan-rifle
  role loadout, and a limited-information NPC behavior profile.
- The mode uses the shared round lifecycle with one life and team elimination.
- The presentation-only juggernaut_arcade actor preset enables damage numbers,
  hitmarkers, hit effects, hit sounds, blood, muzzle flashes, and world impacts.
- Juggernaut presentation uses green friendly outlines and red enemy outlines.
- The round lifecycle is endless: intermission, 3-2-1-GO, numbered round,
  round-over result, and the next countdown repeat without a match cap.

This configuration has not yet been accepted through a connected human client.

## End goal

Create a JSON-controlled, round-based elimination mode in which a small team
of slow, powerful Juggernauts fights a larger team of fast, fragile Fighters.
The mode should reuse the shared actor lifecycle, movement, weapon, damage,
physics, NPC, networking, and spectator systems. Adding this mode must not
create a second player implementation, a second weapon implementation, or a
Juggernaut-only NPC system.

The intended first balance target is:

- Fighters: the larger team, `100` health, fast movement, limited weapon choice.
- Juggernauts: two or four players by configuration, `5,000` health initially,
  slow movement, narrow field of view, and unusually powerful weapons.
- Both teams: no respawns during a round.
- Round victory: eliminate every living member of the opposing team.

All important values are tuning data. The numbers above are the starting
proposal, not hardcoded engine constants.

## Player experience

### Fighters

Fighters should feel close to the current sandbox player, except that the mode
limits their life to one weapon and gives them a faster movement profile.

- Health: `100` initially.
- Movement: fast Fighter movement preset.
- Camera/FOV: normal Fighter values.
- Respawn: disabled during the active round.
- Weapon count: exactly one weapon per life.
- Weapon choices: revolver, shotgun, spy knife, or rocket launcher.
- Weapon selection: an explicit pre-round choice, never an unannounced random
  assignment.

The server validates and locks the choice before the Fighter spawns. The
player cannot switch to another weapon during that life. After death, the
player spectates until the next round and receives a new choice opportunity.

The selection UI should show the four legal choices and enough information to
make the tradeoff understandable. The final authority remains the server:

```text
intermission/team selection
    -> Fighter chooses one weapon
    -> server validates team, phase, and weapon choice
    -> Fighter spawns with one locked weapon
    -> death creates a spectator state
    -> next round permits a new choice
```

### Juggernauts

Juggernauts should be rare, intimidating, and dangerous rather than merely
being Fighters with more health.

- Team size: configurable; initial target is two or four Juggernauts.
- Health: `5,000` initially, JSON-controlled.
- Movement: slow Juggernaut movement preset.
- Turning/handling: may be slower while using heavy weapons.
- Field of view: narrow, JSON-controlled.
- Respawn: disabled during the active round.
- Weapons: a small Juggernaut weapon set with powerful variants.

Their weakness is limited mobility and awareness. Their strength is their
health, damage, knockback, range, and weapon pressure. The mode should not
silently compensate for the narrow FOV with perfect radar, omniscient AI, or
automatic target knowledge.

## Mode lifecycle

The mode uses the shared round lifecycle:

```text
INTERMISSION
    -> team assignment and roster lock
    -> Fighter weapon selection
    -> COUNTDOWN
    -> GO / ACTIVE ROUND
    -> elimination or configured timeout
    -> RESULTS
    -> next round or match results
```

Round rules:

- No player or NPC respawns during an active round.
- A dead participant becomes a spectator for the remainder of the round.
- A team is eliminated when it has no living participants.
- Fighters win by eliminating every Juggernaut.
- Juggernauts win by eliminating every Fighter.
- Match length and rounds-to-win are configurable.
- Team capacity and the number of Juggernauts are configurable.
- The mode must define what happens when a participant disconnects, leaves, or
  never completes weapon selection; it must not leave an invisible living
  participant blocking round completion.

The first slice should support uneven rosters, such as six Fighters versus two
Juggernauts or ten Fighters versus four Juggernauts. Health and damage should
remain explicit tuning values first; later balance work may scale Juggernaut
health or weapon power by roster size if playtesting requires it.

## Reusable architecture

The mode should compose existing owners instead of introducing specialized
actor classes:

```text
Juggernaut gamemode
  -> team membership and round/elimination rules
  -> Fighter role + Fighter actor preset + weapon-choice policy
  -> Juggernaut role + Juggernaut actor preset + weapon set
  -> shared actor lifecycle
  -> human input or NPC ActorIntent
  -> shared movement, weapon, damage, physics, and networking
```

### Gamemode owns

- Team definitions and capacity.
- Round phases, timers, countdown, results, and match victory.
- No-respawn/elimination rules.
- Team assignment and roster lock.
- Fighter weapon-selection window and lock policy.
- Victory conditions and timeout behavior.
- Mode-specific HUD metadata and scoreboard presentation.

### Roles own

- Team identity.
- Actor-preset reference.
- Behavior-preset reference.
- Spawn group.
- Role-level capability restrictions.
- Initial loadout or weapon-selection policy reference.

### Actor presets own

- Health and life baseline.
- Movement preset.
- Camera/FOV policy.
- Avatar and role-specific presentation defaults.
- Weapon restrictions or in-memory weapon overlays.

The Juggernaut mode must not duplicate health, movement, or role-specific FOV
ownership in several JSON files. The mode selects the role; the role resolves
the actor preset; the actor preset supplies that role's actor configuration.
Match-wide gameplay feedback switches belong to the gamemode's `presentation`
object as specified in
[`../graphics/graphics-presentation.md`](../graphics/graphics-presentation.md).
An explicit mode value overrides the actor preset's default for that shared
feedback option; an omitted value inherits the role default.

### Behavior presets own

Behavior presets apply to NPCs using the same roles as human players. They may
control:

- Perception range and FOV.
- Preferred engagement distance.
- Aggression and target priority.
- Whether the actor pursues, holds, retreats, or searches.
- Weapon preference within the role's legal weapon set.

Behavior presets must not grant weapons or override the team rules. A
Juggernaut behavior profile can prefer close-range shotgun pressure, but it
must still consume the Juggernaut role's legal weapon set.

## Weapon plan

Juggernaut weapons are configurations of the shared weapon definition and
execution systems. They are not separate `JuggernautWeapon` code paths.

### Juggernaut big shotgun

Start as a configured shotgun variant:

- More pellets or larger pellet traces.
- Higher damage.
- Stronger knockback.
- Large tracer and muzzle presentation.
- Deliberate range/spread tradeoff.
- Fire delay tuned separately from the Fighter shotgun.

The server continues to own pellet traces, damage, and knockback. Client
prediction may show immediate effects, but confirmation and health remain
authoritative.

### Juggernaut large machine gun

Start as a configured automatic hitscan variant using the shared rifle/hitscan
path:

- High fire rate.
- Large tracer thickness and heavy firing presentation.
- Moderate per-hit damage balanced against sustained fire.
- Victim knockback.
- Weapon-local wall-penetration budget.
- Potentially reduced turn/movement handling while firing.

Wall penetration must be a property of this weapon variant, not a global rule.
The intended trace is:

```text
trace toward target
    -> hit a penetrable wall
    -> measure or classify wall thickness/material
    -> spend penetration budget
    -> continue only if budget remains
    -> reduce damage after penetration
```

The first implementation may use a bounded penetration budget and simple
material classes. It must not permit unlimited wall shots or make every weapon
penetrate by accident.

### Juggernaut heavy revolver

This is optional for the first playable slice, but should fit the same system:

- Slow fire rate.
- Small magazine.
- Very high damage.
- Large projectile/tracer presentation.
- Strong knockback.
- Deliberate recovery time.

It should be a weapon-definition variant, not a separate combat subsystem.

### Juggernaut chainsaw

The spy knife is a valid reuse point for melee validation and contact
authority, but a chainsaw should be represented as a continuous active melee
window rather than one enormous knife hit.

Conceptual behavior:

```text
attack begins
    -> chainsaw active window
    -> bounded melee overlap checks at fixed ticks
    -> damage at a controlled interval
    -> continuous push/knockback while contact persists
    -> attack ends
```

Required safeguards:

- Damage interval prevents damage every render frame.
- Server validates active window, range, target, and life generation.
- Contact work has bounded targets and reusable scratch storage.
- Audio/effects are event-driven and do not create duplicate confirmation FX.

The chainsaw should be added after the core round and firearm slice is stable.

## Automatic-fire batching and performance

Fast fire must remain compatible with the fixed 60 Hz gameplay rule. Batching
means grouping legal work, not silently dropping valid shots.

For each fixed simulation tick:

1. Validate fire intents, life generation, team state, ammo, and cooldown.
2. Accept only the number of shots legally permitted by the weapon's timing.
3. Process related traces and damage in a bounded batch.
4. Accumulate knockback/damage results deterministically.
5. Emit compact confirmed events for replication and presentation.

The implementation must include:

- Per-weapon cooldown validation.
- Per-connection attack-request rate limiting.
- A bounded number of accepted shots per actor per tick.
- Reused scratch buffers in hot paths.
- Bounded wall-penetration steps.
- No unbounded per-shot allocation in fixed-tick combat.
- Deterministic ordering when multiple attacks affect the same target.

The client may predict automatic fire and tracer presentation, but the server
must own final hit, health, death, knockback, score, and round-end decisions.

## Proposed configuration shape

The exact schema should follow the active gamemode/role/actor-preset registries,
but the ownership should be equivalent to:

```json
{
  "id": "juggernaut",
  "name": "Juggernaut",
  "teams": [
    {
      "id": "fighters",
      "role": "juggernaut_fighter",
      "capacity": 10,
      "weapon_selection": {
        "mode": "one_per_life",
        "choices": ["revolver", "shotgun", "spy_knife", "rocket_launcher"]
      }
    },
    {
      "id": "juggernauts",
      "role": "juggernaut",
      "capacity": 4
    }
  ],
  "rounds": {
    "respawns": false,
    "victory": "team_elimination",
    "juggernaut_health": 5000
  }
}
```

This is illustrative only. Do not add a second schema if the active registry
already has equivalent fields. Existing role, actor-preset, behavior-profile,
and weapon-set owners should be extended only where necessary.

## Implementation phases

### Phase 1: data model and ownership

- Confirm the active gamemode, role, actor-preset, behavior-profile, and
  weapon-set schemas.
- Define Fighter and Juggernaut roles.
- Define actor presets for fast/fragile and slow/heavy behavior.
- Define the mode's team capacities, round rules, and weapon-selection policy.
- Add pure validation for unknown references, invalid team sizes, and illegal
  weapon choices.

### Phase 2: round lifecycle

- Reuse the shared intermission/countdown/active/results lifecycle.
- Lock teams and Fighter weapon choices at the correct phase boundary.
- Disable respawn during the active round.
- Transition dead participants to spectators.
- End rounds on team elimination and replicate the result state.

### Phase 3: actor-preset application

- Apply health, movement, FOV, camera, loadout, and behavior through existing
  spawn/reset owners.
- Verify initial spawn and every new round reset use the same role/preset path.
- Ensure NPCs and human players receive equivalent gameplay configuration.

### Phase 4: weapon restrictions and selection

- Present the four Fighter choices.
- Validate and replicate the selected weapon.
- Lock the selection for the life.
- Reject equip, fire, or swap requests for weapons outside the role policy.

### Phase 5: Juggernaut firearms

- Add configured big shotgun and LMG variants.
- Add weapon-local knockback and tracer presentation.
- Add bounded, weapon-local wall penetration for the LMG.
- Add automatic-fire batching and focused performance diagnostics.

### Phase 6: AI behavior and balancing

- Add Juggernaut and Fighter behavior profiles.
- Enforce perception FOV and range for NPC target acquisition.
- Confirm no omniscient targeting bypasses the Juggernaut's intended weakness.
- Tune health, damage, fire rate, movement, FOV, and team-size balance through
  live JSON values.

#### Fighter unit behavior (general, not Juggernaut-specific)

The intended Fighter experience is a **coordinated swarm**: Fighters leave spawn
together, move as one unit toward the nearest Juggernaut, spread out instead of
stacking, and keep shooting while they close. This is composed entirely from the
general owners defined in:

- `docs/architecture/player-npc-systems/npc-group-behavior.md` (TeamFocus +
  SquadCoordinator);
- `docs/architecture/player-npc-systems/npc-local-avoidance.md` (spacing);
- the Recast/Detour navigation backend (routing).

The mode owns no Fighter AI. The Fighter role references a behavior profile whose
group fields (see the group-behavior contract section 6) enable focus toward the
hostile team and swarm slots. The Juggernaut role uses a profile without focus
(`focus_enabled: false`) unless a future revision says otherwise.

Required Fighter properties:

- leave spawn toward the focus instead of patrolling or wall-grinding;
- converge as a unit on the focus with bounded cohesion;
- occupy distributed firing slots rather than one line or a stack;
- cap how many members commit to a single focus actor (`max_attackers_per_target`);
- re-rally and re-slot after deaths or when the focus moves;
- never aim through walls; a focus actor is a travel target, not an aim target.

#### First acceptance map

The first acceptance map for this behavior is `dust2cyberiav4`. The mode's map
list should include it so the round/scenario pipeline can exercise the swarm on
the same geometry used by the navigation contract.

#### Juggernaut behavior

Juggernauts advance or hold with limited awareness. Their narrow FOV and range
limits are respected by the perception owner; they must not receive focus-style
shared omniscience that bypasses their intended weakness.

#### Acceptance metrics (initial, dust2cyberiav4, fixed seed)

- 100% of Fighter routes have a result or an explicit hold/fallback reason;
- Fighters make positive net progress toward the focus until engagement;
- zero unrecovered wall-intersection or collision-loop failures;
- Fighters arrive distributed (measured slot spread) rather than single-file;
- at least one coordinated push reaches interaction range with a Juggernaut
  before the round's first engagement window ends;
- no aim or damage event originates without a perception-valid visible target;
- human review confirms the Fighters look like a unit rather than loose
  individuals.

### Phase 7: optional heavy weapons

- Add the heavy revolver.
- Add the chainsaw using continuous melee-window logic.
- Validate damage interval, knockback, replication, and presentation.

## Ownership targets

Likely owners to inspect before implementation:

- Gamemode rules/lifecycle: `src/gamemode/` and
  `src/network/server-gamemode.*`.
- Roles and actor-preset composition: `src/gamemode/match-roles.*` and the
  existing role/preset registries.
- Shared weapon definitions/execution: `src/combat/` and the active weapon
  configuration registries.
- Authoritative attacks/damage/knockback: existing server attack and damage
  owners; do not create a Juggernaut-only network path.
- NPC behavior/perception: existing behavior-profile, perception, intent, and
  navigation owners.
- Spectating and life generations: existing shared actor lifecycle and
  spectator replication owners.
- HUD and selection UI: generalized gamemode GUI/selection owners.
- Diagnostics: the canonical structured JSONL event logger.

These paths are ownership targets for investigation, not permission to assume
that every historical implementation is active. The active checkout and
runtime-loaded configuration must be verified before edits.

## Acceptance criteria

### Data and source

- The mode loads from the active gamemode registry.
- Fighter and Juggernaut role references resolve to valid actor and behavior
  presets.
- The four Fighter weapons are legal choices and no other Fighter weapon is
  equipable.
- Juggernaut health, movement, FOV, team capacity, and weapon values are
  configuration-owned.
- No duplicate mode-specific actor or weapon subsystem is introduced.

### Round behavior

- Teams lock before the countdown.
- Fighters select one weapon before spawning.
- A Fighter cannot change weapon during the life.
- Death removes a participant from the active roster and creates spectator
  state without respawning them.
- The round ends when one team has no living participants.
- Next-round reset applies the correct role, preset, health, weapon policy, and
  life generation to every participant.

### Combat and performance

- Juggernaut weapons use shared validation, tracing, damage, knockback, and
  replication owners.
- LMG wall penetration is bounded, weapon-local, and reduces damage according
  to the configured rule.
- Automatic fire remains fixed-tick and rate-limited under many simultaneous
  shooters.
- No render-frame collision or damage path is introduced.
- Server health, death, knockback, score, and round outcome remain authoritative.

### Human acceptance still required

- A real round visibly communicates team, health, weapon selection, countdown,
  death, spectating, and victory.
- Fighters feel fast and vulnerable.
- Juggernauts feel powerful but meaningfully slow and unaware outside their FOV.
- Two- and four-Juggernaut rosters are both playable.
- Heavy weapon audio, tracer size, knockback, and penetration feel correct.
- Multiplayer behavior remains stable with simultaneous automatic fire.

## Per-mode aim, feedback, and NPC combat (2026-10-29)

Juggernaut owns its aim and victim-feedback policy in
`config/gamemodes/juggernaut.json` through the nullable masterlist fields
described in `docs/specs/gamemodes/gamemodes.md`. The intended values are:

- `aim.mode: "physical"` — bullets follow the weapon barrel.
- `aim.body_mode: "hybrid"`, `aim.body_enabled: true` — the body is physically
  simulated while limbs follow the procedural animation pose.
- `presentation.hit_flinch: true` — the local victim camera punches when hit.
- `presentation.blood_client_feedback: true` — camera-locked blood appears when
  the local player is hit, independently of world blood.

The mode must not require editing `config/gameplay.json`,
`config/aimbody.json`, or `config/impact_decals.json` for these values, and
other modes that leave the fields `null` keep the shared base behavior.

### Hostile Fighter NPCs

The human Juggernaut fights the Fighter team, so Fighter NPCs are the primary
combat opponents. They must:

- use every attack-capable weapon in the `juggernaut_fighter` role loadout, not
  only the revolver, through an authored behavior-profile weapon cycle;
- dash when the shared movement and dash rules allow it instead of staying on
  the roster's hardcoded low difficulty;
- keep pursuing a target after line of sight is lost instead of forgetting it;
- approach from different sides relative to the target's look direction when
  the profile opts into sector approach, so the squad does not collapse into
  one point.

These are shared NPC behavior-profile and TeamBrain rules, not a
Juggernaut-only NPC implementation. The exact profile fields and acceptance
criteria are in `docs/specs/npc-behavior/npc-behavior.md` and
`docs/architecture/player-npc-systems/npc-movement.md`.

## Related documents

- `docs/specs/gamemodes/gamemodes.md`
- `docs/specs/weapons/weapons.md`
- `docs/specs/networking/networking.md`
- `docs/specs/performance/performance.md`
- `docs/skills/spec-behavior-review-v1.md`
- `docs/features/gamemodes/counterstrike.md`

## Confirmed damage-attribution bug — 2026-10-20

Juggernauts must damage Fighters through the shared authoritative actor-damage
path. The server must preserve the attacking NPC's actor ID, actor kind, and
team through friendly-fire checks. Friendly fire stays disabled: reject a hit
only when both known teams match; an enemy Juggernaut must damage a Fighter.

In journal `logs/2026-10-20/20261020_221119/events.jsonl`, the server rejected
a Juggernaut hitscan hit on player 1 as same-team damage. The record reports
`attacker_id=0`, `attacker_team=0`, and `target_team=0`, even though the NPC
damage path separately knows the Juggernaut ID and team. This explains the
observed “shot but no health loss” symptom for that hit.

This does not yet explain every invisible avatar or missing shot effect. The
same run logged actor 130 binding to a fallback while its avatar load was
pending, but that alone does not prove the rendered actor was invisible. Shot
send/receive/render evidence is also not sufficiently correlated to locate a
missing visual. Keep those as separate open investigations.

Acceptance for this fix: an enemy Juggernaut hit reduces Fighter health and
replicates confirmed damage with the NPC identity; a same-team NPC hit remains
blocked. The live journal must show attacker ID, actor kind, attacker team,
victim team, and applied/rejected result.

## Juggernaut Horde variant

`jugger_horde` is a separate selectable mode. It fills the normal
Juggernaut Fighter team to its current 64-actor capacity and fields four
Juggernauts. Humans join Fighters; NPCs fill open Fighter slots and all four
Juggernaut slots. A Fighter respawns on the next fixed update after death;
a dead Juggernaut stays out until the next match. Each Juggernaut kill of a
Fighter restores 100% of the Juggernaut's role maximum HP, capped at maximum,
for both human and NPC Juggernauts. Fighter kills grant no kill healing.

Fighters win when every Juggernaut is eliminated. Juggernauts win if at least
one survives until the authoritative active-match deadline at 300 seconds,
which is 18,000 ticks at 60 Hz. Elimination is evaluated before timeout on
the same tick, so a final Juggernaut death on the deadline wins for Fighters.
The mode reuses the shared team roster, actor lifecycle, kill credit, health,
and results owners. Team respawn delay, team kill-heal percentage, and a
configured timeout winner are general mode data rather than Horde-only logic.
The match HUD shows the authoritative time remaining at the top, starting at
`5:00` and counting down to `0:00`. Every match uses the shared visible
`3 → 2 → 1 → GO!` countdown. Results name the winning team, then the next
intermission and countdown start automatically. This cycle repeats indefinitely.
