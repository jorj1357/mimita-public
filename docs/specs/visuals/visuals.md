2026-09-07T18:40:16Z jorj - todo explain this  more like
plr visuals plan  and how to maek them easier to see
and also what  fits the vision for mimita  for me and what doesnt etc like explain the art style and stretched picutres and silent hill 3  ps2 type textures and sounds etc 

rough:

end goal  : gameplay visuals are much more editable, and we get rid of teh issue where enemies/other players are hard to see because of the quick movement and not super good contrast between players and the environment

desired behavior
i go into the game
i run around and fight people and die and kill others
but people seem to be too  thin too hard to see
so , i ahve a json here C:\mimita-priv-v8\config\playervisuals.json

and i can edit things like 

outline: true/false
outlinethickness: 0, 0.1, 1.0 10.0 100.0 etc 
outlinealpha: 1.0 = full opaque, 0.0 = full invisible
outlinecolor: 255,255,255, or any other color i want to have 
outlinevisiblethruwalls: true/false - so if true, outline visible all the time, if false, only if i can actually see their body means i can see them 
outlinedisappearondeath: true/false, if true then no more outline for a player if theyre dead

and other things, but those are  the things
hot reloadable according to hot reload spec although not sure if that is defined anywhere , C:\mimita-priv-v8\docs\specs\hotreload\hotreload.md its defined here just its not detailed at all 


# MiMITA Player Visuals and Readability Specification

**Created:** 2026-09-07T18:40:16Z  
**Primary config:** `C:\mimita-priv-v8\config\playervisuals.json`  
**Related competitive config:** `C:\mimita-priv-v8\config\competitive.json`  
**Related hot-reload spec:** `C:\mimita-priv-v8\docs\specs\hotreload\hotreload.md`

---

# 1. Purpose and End Goal

The player visuals system exists to make players easier to perceive during fast MiMITA gameplay without requiring the game to abandon its intended visual identity.

The current problem is that players can appear:

- too thin;
- too visually similar to the environment;
- difficult to track during fast movement;
- difficult to reacquire after dashes, jumps, explosions, or camera movement;
- difficult to distinguish from teammates, enemies, NPCs, or environmental geometry.

The end goal is not merely to add an outline.

The end goal is:

> **Gameplay-relevant entity visuals are represented through a generalized, hot-reloadable, layered presentation system that can be edited independently of gameplay simulation.**

A player should be able to enter the game, fight normally, notice that another player is difficult to see, edit `playervisuals.json`, save the file, and immediately experiment with a different representation without restarting the game.

Possible visual representations eventually include:

- outline;
- capsule;
- box;
- sphere;
- silhouette;
- body tint;
- glow;
- rim lighting;
- shadow;
- nameplate or marker;
- movement trail;
- future visual readability techniques.

These must be implemented as independent visual layers rather than unrelated one-off features.

Multiple layers may be active simultaneously.

For example:

```text
enemy player
    ├── player mesh
    ├── white outline
    ├── transparent capsule
    └── movement trail
```

V1 does **not** need to implement every layer.

V1 should establish the generalized system and implement the outline layer correctly first.

---

# 2. MiMITA Visual Direction

The readability system must fit MiMITA's art direction rather than gradually turning the game into a conventional modern competitive shooter.

MiMITA intentionally favors visual characteristics inspired by PS2-era games and especially the atmosphere and texture character of games such as Silent Hill 3.

Desired characteristics include:

- low-resolution textures;
- visibly stretched textures;
- texture distortion;
- simple or inexpensive geometry where appropriate;
- strong silhouettes;
- imperfect surfaces;
- harsh or unusual texture presentation when visually interesting;
- deliberately non-photorealistic rendering;
- atmospheric environments;
- distinctive, raw, compressed, or otherwise characterful sound;
- visual imperfections that contribute personality.

Low texture quality or texture distortion must **not automatically be treated as a defect**.

For MiMITA, these can be deliberate artistic choices.

The desired direction is not:

```text
make everything maximally clean
make every material physically correct
make everything glossy
make everything AAA-realistic
remove every visual imperfection
```

Instead:

```text
keep intentional visual character
+
make important gameplay information extremely readable
```

Player readability therefore sits above the environment stylistically.

The world may intentionally contain noisy, stretched, low-resolution, dark, strange, or distorted visuals while player representations remain easy to identify.

The visual system should solve readability through configurable gameplay presentation rather than by flattening or sanitizing the entire environment.

---

# 3. General Layered Player Visual Architecture

