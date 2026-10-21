# Swordsword Melee punches and movement layering

- Result: `PASS_WITH_HUMAN_REVIEW`. Configuration checks and canonical build passed; in-game visual feel and attack input still need a live match check.
- Local time: 2026-10-20 22:22:13 EDT.
- Canonical time: 2026-10-21T02:22:13Z.
- Branch: `20261011zt`.
- HEAD: `f6d6eeb2ce409153c8d23933b32c77eee39a5b48`.
- The client remained on build 2718. No client or server restart was issued. Dev-loop status showed the client present and its recorded server PID no longer alive.

## Requested behavior

All Swordsword attacks use shared Captain Falcon Melee punch clips. The actor keeps its avatar's movement poses and hybrid AimBody instead of being held in a weapon idle pose. Ground and air inputs both resolve. A fresh attack replaces the current animation immediately, without cooldown or fire delay.

## Cause and changes

The weapon animation profile had `poseComposition: authored_only`, which made `Player::usesMeleeAuthoredPose()` suppress hybrid body, aim, ragdoll presentation, and other procedural layers for any weapon with an animation profile. Its state map also assigned the same Swordsword idle clip to walking, running, jumping, and falling. The punch actions were still pointing to custom Swordsword clips.

The profile now includes the shared `punch_a`, `uppunch_a`, and `downpunch_a` clips. The three attacks reference those clips; the defensive hold keeps its dedicated defensive pose. The profile no longer forces authored-only composition or overrides avatar movement states. `usesMeleeAuthoredPose()` now honors the profile's explicit authored-only policy and retains the existing Captain Falcon move-set behavior. Avatar state selection is preferred for locomotion while weapon-profile clips remain available for actions.

The Swordsword `fire_delay` is zero. The existing physical-contact attack path skips weapon cooldown rejection; each accepted action start overwrites the server action state. The four Swordsword actions have no grounded/airborne actor-state restriction, so they resolve in either state. These source paths were inspected; live input/animation was not observed in this update.

## Changed-file excerpts

Quotes are verbatim. “No prior text” describes the addition; deleted lines have no replacement quote where noted.

| File | Line | Before | After |
|---|---:|---|---|
| `config/weapons.json` | 353 | `"fire_delay": 0.03,` | `"fire_delay": 0.0,` |
| `config/weapons.json` | 412 | `"animation_clip_id": "swordsword_slash_one",` | `"animation_clip_id": "punch_a",` |
| `config/weapons.json` | 418 | `"animation_clip_id": "swordsword_slash_two",` | `"animation_clip_id": "uppunch_a",` |
| `config/weapons.json` | 426 | `"animation_clip_id": "swordsword_right_attack",` | `"animation_clip_id": "downpunch_a",` |
| `config/animations/library/weapons/swordsword.json` | 3 | `"poseComposition": "authored_only",` | `"include": [` |
| `config/animations/library/weapons/swordsword.json` | 4 | No prior text | `"config/animations/library/melee/captainfalcon/punch_a.json",` |
| `src/entities/player-animation.cpp` | 206 | `return def && (!def->animationProfile.empty() ||` | `if (!def->animationProfile.empty()) {` |
| `src/entities/player-animation.cpp` | 394 | `const AvatarStateMap* sm = weaponAnimationSet && weaponAnimationSet->valid` | `const AvatarStateMap* sm = animationSet && animationSet->valid` |

## Validation and evidence

- Parsed the weapon and animation JSON with the repository JSONC loader. Confirmed all four clips resolve, attacks use the three shared punches, actions have no attack hitboxes or self impulses, action inputs have no ground-state restriction, and `fire_delay` is zero.
- `git diff --check` passed for the three changed implementation/config files.
- Canonical dev-loop build 2722 succeeded. `--versioninfo` identified `C:\\mimita-v9\\.dev\\builds\\2722\\mimita.exe`.
- Routed animation, avatar, movement, collision, and multiplayer documents were reviewed. The Swordsword spec's requirement that the weapon reuse the shared animator and that the root remain movement-owned guided this change.
- The current live client was not replaced. The dev-loop status showed client build 2718 still present and its server PID absent. No real grounded/air input run or visual HybridBody/punch review was performed; build and JSON checks do not prove either.

## Preserved work

Only `config/weapons.json`, `config/animations/library/weapons/swordsword.json`, and `src/entities/player-animation.cpp` were edited for this request. Other pre-existing files and changelogs were left untouched.
