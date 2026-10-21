# Swordsword

**Status:** Desired behavior contract. The source and live runtime have not been
verified against this complete contract.

## Purpose

Define how Swordsword looks, animates, contacts actors, deals damage, and works
for both players and NPCs. Swordsword is an ordinary health-and-damage weapon;
it is not the Captain Falcon Melee-mode percent/knockback ruleset.

**Audience:** gameplay and networking implementers, animation authors, and
reviewers validating Swordsword behavior.

**This document does not define** the generic collision solver, network packet
formats, animation file schema, or the general damage/team policy. It defines
how Swordsword uses those shared systems.

This document owns Swordsword-specific behavior. Reusable physical-contact and
authoritative damage rules remain in [`weapons.md`](weapons.md) and
[`networking.md`](../networking/networking.md). Shared animation asset and
playback rules remain in [`animations.md`](../animations/animations.md).

## In-game behavior

- Use the model currently assigned to Swordsword in the active weapon
  definition. This feature does not require a new model.
- The sword is a real, continuously damaging physical weapon. Its visible blade
  and actor-contact shape follow the same weapon transform.
- Clicking selects an animation. It does not create or enable damage: contact
  can damage a target even when no attack button is pressed.
- An attack input never adds forward velocity or an impulse to the wielder.
  Normal movement and physical responses to world or actor collisions still
  apply. Contact knockback is applied to the target.

## Weapon animations and input

The equipped weapon definition selects the poses or clips. Any player or NPC
holding Swordsword uses the Swordsword animation set, regardless of avatar
identity or game mode. Do not require Captain Falcon's avatar or a Melee-mode
role to play these animations.

Players and NPCs use the same animation selection and playback path. A player
button press and an NPC's equivalent AI action resolve to the same Swordsword
animation state. The visible pose and sword transform must be available to the
same actor simulation that supplies the blade contact shape.

Avatar rigs may use a retarget map to fit a shared weapon clip to their body,
but avatar identity does not choose a different weapon move set or prevent a
compatible actor from using Swordsword animations.

| Input | Swordsword animation |
|---|---|
| Left click once | Slash animation 1 |
| Left click twice within the shared double-click window | Slash animation 2 |
| Right click once | Right-click animation 3 |
| Right click twice within the shared double-click window | Defensive pose |

The shared input/action system owns double-click recognition and its configured
window; Swordsword does not create a competing timer. A second click selects
the double-click animation and replaces the corresponding single-click
animation.

The defensive pose is animation only. It does not block damage, reduce damage,
parry, or change blade contact. After the second right click, keep the pose
while right mouse button remains held. When it is released, return to the
normal equipped Swordsword pose. NPCs enter and leave the same pose state using
their AI action state rather than keyboard input.

Animation assets still need to be authored for the four input outcomes and the
normal equipped pose. Use the shared animation format and runtime; do not add a
Swordsword-only animator. Animation playback does not decide whether contact
damages a target.

## Continuous blade contact

Swordsword contact is always active, including during its idle pose, during
each attack animation, and when the wielder is running, falling, rotating, or
being knocked back. There is no attack-active window for damage.

Use a capsule along the actual blade, from blade base to blade tip, with the
configured blade thickness. Sweep the blade between its previous and current
fixed-tick transforms so fast movement and rotation do not pass through an
actor between samples. Test contact against the shared physical body shapes of
players and NPCs. Do not substitute the wielder's root position, an
owner-centered radius, a hitscan ray, or a temporary attack-only box for blade
contact.

Use the shared physical-contact weapon system described in
[`weapons.md`](weapons.md). The weapon's existing world-collision configuration
may remain as it is; this feature does not require adding a new
`weaponcollisions.json` entry.

## Damage, force, and sound

Every valid contact uses the shared authoritative actor-damage path and normal
health, death, and kill handling. All four attacker/target pairings are
supported:

- Player damages player.
- Player damages NPC.
- NPC damages player.
- NPC damages NPC.

The server uses the real attacker and target IDs, actor kinds, and teams.
Existing game-mode and team/friendly-fire rules still decide whether a given
pair is allowed to damage each other. A permitted hit can reduce health to zero
and kill the target. Multiple overlapping body parts must not multiply damage:
Swordsword applies at most one selected damage sample per target per fixed tick,
as specified by the shared physical-contact rules.

Damage and target knockback scale with contact force. Contact speed and impact
angle both contribute: a fast, direct impact deals more damage and knockback
than a slow, glancing touch. The impact sound follows the same force direction:
stronger impacts are louder and lower in pitch; weaker impacts are quieter and
higher in pitch. Use one consistent force result for damage, knockback, and
sound so their feedback agrees.