Player visuals must be designed as layers.

Conceptually:

```text
PlayerVisualPresentation
    ├── base player mesh
    ├── outline layer
    ├── capsule layer
    ├── box layer
    ├── silhouette layer
    ├── glow layer
    ├── tint layer
    ├── rim-light layer
    ├── trail layer
    └── future layers
```

Each layer should eventually have:

```text
enabled
render order / layer priority
visibility rules
color
alpha
geometry settings where relevant
depth behavior where relevant
death behavior
team/enemy/self applicability
```

Layers must be independently toggleable.

Examples:

```text
outline = enabled
capsule = enabled
trail = disabled
```

or:

```text
outline = disabled
capsule = enabled
trail = enabled
```

or:

```text
everything = disabled
```

Multiple layers must be allowed to render simultaneously.

There must be an explicit ordering system so that combinations do not depend on accidental renderer ordering.

Conceptually:

```json
{
  "renderOrder": [
    "capsule",
    "silhouette",
    "outline",
    "nameplate"
  ]
}
```

The exact JSON representation may differ if a better structure fits the existing engine architecture.

The important requirement is that ordering is explicit and deterministic.

V1 only needs to fully implement the outline layer.

However, the implementation must avoid making the outline system so special-case that capsules, boxes, silhouettes, or trails later require replacing the entire architecture.

The generalized primitive should be approximately:

> Apply one or more configurable visual presentation layers to a gameplay entity according to local settings and authoritative game-mode restrictions.

---

# 4. Entity Categories and Local Preferences

Player visual preferences are primarily client-side.

Each client chooses how entities are presented on that client's screen.

At minimum, configuration must distinguish:

```text
enemy
teammate
self
```

The architecture should also allow later support for:

```text
NPC
friendly NPC
enemy NPC
spectator
dead player
ragdoll
other gameplay entity types
```

Enemy and teammate settings must be independent.

Example desired behavior:

```text
Enemies:
    outline enabled
    white outline
    thickness 3.0
    alpha 1.0

Teammates:
    outline disabled
```

Another user might instead choose:

```text
Enemies:
    red outline

Teammates:
    green outline
```

Or:

```text
Enemies:
    white outline
    translucent capsule

Teammates:
    blue outline
    no capsule
```

The game must ship with default values so players do not need to construct a configuration manually.

Local configuration defines preference.

It does **not** necessarily define final authority.

Game-mode or server rules may restrict which visual settings are legal, as defined later in this specification.

---

# 5. V1 Outline System

The first implemented visual layer is the player outline.

At minimum, the outline configuration must support:

```text
enabled
thickness
alpha
color
visibleThroughWalls
disappearOnDeath
renderOrder
```

Example conceptual configuration:

```json
{
  "enemy": {
    "outline": {
      "enabled": true,
      "thickness": 3.0,
      "alpha": 1.0,
      "color": [255, 255, 255],
      "visibleThroughWalls": false,
      "disappearOnDeath": true,
      "renderOrder": 100
    }
  },

  "teammate": {
    "outline": {
      "enabled": false,
      "thickness": 2.0,
      "alpha": 1.0,
      "color": [100, 200, 255],
      "visibleThroughWalls": false,
      "disappearOnDeath": true,
      "renderOrder": 100
    }
  }
}
```

The exact schema may be changed if repository architecture indicates a cleaner generalized format.

Behavior definitions:

### `enabled`

```text
true  = render the outline
false = do not render the outline
```

### `thickness`

Controls outline thickness.

Examples:

```text
0.0
0.1
1.0
10.0
100.0
5000.0
```

V1 should intentionally permit extreme experimental values.

Do not arbitrarily constrain values merely because they look unreasonable.

Later revisions may establish safe or competitive ranges once useful values are known.

### `alpha`

```text
1.0 = fully opaque
0.0 = fully invisible
```

Values outside the conventional `0.0–1.0` range may either be passed through or handled according to the renderer's existing numeric rules during early experimentation.

Do not silently introduce restrictive policy unless necessary for renderer stability.

### `color`

Must support arbitrary color selection.

Example:

```json
"color": [255, 255, 255]
```

represents white.

### `visibleThroughWalls`

```text
true:
outline may remain visible even when geometry occludes the player's body

false:
outline only appears where normal visibility rules permit seeing that player
```

V1 only needs this boolean behavior.

More advanced occlusion behavior may be added later.

### `disappearOnDeath`

```text
true:
outline stops rendering once the player is considered dead

false:
outline may continue rendering on the dead player/ragdoll according to other rules
```

