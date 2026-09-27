// 2026-09-27T19:39:50Z
/* purpose
* record the body-mesh false-contact and old-contact-sticking fix
* preserve spark visuals unchanged
* does NOT claim human gameplay acceptance
*/

# Task

- Summary: Stop body triangles from colliding with far-away or merely
  projected world triangles, and stop a limb from remaining stuck to a surface
  after its current pose has moved away.
- Status: CODE_COMPLETE / DEV_BUILD_VERIFIED / DETERMINISTIC_TEST_PASS /
  HUMAN_REVIEW_REQUIRED
- Date, time, timezone: 2026-09-27T19:39:50Z, ISO 8601 UTC

# Root cause

- `pointInTriangle` used barycentric coordinates without checking distance from
  the triangle plane. A point far in front of a large wall or cylinder could
  therefore be classified as inside its projected triangle.
- The swept body-mesh test accepted a collision found only at the previous
  pose. When the limb moved away, that historical contact could keep applying
  correction toward the old surface.

# Implementation

- `src/physics/movement/physics-collision-mesh.cpp`: require triangle-plane
  agreement before accepting barycentric containment; accept current overlap as
  penetration; accept swept crossings after the sweep starts; ignore a contact
  that exists only at the old pose while the limb is leaving.
- `src/physics/movement/physics-collision-stress.cpp`: deterministic tests now
  cover thin-wall crossing and a limb one meter away after leaving an old wall
  contact.
- No spark rendering or hit-effect code was changed.

# Validation

- Live development build `0058` compiled the changed collision source.
- `C:\mimita-v9\.dev\builds\0058\mimita.exe --collision-selftest` passed:
  mesh limb floor contact, swept limb thin-wall crossing, limb leaving old
  wall contact, and the complete collision self-test.
- The standalone build wrapper later reported a linker resource-lock error
  because the live `.dev` executable was running; the wrapper's success text is
  not treated as link proof. The live build and self-test above are the valid
  evidence for this change.
- Human gameplay acceptance against the pictured Train and Chain of Judgement
  large objects remains required.
