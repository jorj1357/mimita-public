# Terminal aliases: sandbox and Zombie Tower

- Timestamp: 2026-10-13T23:05:32Z (display timezone: America/New_York)
- Branch: `20261011zt`
- Result: PASS_WITH_HUMAN_REVIEW

## Change

Added the plain `sandbox` alias at `config/command-aliases.json:7`:

```text
"sandbox": "server_start_preset sandbox_1"
```

Added `zt1` at `config/command-aliases.json:8`:

```text
"zt1": "server_start_preset zombie_tower_1"
```

Added the supporting `zombie_tower_1` server preset at
`config/server-presets.json:34-42`, configured for map `zombietower4`, mode
`zombie_tower`, no startup NPCs, no map rotation, and automatic host joining.
The existing working-tree edits for `ss`, `sandbox_1`, and `h` were preserved;
they predated this session's changes.

## Documents and focused review

- `docs/ROUTER.md`
- `docs/architecture/terminal-commands/terminal-commands.md`
- `docs/skills/terminal-command-checker-v1.md`
- `docs/operations/task-completion/task-completion.md`
- `docs/architecture/time-and-formatting/time-and-formatting.md`
- `docs/specs/gamemodes/zombie-tower.md`
- Memory note: terminal alias ownership and live reload guidance in `MEMORY.md`.

The terminal-command review is satisfied at the configuration level: both
aliases resolve through the existing `server_start_preset` owner, which starts
the local server and joins the host instead of creating a second command path.

## Validation

- `config/command-aliases.json` and `config/server-presets.json` parsed
  successfully after removing the repository's supported `//` comment lines.
- `assets/maps/zombietower4.glb` exists.
- `config/gamemodes/zombie_tower.json` exists.
- `git diff --check` reported no issue in the files changed here. It reported
  a pre-existing trailing-whitespace warning in
  `docs/jorj-docs/20261013plan.md:17`.
- No build was run because this is a JSON-only configuration change.

## Human review still needed

Run `sandbox` and `zt1` in the live client and confirm the terminal reports a
successful preset start and the client enters the expected map/mode. Source
and config validation do not prove live gameplay acceptance.