Death handling must use the game's actual authoritative/local replicated death state rather than guessing based on animation.

---

# 6. Future Geometry Layers: Capsule, Box, Sphere, and Rendering Modes

Although V1 implements outlines first, the architecture must explicitly anticipate geometric readability layers.

Supported future shape types should include:

```text
collision capsule
custom capsule
box
sphere
custom geometry where appropriate
```

A visual shape must eventually support two major geometry modes.

## Follow Gameplay Collision Shape

Example:

```text
geometrySource = collision
```

The visual capsule follows the player's actual gameplay collision capsule.

Changes to the gameplay capsule therefore automatically affect the visual representation.

## Independent Visual Shape

Example:

```text
geometrySource = custom
```

The shape exists only for rendering.

It can have independent dimensions such as:

```text
width
height
radius
scale
offset
rotation
```

A larger visual capsule could therefore surround a thin player model without changing collision.

The geometry layer should eventually expose rendering behavior such as:

```text
alpha
color
frontFaceCull
backFaceCull
wireframe
depthTest
depthWrite
visibleThroughWalls
scale
offset
renderOrder
```

One important desired experiment is:

> Render a transparent capsule around the player while culling the front-facing surface so primarily the inside/back surface is visible.

This may make the player visually readable without placing an opaque object over the player model.

Example conceptual configuration:

```json
{
  "capsule": {
    "enabled": true,
    "geometrySource": "collision",
    "alpha": 0.15,
    "color": [255, 255, 255],
    "frontFaceCull": true,
    "backFaceCull": false,
    "depthTest": true,
    "depthWrite": false,
    "visibleThroughWalls": false,
    "scale": 1.05
  }
}
```

This is a future capability, not necessarily required for initial outline implementation.

However, rendering code introduced for V1 should not prevent these modes later.

---

# 7. Hot Reload Behavior

`playervisuals.json` must follow the repository-wide hot-reload behavior defined in:

`C:\mimita-priv-v8\docs\specs\hotreload\hotreload.md`

That hot-reload specification should eventually be expanded because the current document is not detailed enough.

For this system, hot reload means:

```text
edit config
→ save file
→ running game detects change
→ validate configuration
→ apply valid configuration
→ existing visible players update
```

The player must **not** need to:

```text
restart MiMITA
restart the renderer
reconnect
change server
reload the map
respawn
wait for another player to respawn
```

Existing rendered players should update as soon as reasonably possible after a valid save.

Examples:

```text
outline thickness 2 → 15
```

should update on already-existing players.

```text
enemy outline white → red
```

should update immediately.

```text
teammate outline true → false
```

should remove existing teammate outlines.

## Malformed Configuration

A malformed file must not destroy the currently working visual state.

Required behavior:

```text
last valid config active
        ↓
user saves malformed JSON
        ↓
parser rejects new config
        ↓
last known-good config stays active
        ↓
clear error is logged
        ↓
user corrects file
        ↓
next valid save applies automatically
```

Errors should identify useful information whenever possible:

```text
file path
line
column
key/property
reason
```

Example:

```text
playervisuals.json hot reload failed:
line 27, column 14
expected ',' after "outlineAlpha"

Keeping previous valid configuration.
```

A failed hot reload must not crash the game or leave player visuals in a partially-updated state.

Configuration application should behave atomically where practical:

```text
parse
validate enough for safe application
construct candidate state
apply complete state
```

rather than changing half the settings before discovering an error.

---

# 8. Competitive and Game-Mode Authority

Local player visuals are preferences, but game modes may enforce restrictions.

The initial cross-system competitive configuration lives at:

`C:\mimita-priv-v8\config\competitive.json`

This config is intended to eventually govern restrictions across multiple configurable systems, not only player visuals.

Examples include:

```text
player visuals
camera
avatar settings
weapons
movement options
gameplay settings
other client-customizable presentation or gameplay systems
```

The generalized concept is:

```text
LOCAL PREFERENCE
        ↓
GAME-MODE / HOST POLICY
        ↓
EFFECTIVE SETTING
```

For example:

```text
local:
visibleThroughWalls = true

competitive rules:
allowVisibleThroughWalls = false

effective:
visibleThroughWalls = false
```

The client may still store its preference.

The restriction only changes what is permitted in the current game mode/server.

When the user leaves that restricted mode, the original local preference should remain available.

A game mode may also eventually force exact values.

Example:

```text
competitive duel:
enemy outline = enabled
enemy visibleThroughWalls = false
outline thickness = fixed value
```

