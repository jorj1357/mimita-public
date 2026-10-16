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

---

## Structured open issues

Status: user-reported, not yet runtime-reproduced in this documentation task.

- **Own model:** define and provide a SwordSword-specific model/asset contract;
  do not assume a generic melee model is sufficient.
- **Own authored move set:** left-click first/second attacks and right-click
  first/second attacks need distinguishable clips and gameplay states. A
  defensive pose is a possibility, not a committed move. Use the shared
  animation runtime and proposed Blender export path rather than creating a
  second animator.
- **Damage:** the informal note says the weapon currently does not damage
  targets. Trace prediction, contact, server validation, and authoritative
  player/NPC damage through `docs/specs/weapons/weapons.md` and
  `docs/specs/networking/networking.md`; do not infer a root cause.
- **World collision:** add a defined collision profile and test it with the
  Juggernaut LMG scenario in the collision issue index. The rough “10 impulse”
  dash value in the notes is a tuning suggestion, not an accepted number.

See `docs/specs/animations/blender-animation-authoring.md` and
`docs/specs/gamemodes/supersmashbrosmelee.md` for the related animation/mode
proposals.
