# Projectile Firearms

**Purpose:** define shared ballistic behavior and networking for Bloodtest
firearms.
**Audience:** weapon, projectile, networking, physics, audio, and gamemode
contributors.
**Does not:** create a new projectile owner, promise a performance level without
measurement, or make clients authoritative for damage.

## Core contract

- Every Bloodtest firearm listed in the individual weapon specs fires one or
  more physical projectile bodies. Hitscan, instant beams, and hitscan fallback
  damage are forbidden for these weapons.
- Projectiles have finite muzzle velocity, gravity-driven drop, configurable
  drag, mass/size, lifetime, and per-weapon impact response. Resolve each
  projectile's swept world and actor contacts through the shared authoritative
  projectile/contact and damage owners at the fixed 60 Hz gameplay tick.
- Client prediction may render the locally fired projectile immediately and
  may predict cosmetic impact feedback. Only the server confirms contact,
  damage, body-part changes, and kills. A client hit claim may identify a
  predicted contact for validation; it cannot directly apply damage.
- Weapon fire sequence IDs and deterministic per-shot seeds let the server
  reconcile locally predicted projectiles. Reconciliation must not create a
  second visible projectile or silently alter the authoritative trajectory.

## Networking and load

- Batch transport records for multiple projectile spawns/updates when this
  reduces packet and handler overhead. Batching is a transport optimization:
  every round remains an individually identified ballistic trajectory with its
  own collision, impact, damage, and lifetime.
- Prefer compact fire commands (weapon, sequence, tick, aim/muzzle state, and
  seed) over per-frame client position streams. The server derives the accepted
  muzzle and trajectory from authoritative actor/weapon state, validates the
  command, simulates the projectile, and publishes confirmed contact.
- Use the existing cached broadphase and shared projectile stepping path;
  avoid per-projectile allocation and duplicate world queries where possible.
  Do not reduce the fixed simulation rate or silently skip accepted shots to
  meet a load target.
- If overload is measured, expose bounded queues and categorized diagnostics;
  retain accepted shot ordering and report rejected/expired work explicitly.
  Any update batching or interest filtering must preserve contact ordering and
  provide remote clients timely projectile and impact presentation.
- Acceptance measures server tick stability, CPU cost, packet volume, and hit
  latency for the target Bloodtest roster with simultaneous automatic fire.
  A guessed client-side damage authority is not an acceptable optimization.

## Shared ballistic and presentation tuning

Each weapon config owns at least projectile velocity, gravity scale, drag,
projectile mass/radius, lifetime, muzzle offset, spread, impact-damage profile,
recoil, fire cadence, magazine/reserve, reload, equip/unequip duration, and
sound/model references. All fields use the existing weapon definition and
validated live reload path. A Bloodtest mode overlay may override any declared
field through an atomic deep merge by stable weapon ID; changing a field must
not reset unrelated ammunition, cooldown, projectile, or reload state.

Bullet drop must be visible and measurable over a declared range, not faked by
moving a rendered sprite away from its actual collision trajectory. Tuning
may make a projectile lethal on impact, but finite travel time means firing is
not an instant hit. Damage and head/body lethality remain separate balance
decisions.

World contact produces material-appropriate bullet-hole marks and a short,
pooled debris burst. Reuse existing impact/decal and debris owners; do not use
rocket/grenade explosion visuals or spawn unbounded debris. Actor contact uses
the Bloodtest blood/body-part impact event.

## Acceptance

At short, medium, and long ranges, record launch parameters, per-tick trajectory,
drop, first contact, authoritative result, and reconciliation. Verify there is
no hitscan damage path, no duplicate projectile, and no client-authoritative
damage. Run concurrent-fire load at the intended roster and capture tick,
resource, packet, and hit-latency evidence before setting a performance claim.

## Individual weapons

- [AK-47](ak47.md)
- [M4A1](m4a1.md)
- [FAMAS](famas.md)
- [Desert Eagle](desert-eagle.md)
- [Judgment revolver](judgement.md)
- [Glock 17](glock-17.md)
- [Sawed-off double barrel](sawed-off-double-barrel.md)
- [AA-12](aa12.md)
- [Uzi](uzi.md)