while another mode might allow full customization.

Server/host policy must be treated as authoritative where a setting provides gameplay information.

In particular, through-wall visualization is not merely cosmetic because it can reveal otherwise unavailable positional information.

Therefore a host/game mode must be able to prohibit it.

Rules should be defined as part of the generalized game-mode rules architecture rather than implementing visual restrictions through a one-off hardcoded condition.

The same architecture should eventually support statements such as:

```text
this mode permits these weapons
this mode forbids these camera settings
this mode forces these player visual settings
this mode permits these avatar settings
```

---

# 9. Extensibility and Future Experiments

V1 intentionally starts simple:

```text
fixed visual behavior
+
outline implementation
+
hot reload
+
enemy/teammate distinction
+
competitive restrictions
```

Do not initially make visual intensity dynamically depend on distance or movement speed.

The initial system should be predictable enough that changing a value has an obvious effect.

Future extensions may include:

### Distance-Based Readability

Example:

```text
near player:
outline thickness = 1

far player:
outline thickness = 5
```

### Relative Screen-Speed Readability

A player moving rapidly across the local player's screen could receive a stronger readability treatment.

Possible future behavior:

```text
slow relative movement
→ weak outline

very fast relative movement
→ stronger outline
→ optional trail
```

### Occlusion-Based Behavior

Future options might include:

```text
never through walls
outline through walls
silhouette through walls
fade while occluded
visible through walls within X distance
remember outline briefly after losing sight
```

### Combined Layers

Examples:

```text
outline + capsule
outline + silhouette
outline + trail
capsule + rim light
outline + capsule + trail
```

### Other Entities

The generalized presentation system should eventually work for things beyond human players:

```text
NPCs
bosses
projectiles
important pickups
objectives
spectated players
training targets
debug entities
```

The core architecture should therefore avoid assumptions such as:

```text
visual layer == enemy player outline
```

Prefer:

```text
entity
+
relationship/category
+
visual layer configuration
+
authority policy
=
rendered presentation
```

The system exists partially as an experimentation platform.

The implementation should therefore expose primitives rather than attempt to predict the one final visual style MiMITA will use forever.

---

# 10. Validation, Human Review, and Regression Philosophy

The initial implementation should **not begin by writing automated tests for behavior that has not yet been established as correct**.

The desired process is:

```text
implement candidate behavior
        ↓
run game
        ↓
human observes behavior
        ↓
adjust configuration / implementation
        ↓
repeat
        ↓
multiple reviews agree behavior is correct
        ↓
behavior becomes known-good
        ↓
write regression tests protecting known-good behavior
```

Automated tests should preserve confirmed behavior rather than prematurely freezing an incorrect interpretation of the specification.

Initial human review should examine cases including:

```text
stationary enemy
moving enemy
maximum-speed enemy
dashing enemy
jumping enemy
enemy at close range
enemy at long range
enemy against bright environment
enemy against dark environment
enemy against visually noisy environment
enemy partially behind geometry
enemy fully behind geometry
enemy dying
enemy ragdoll
teammate
enemy and teammate overlapping
multiple players simultaneously
explosions/effects around players
outline enabled/disabled during runtime
color changed during runtime
thickness changed during runtime
visibleThroughWalls changed during runtime
malformed config followed by corrected config
joining/leaving competitive mode
competitive override of local preference
leaving competitive mode and restoring local preference
```

Reviewers should specifically confirm:

1. the intended player is visibly easier to perceive;
2. teammates and enemies receive the correct independent configuration;
3. hot reload changes already-existing entities;
4. death behavior matches configuration;
5. wall visibility matches effective policy;
6. competitive rules correctly override disallowed local preferences;
7. malformed config keeps the previous known-good state;
8. layer ordering remains deterministic;
9. player visuals do not modify gameplay collision or simulation;
10. the result remains compatible with MiMITA's intentionally distorted, PS2-like visual direction.

Once a behavior has been observed repeatedly and accepted as correct, regression coverage may be added around the underlying known-good code path.

The goal of those regressions is:

> **Prove that a previously observed working behavior continues happening after future changes.**

They should not exist merely to obtain a passing test count.

The final design principle for this system is:

> **Player visibility is a configurable presentation problem, not an art-style limitation. MiMITA should be free to use distorted, low-resolution, strange environments while separately making gameplay entities as readable as necessary.**

And architecturally:

> **Do not build an outline feature. Build a generalized entity-visual layer system whose first real layer happens to be an outline.**