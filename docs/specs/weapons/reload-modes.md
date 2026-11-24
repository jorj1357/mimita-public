# Reload modes

**Purpose:** define the normal and user-controlled reload contracts for
Bloodtest firearms.
**Audience:** weapon, animation, input, HUD, network, and gameplay contributors.
**Does not:** create a second ammunition, weapon-state, or server-authority
owner.

## Mode 1 — normal reload

- Press the reload input.
- The weapon plays its reload action and refills from reserve over the
  configured reload duration.
- Firing is blocked until the reload completes.
- Cancellation, ammo accounting, and cooldowns use the existing weapon runtime.

## Mode 2 — user-controlled reload

Mode 2 is a per-weapon interaction state machine. It is specified now and
implemented later.

### Revolver

1. Empty the cylinder. A fire attempt produces a dry-fire sound and no shot.
2. Press **T** to enter reload mode. Spent casings are ejected.
3. Press **R** once per round to load one round.
4. Press **T** again to leave reload mode. The revolver is fireable when it has
   at least one loaded round.

### AK-47

1. Empty the magazine. A fire attempt produces a dry-fire sound.
2. Press **T** to enter reload mode. The old magazine is removed and dropped as
   a physical or visual object.
3. Press **R** to insert a new magazine from reserve.
4. Press **T** again to leave reload mode and make the rifle fireable.

### Shared rules

- No forced wait: the player controls when each step happens.
- Entering reload mode does not consume reserve ammo.
- Firing is blocked during reload mode.
- Cancelling returns the weapon to its previous state without duplicating ammo.
- The server validates every transition; the client predicts the animation and
  HUD state.
- Ammo counts, magazine state, and the active mode replicate through the shared
  weapon-state path.

## HUD

- Show the current reload mode, rounds loaded, reserve, and the next valid input
  (T to enter/exit, R to load/swap).
- A dry-fire attempt shows a dry-fire indicator and plays the dry-fire sound.

## Evidence

| Event | Required fields |
|---|---|
| `weapon.reload.mode.changed` | actor, weapon, previous mode, new mode, tick |
| `weapon.reload.round.loaded` | actor, weapon, loaded, capacity, reserve after |
| `weapon.reload.magazine.swapped` | actor, weapon, magazine id, reserve after |
| `weapon.dry_fire` | actor, weapon, loaded, reason |
