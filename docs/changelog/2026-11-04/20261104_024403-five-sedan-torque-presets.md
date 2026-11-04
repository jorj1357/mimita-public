# Five sedan torque presets

- Timestamp: 2026-11-03 21:44:03 EST (2026-11-04T02:44:03Z)
- Branch/commit: not captured in this session.
- Pre-existing edits: `config/vehicles/car_sedan.json` already had four wheel radii changed from `0.55` to `0.85`; preserved without modification.
- Changed file: `config/vehicles/car_sedan.json`, engine `torqueCurve` near line 108.
- Old active curve: `[[800, 120], [2500, 240], [4500, 300], [6500, 260]]`.
- New labeled presets: Current `[[800, 120], [2500, 240], [4500, 300], [6500, 260]]`; Sports `[[800, 180], [2500, 360], [4500, 450], [6500, 390]]`; Turbo `[[800, 240], [2500, 480], [4500, 600], [6500, 520]]`; Supercar `[[800, 320], [2500, 640], [4500, 800], [6500, 700]]`; Extreme `[[800, 400], [2500, 800], [4500, 1000], [6500, 875]]`.
- Reason: User requested five numbered acceleration options and confirmed preset 5 should be active. Vehicle config supports a single torque curve; the other presets are comments, with instructions for switching.
- Specification/architecture read: `docs/specs/vehicles/vehicles.md`; `docs/architecture/json-configuration/json-configuration.md`; task flow from `docs/ROUTER.md`; completion requirements from `docs/operations/task-completion/task-completion.md`.
- Skills: no focused skill used; this was a narrowly scoped configuration edit.
- Validation: `python devscripts/config_selftest.py` failed on numerous existing avatar schema findings (missing normalized keys and advanced texture fields); vehicle groups reported by that run passed. `python devscripts/quality/agent-gate.py` failed at `damage-authority.findings: 82 > baseline 81`. No baseline edits made. The JSON config loader uses nlohmann parsing with comment skipping. No runtime drive test or human handling review performed.
- Build/runtime/deployment: The agent gate ran its canonical build stage; no behavior or performance result was measured. No separate runtime scenario or deployment performed.
- Human review: Please drive the sedan and tune/choose the curve based on feel; preset names and torque levels are proposed tuning values, not runtime-tested acceleration targets.
