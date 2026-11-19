# In-engine weapon and melee authoring

Status: desired behavior. The current Map Editor does not create reusable
weapon assets or author weapon animation clips.

## Goal

Let a creator build a reusable melee weapon, its held presentation, its idle
pose, and its attack timeline entirely in MiMITA. The first deliverable is a
Juggernaut bat authored from reusable primitive parts and keyframed transforms.
This does not require arbitrary polygon sculpting, skinning, or an external
rigging package.

The authoring surface may be exposed from the in-game Map Editor, but weapon
assets are not map geometry. They are saved under the stable item definition
and referenced by the normal weapon registry. Editing or switching maps must
not move, duplicate, or delete a weapon asset.

## Ownership and saved data

- The weapon definition owns item identity, display name, contact behavior,
  damage bounds, cooldown, and the authoring asset reference.
- The weapon model asset owns an ordered list of reusable primitive parts.
  Each part has a stable ID, primitive shape (initially box, cylinder, or
  sphere), local transform, dimensions, and material/color.
- The animation asset owns named clips, fixed-tick duration, pose keyframes,
  interpolation, and phase markers. Clip IDs are stable and referenced by the
  weapon's melee action.
- The editor owns previews and editing interaction. It does not own a second
  copy of gameplay damage, contact, or animation timing.
- Save, undo, redo, and reload operate on the same asset revision. Invalid
  edits remain visible in the editor but cannot replace the last valid runtime
  revision.

Use the existing weapon definition location `config/weapons/<id>/config.json`
and shared animation library conventions. The precise asset subdirectory and
serialization schema must be selected during implementation after checking
the current loaders; do not store authored weapon assets inside a map JSON.

## Editor workflow

The editor must provide a weapon-authoring workspace with:

1. Part list and viewport preview; add, duplicate, reorder, rename, and delete
   primitive parts.
2. Transform fields and viewport handles for position, rotation, and scale.
3. Timeline with clip selection, fixed-tick playhead, keyframe insertion,
   deletion, and playback preview.
4. Named poses and clips, including idle and action clips. A melee action can
   reference windup, active, recovery, and return-to-idle poses/clips.
5. A visible active-hitbox preview. Damage geometry uses the same authored
   hitbox definition consumed by client prediction and server validation; a
   debug sphere is not a separate damage implementation.
6. Validation that reports missing part IDs, invalid dimensions, invalid
   phase ranges, duplicate stable IDs, or missing clip references before
   publishing a revision.

All gameplay time is fixed 60 Hz ticks. The timeline must show phase boundaries
and clearly distinguish presentation keyframes from damaging active windows.

## Melee contact and damage

Melee hit geometry is a swept capsule/OBB or a bounded collection of swept
spheres attached to authored weapon parts. For each active tick, query overlap
against actor body volumes. A target is damaged at most once per swing unless
the weapon explicitly authors repeat-hit cadence. Contact resolves through the
shared server damage owner; no weapon/editor code writes actor health.

The Juggernaut bat starts with a minimum damage of 10 per confirmed hit, even
at the edge of the valid contact volume. Damage may rise toward an authored
maximum based on proximity to the closest active hit sphere's center. The
server accepts a client-predicted target/contact only when identity, generation,
action window, reach, world occlusion, and contact volume are plausible. The
client does not author final damage or bypass the shared damage sink.

Active contact volumes may be displayed as red editor/debug spheres. Their
visualization is a view of the same geometry used by the contact query and
cannot independently deal damage.

## Juggernaut integration

Add a dedicated `juggernaut_bat` item to the Juggernaut mode role loadout. Do
not change Force Punch or other roles as a side effect. The bat's model,
animation, active hit volumes, damage bounds, sounds, and cooldown are data;
the existing shared melee, animation, and damage owners execute them.

The initial swing timeline is idle -> windup -> active -> recovery -> idle.
The bat's authored dense feel is visual/physics presentation metadata and must
not be inferred from its visible volume for actor damage. If world impact mass
or destructible-world interaction is requested, it remains governed by that
separate contact profile.

## Acceptance evidence

- Create and save the bat from the in-game authoring workspace without Blender
  or hand-editing asset files.
- Reopen the item and verify the primitive model, keyframes, and phase markers
  persist through save/reload and undo/redo.
- Equip the bat in a live Juggernaut match and confirm idle, windup, active,
  recovery, and return-to-idle presentation.
- Confirm the active red hit volumes match the damaging swept geometry.
- Record close and edge contacts in the live `events.jsonl`; each accepted hit
  applies at least 10 damage, proximity affects damage monotonically, and each
  target receives no unintended duplicate damage during one swing.
- Confirm rejected impossible claims do not damage, while valid predicted
  contacts are accepted despite normal interpolation differences.
- Verify other roles using Force Punch retain their existing behavior.

Build, runtime journal, visual review, and human gameplay acceptance are
separate evidence claims.
