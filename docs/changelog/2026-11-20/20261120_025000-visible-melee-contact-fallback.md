// 2026-11-20
/* purpose
* record the visible melee contact fallback and the pending live build proof
* preserve the client/server parity boundary and the unchecked runtime claim
* this file DOES NOT claim a 99.99% hit rate yet
*/

# Visible melee contact fallback

# Task

- Task ID: melee-networking / melee-visible-contact
- Session ID: session-88c13dfe4d39
- Owner: melee-networking
- Issue: issue-04079f42c6f9
- Focus: a client-visible weapon contact must become a client claim and a
  server-plausible accepted damage event
- Summary: added a blade-capsule fallback when configured weapon damage boxes
  miss a visually intersecting target; the fallback uses the same blade shape
  in the claim and records `contact_source`/`blade_fallback` for parity.
- Status: SOURCE_READY; game-DLL build pending; no live parity proof yet
- Date, time, timezone: 2026-11-20T02:50:00+00:00
- Final commit: uncommitted shared-checkout edits

# Requested behavior

The player reports that the weapon visually passes through an enemy without a
hit. Desired: when the client sees the weapon hit, send that same contact; the
server must accept it when historically plausible; prediction and authoritative
damage should agree 99.99% of the time.

# Exact implementation changes

## `src/combat/melee-client.cpp`

- Added `applyBladeFallback()` inside the per-target contact path.
- If configured `damageBoxes` exist but none overlap the target, the client now
  checks the visible blade capsule against the target root capsule instead of
  silently returning.
- The claim keeps the blade box center/half/axes when the fallback is used, so
  the server receives the same visible shape.
- The client contact event now reports:
  - `contact_source`: `damage_box`, `blade_shape`, or `blade_fallback`;
  - `blade_fallback`: boolean.
- Existing damage-box hits remain unchanged.

## `src/network/server-packet-handlers.cpp`

The earlier claimed-shape change remains in the source:

- the client-claimed OBB/capsule is validated against the rewound target;
- the second independent body-volume check is disabled;
- accepted/rejected events carry `stage`, target generation/kind, and claimed
  shape fields.

# Validation

- Build note `ec12bcee71b2` queued before the client edit.
- Game-DLL build `game-dll-20261120T024625732466Z-8281b0` (source 58) was
  accepted and was in `manifest_snapshot`/compilation when this record was
  written.
- No live NPC or PvP melee run has yet been performed with this build.
- No 99.99% claim is made.

# Next exact step

After the root dev-loop publishes the game DLL:

1. Launch a clean TDM/NPC or PvP run.
2. Exercise visible Mace/Chainsaw/SwordSword contact.
3. Correlate:
   - `melee.client.contact_claim` (including `contact_source`);
   - `melee.contact.claim.accepted/rejected` (including `stage` and `contact_id`);
   - `server.damage.applied` / client reconciliation.
4. Fix the first divergence; repeat until the visible-contact-to-accepted-damage
   rate is proven.
5. Add the live evidence to `issue-04079f42c6f9`; do not close on source/build
   alone.

# How to reverse

- `git checkout -- src/combat/melee-client.cpp`
- Reverse only files touched by this session; preserve pre-existing changes.
