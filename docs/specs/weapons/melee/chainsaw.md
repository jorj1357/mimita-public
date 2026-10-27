# Chainsaw

**Status:** Desired behavior contract for the placeholder implementation.

## Purpose

Chainsaw is a continuous-contact melee weapon for players and NPCs. It shares
the physical-contact and authoritative damage owners used by Swordsword. It is
not a separate damage system.

## Behavior

- The damaging blade is always active while the chainsaw is equipped. Contact
  damages without an attack press.
- Base contact damage is 5–15 per valid contact tick, scaled by contact force.
- A primary click revs the chainsaw. On the tick the click registers, the
  damage multiplier becomes 2x, producing 10–30 base damage per contact tick.
- The rev multiplier decays linearly back to 1x over 60 fixed 60 Hz ticks, not
  render frames. Each new click resets the multiplier to 2x.
- Revving extends the visible and physical reach forward. Contact follows the
  animated weapon transform.
- A click does not add a dash or self-impulse.
- There is no generic melee cooldown. The action may restart on every click.
- The rev action is usable on the ground and in the air by default.

## Feedback

- Contact plays a chainsaw hit sound.
- Rev pitch and volume scale with the current rev multiplier: 1.2x pitch and
  1.2x volume at 2x damage, 1.0x at 1x damage, interpolated across the same
  60-tick decay.
- The weapon tint follows the same multiplier: bright red at full rev, darker
  near-black red at base damage.

## Authority and assets

- Use `assets/objects/weapons/mimita-chainsaw-v1.glb`.
- Use `assets/sound/weapon/chainsaw/chainsawequip.wav`,
  `chainsawhit1.wav`, and `chainsawidle.wav`.
- Player and NPC contacts use the shared animated physical-contact owner.
- A placeholder procedural pose set may be used until authored clips exist.

## Acceptance

Runtime evidence must show continuous contact without clicking, the click reset
to 2x damage, the 60-tick decay, extended reach, and the same path working for
players and NPCs. Build success alone is not acceptance.
