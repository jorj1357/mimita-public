# Duel damage number presentation fix

- Root cause: the packet dispatcher sent `matchMode == "duel"` state only to `DuelQueue`, skipping `CommunityMatchClient`, the owner that applies mode JSON presentation overrides.
- Fix: route accepted match state through `CommunityMatchClient` for every mode, and continue forwarding Duel state to `DuelQueue` for its queue UI.
- Build/runtime: canonical dev loop published build 2733 and launched matching Duel server/client processes. Journal: `logs/2026-10-21/20261021_084608/events.jsonl`.
- Runtime evidence: client event `client.gamemode.presentation.applied` records `mode: "duel"`, `damage_numbers: false`, `hit_effects: false`, and the configured effect scales.
- Human acceptance: visually confirm a confirmed hit produces no floating damage number. This run proves the mode setting reached the client owner, not the final rendered appearance.
- Regression note: `docs/regressions/2026-10-21/duel-mode-presentation-packet-routing-REG.md`.
