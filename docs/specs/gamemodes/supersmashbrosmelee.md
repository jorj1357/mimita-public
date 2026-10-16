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
