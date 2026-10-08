# Hybrid weapon collision bounce investigation

## Scope

Investigated why a weapon set to zero bounce can still appear to push the player
away from walls when `config/aimbody.json` uses hybrid mode. No gameplay source
or configuration was changed by this investigation.

## Findings

- The working tree currently has `player_bounce: 0.99` enabled for the
  revolver and the other listed weapons. The zero value is commented out.
- `WeaponCollisionJsonConfig::parseOneWeapon()` clamps and loads the field,
  and `applyCollisionConfig()` stores it in the player's runtime collision
  state.
- The active actor-triangle solver applies that multiplier only when the
  contact label is exactly `weapon`. Body-part contacts such as `rightArm`
  retain the global `config/collision.json` bounce strength.
- Hybrid mode physically simulates all six body parts and then blends them
  toward the procedural pose. Those body-part sweeps therefore remain capable
  of producing a global-bounce wall response even when the weapon's own
  `player_bounce` is zero.
- The weapon box is a separate actor mesh labeled `weapon`, so the JSON value
  does control the configured weapon box contact itself. It does not control
  the arm that carries the weapon or other hybrid body geometry.
- Collision is dispatched inside the normal fixed-tick movement update before
  the per-tick hybrid aim-body update. This ordering means the hybrid pose and
  its attached weapon can change between collision ticks, which can create
  repeated fast contacts at a wall. Existing logs did not contain a bounded
  weapon-contact trace proving the reported wall reproduction.

## Evidence status

- Source/ownership: confirmed by inspection of the JSON loader, actor mesh
  collector, actor-triangle response, fixed-tick dispatch, and hybrid spring.
- Build: not run; this was an investigation-only request.
- Runtime: not performed in this session. Existing runtime logs show hybrid
  aim-body activity but do not identify the reported contact labels, normals,
  penetration, or velocity response.
- Human acceptance: still required.

## Likely explanation

There are two overlapping sources of the apparent bounce: the current
configuration explicitly enables substantial weapon bounce (`0.99 * 0.35`
global strength), and hybrid body-part collision can independently launch the
player through the `rightArm` or another physical part. Default mode avoided
the second source because the body was not being run as a physical hybrid
ragdoll.

## Next diagnostic gate

A fresh real-executable reproduction should record contact label, weapon bounce
scale, normal, penetration, part sweep velocity, player velocity before/after,
and the later hybrid pose update. That will distinguish weapon-box bounce from
right-arm/body bounce before any fix is selected.
