# Dev Loop launch preflight ordering

Time created: 2026-10-24T02:13:41Z
Display time: 2026-10-23 22:13:41 EDT
Branch: 20261011zt
HEAD: 2103321b012f546c07bb418add26269c66bbace0

## Final state

Changed `devscripts/dev-loop.py`, `DevLoop.launch_latest`, around line 2314. The loop now validates its firewall/bind launch policy before stopping the current client. This prevents a rejected preflight from closing the running game without a replacement.

The live loop status observed during this session was mode 1, running build 2976, latest build 2978, with client and server readiness true. That means pressing `[1]` would select a newer build and replace the older runtime; this is not a same-build reuse scenario. The user's specific mode-15 report was not reproduced during this session.

## Exact source change

Before:

```python
        # Capture one immutable published artifact. A worker may publish a newer
        # build concurrently, but this launch keeps one server/client identity.
        self.stop_processes()
        self.client_ready = False
        self.builds_match = False
        print(f"[DEV] selected allowed map: {map_name}")
        if not self.validate_firewall_policy():
            return False
```

After:

```python
        print(f"[DEV] selected allowed map: {map_name}")
        if not self.validate_firewall_policy():
            return False
        # Reject an unsafe launch configuration before stopping the current
        # client. A failed preflight must not turn a launch request into a game
        # shutdown with no replacement.
        # Capture one immutable published artifact. A worker may publish a newer
        # build concurrently, but this launch keeps one server/client identity.
        self.stop_processes()
        self.client_ready = False
        self.builds_match = False
```

## Pre-existing work

The checkout had numerous unrelated modified and untracked files before this task, including user changes to launch-mode configuration and gameplay work. They were left untouched. `devscripts/dev-loop.py` had no pre-existing modification in the initial status.

## Instructions and review

Read `docs/ROUTER.md`, `docs/specs/dev-loop/dev-loop.md` (§§7–8), `docs/architecture/terminal-commands/terminal-commands.md`, `docs/architecture/live-development/live-development.md`, `docs/operations/build-and-exe/build-and-exe.md`, `docs/operations/task-completion/task-completion.md`, `docs/workflows/runtime-scenario-validation.md`, `docs/skills/terminal-command-checker-v1.md`, and `docs/regressions/README.md` plus the existing Dev Loop regression.

Focused review: the key `1` maps to an explicit build-or-launch request. The Dev Loop specification says a healthy matching client/server pair is reused, while an incompatible owned pair may be replaced through checks. The launch code already has a reuse path for a matching live client and server. This correction only moves launch-policy preflight ahead of client shutdown; it does not change build selection or prove that the reported mode-15 scenario works.

## Validation and remaining evidence

- `python` AST parse of `devscripts/dev-loop.py`: PASS.
- Read-only Dev Loop status: daemon running; mode 1, client PID 7588, server PID 33420, running build 2976, latest build 2978; readiness flags true.
- No game or Dev Loop process was restarted. No `[1]` launch was sent, because that would replace the active runtime and the reported case was not the currently observed mode.
- Build/runtime evidence and human same-mode acceptance: NOT RUN. A real repeat-launch in mode 15 with the expected build and configuration is still required to verify the user's exact report.
