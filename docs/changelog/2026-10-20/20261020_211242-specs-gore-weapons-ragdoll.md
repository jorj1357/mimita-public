# Update gore, weapon falloff, and ragdoll decisions

- Result: `PASS_WITH_HUMAN_REVIEW`.
- Session time: 2026-10-20 17:12:42 EDT (`2026-10-20T21:12:42Z`).
- Branch: `20261011zt`.
- HEAD: `7556a218ec6b6bb8443485ff47b51c9ebf844e0d`.
- Scope: documentation/specification edits only; no code or configuration files changed.
- Pre-existing workspace changes: all three target spec files already had uncommitted user-authored top notes. Those notes were preserved. Other workspace edits, including the earlier Zombie Tower changelog and normal-zombie movement changelog, were left untouched.

## Changes and exact content

### `docs/specs/visuals/blood-bulletholes-viscera.md`

- Metadata line 26: before `Last updated: 2026-09-09`; after `Last updated: 2026-10-20`.
- Purpose, lines 33-42: before, “The primary immediate goal is to preserve or improve the existing visual quality ... while radically reducing their frame-time cost” and “the desired output is known, but the current representation and implementation are too expensive.” After, the user-reported V1 blood presentation is good/already in place as of 2026-10-20, the 2026-09-07 slowdown note is historical, and current work extends the baseline with part health, gore, viscera, limb destruction, and large explosion marks.
- After Blood V1, lines 269-302: added current-work requirements: hidden 100-health body parts with JSON ownership/schema TODO; torso 100 -> 85 after 15 damage; hit-location blood; zero-health part explosion with sound and movable red organ/meat/bone pieces; sound and GLB TODOs; mixed-size pools; 5000 damage against 100 health as an example with about 95% large spray; large black bomb decal; and 240 FPS target with hardware/scenario TODO.
- Blood V2+ lines 306-318: removed body parts, organs, viscera, tissue destruction, and detached geometry from future-only work because these are current scope now.
- V1 scope lines 1817-1841: renamed to distinguish the user-reported existing baseline from the current gore/limb-destruction work.
- V1 non-goals lines 1845-1858: removed “full gore destruction”; detailed organ simulation remains a non-goal.
- Required end behavior lines 1933-1935: clarified “much later” means later in the same running game/server; persistence over restart is not required.

### `docs/specs/weapons/weapons.md`

- Acceptance line 261: before, “Add a test weapon through validated data and asset references only”; after, it must be defined at `config/weapons/<weapon-id>/<weapon-id>.json` with referenced assets.
- Data-only additions, lines 275-300: changed the desired source of truth from one growing `config/weapons.json` to one per-weapon JSON file. Added gradual migration, the rule that the per-weapon file wins over its temporary aggregate duplicate, and the requirement to remove compatibility only after migration completes.
- New linear falloff section, lines 317-336: added `damage = 15 + impactFactor * (45 - 15)`, where distance and impact angle determine the factor; close/direct approaches 45, far/glancing approaches 15. The same factor scales knockback. Exact normalization and weapon knockback bounds remain TODOs. User-reported “already working” status is identified as not runtime-verified here.
- Melee definition line 347: before, the moveset data was in “that one `config/weapons.json` definition”; after, “that one per-weapon JSON definition.”

### `docs/specs/ragdoll-retrograd/ragdoll-retrograd.md`

- Metadata line 38: added `**Updated:** 2026-10-20`.
- New current decisions section, lines 49-76: separated lethal-hit ragdoll (discard old movement velocity and use the lethal hit's direction/strength) from the alive `G` toggle (preserve velocity); recorded stronger-hit/stronger-push behavior; min/max push and owner TODO; stable single crosshair while RMB is held; `C:\mimita-v9\config\aimbody.json` ownership; and full hit-caused gore as current work in the visuals spec.
- Grab strength section lines 836-867: clarified that dismemberment from hit damage is current work, while grab failure/grip strain still does not dismember in v1.
- Arcade body strength section lines 1106-1125: clarified that stretching alone does not cause damage/gore; hit-caused part health and destruction are current work.
- Momentum section lines 1178-1196: narrowed momentum preservation to alive `G` toggles and explicitly routed death to the lethal-hit rule in §3.2.
- Non-goals lines 1756-1770: replaced general dismemberment exclusion with the narrower “dismemberment from grab failure or stretching alone.”
- Invariants lines 1919-1929: added RAG-046 (death uses lethal hit impulse), RAG-047 (stronger hit means stronger push within future min/max), and RAG-048 (one stable RMB crosshair).
- Existing links into `C:\mimita-priv-v8` were left unchanged per request.

## Review and validation

- Read `docs/ROUTER.md`, the three target specifications, `docs/skills/documentation-checker-v1.md`, `docs/skills/spec-behavior-review-v1.md`, `docs/architecture/time-and-formatting/time-and-formatting.md`, and `docs/operations/task-completion/task-completion.md`.
- The router's `docs/doc-review-09-03-2026.md` file was not present in this checkout (`rg --files docs` found no matching file). The focused documentation and specification-behavior skills were used instead.
- Specification behavior review: PASS for decisions supplied directly by the user. Remaining values/owners are labeled TODO rather than invented.
- Documentation check: PASS_WITH_HUMAN_REVIEW. Cross-references between the gore and ragdoll specs are valid. The `aimbody.json` path exists. Existing v8 links remain intentionally untouched. The blood document's user-authored first note remains dated 2026-10-29, which is future relative to this 2026-10-20 session; no replacement date was guessed.
- `git diff --check`: no new whitespace issue in authored prose. It reports trailing spaces in the pre-existing user-authored raw notes at the tops of the three documents; those notes were preserved.
- Tests/build/runtime: not run; this task changed specifications only. User-reported implementation status is recorded as user-reported, not as independently verified runtime evidence.
- Open decisions retained: part-health JSON schema/owner; gore sound IDs and GLB assets; exact spray/puddle tuning and benchmark hardware/scenario; weapon distance/angle normalization and per-weapon knockback limits; death push min/max and configuration owner; cause of RMB crosshair jitter; staged migration implementation for per-weapon JSON.
- No confirmed regression was found and no regression file was added.