The exact force curve, damage and knockback bounds, pitch and volume ranges,
and double-click window are tuning values, not settled numbers in this spec.
Keep them owned by the weapon/action configuration that already owns the
relevant behavior. Do not invent a second source of truth or treat placeholder
values as approved balance.

## Authority and feedback

Gameplay contact and damage run on the fixed 60 Hz simulation. The server owns
confirmed damage, knockback, health, death, and kill attribution. A client may
show immediate predicted contact feedback, then reconcile it to the server's
confirmed result without duplicating the hit sound or effect. Contact
confirmation identifies the attacker, target, weapon, and simulation tick.

## Acceptance requirements

The feature is ready for human review only after a real runtime scenario shows:

1. Each of the four actor pairings can damage and kill when game-mode team
   rules allow it, with the correct attacker and target credited.
2. Blade contact damages without an attack press, including contact caused by
   the wielder moving into a target and by the target moving into the blade.
3. Fast, direct contacts produce more damage and knockback, a louder sound,
   and a lower pitch than slow, glancing contacts.
4. Left single/double and right single/double inputs select the four intended
   animations. Holding RMB after the double-right input holds the defensive
   pose; releasing RMB returns to the normal equipped pose and never grants
   blocking behavior.
5. Players and NPCs holding the weapon use the same weapon-selected animation
   states, and the visible blade motion matches the swept contact shape.
6. Attack inputs add no impulse to the wielder. Target knockback remains
   active. Existing model and world-collision configuration need no new entry.

A successful build alone does not prove configuration loading, contact, damage,
animation visibility, or human acceptance. Record source, build, runtime
journal, and visual/gameplay review evidence separately.

## Source-review status

Source inspection on 2026-10-20 found these gaps; this is not live-runtime
proof:

- The server's physical Swordsword shape is currently gated by an attack
  timeline rather than continuously active contact.
- Local slash and lunge start functions currently add forward velocity to the
  wielder.
- NPC Swordsword combat currently has a close-range hitscan branch instead of
  the shared swept blade-contact path.
- The Swordsword weapon definition currently points at the HAFS pose set.

These are implementation gaps relative to the desired behavior above, not
alternate requirements.

## Related specifications

- [`weapons.md`](weapons.md): shared physical-contact weapon, damage, and
  knockback behavior.
- [`networking.md`](../networking/networking.md): server authority and actor
  identity/team data for damage.
- [`animations.md`](../animations/animations.md): shared clip format, avatar
  compatibility, and playback.
- [`melee-weapons.md`](melee-weapons.md): the separate Captain Falcon Melee
  mode move-set system; its percent-based match rules do not define Swordsword
  damage.

## Historical notes

The following notes are retained as history. The behavior contract above
contains the current decisions and supersedes earlier tentative suggestions.

### 2026-10-20 user notes

```text
2026 10 20 1928 make  better hmmmmm

ok so now 
well the issue right now is  like spyknife model doens show but that doenst evne relate to this 
but

swordsword
should collide for real with plauers
and npcs
and it should be like
damaging npcs and plaers

npc vs npc, npc vs plr, plr vs plr
all should work

it shoudl be like spyknife where its acive all the time just touch them and they take dmaage with it, scaled with force so , direct angle + fast speed = more dmg more knockback lower hit sound ptich higher volume et c

left click = anim one
double left click = anim two
right click = anim 3 
double right click = defensive pose, hold right click to stay in the pose
as long as rmb is held down after the double clcik that means u are holding the pose
then go back to normal pose 

todo need to make the poses animation things etc dont have tem right now  

need to make sure that npcs cna use this weapon, 
as well as 
npc vs npc
npc vs plr
plr vs plr
all should use the same damage path
bc its actor
actor vs actor damage works 

use the current model
change it so there is no impulse when u do the attakcs

also need a entry in weaponcollisions.json  just put a big box around it 
```

### 2026-10-11 earlier notes

```text
2026 10 11 1417 jorj to do idk what  really makinga  doc for this but i feel it sbetter od  do it like this 

ok so 

swordwsword is awesome
we nede to incldue it again

but the issuesa re

1. no weapon collisions netry for it

2. dash too strong, need to be like idk 10 impulse 
3. animations not right, but htats ok, we need to add like

left click once = slash
left click twice = diffrent slash
right click once = lunge
right click twice = idk diffrent thing
maybe even like a defensive pose

and it doesnt do damage to anyone, we need to do it the same as spyknife jsut like cooler 

and wepaon collisions arent really stellar right now i think

but its whatever, what i mean by that is  jgugernatu on map juggernaut4 colisions its a super tight small map so the LMG knocks u around evrywhere , cant have that
```

The old note's lunge impulse and missing-entry ideas are historical. The
current decisions above supersede them: attacks do not propel the wielder, and
no new collision entry is required.
