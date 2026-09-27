# Development Loop Status Menu Redrew Forever While Idle

Time created: 2026-09-27T01:16:26Z
Time last updated: 2026-09-27T01:16:26Z

Status: ATTEMPTED FIX (1)

Related specification:
`docs/specs/performance/performance.md`

Relevant requirement: repeated background work must be bounded and must not
continuously produce duplicate output when the observable state has not
changed.

Related changelog:
`docs/changelog/2026-09-26/20260926_210526-development-loop-ccache.md`

---

## Regression Occurrence 1

### Observed

Time:
`2026-09-26 local development session; recorded 2026-09-27T01:16:26Z`

The terminal repeatedly printed the same four-line menu while idle:

```text
RUNNING: (none)
LATEST:  2
AUTO-RESTART: OFF
[1] Switch to newest  [2] Stay on current  [A] Toggle auto-restart  [Q] Quit
```

### Expected Behavior

The menu should print once when its state changes, such as after a build,
launch, or auto-restart toggle. An idle poll must not print the identical menu
again.

### Actual Behavior

The main loop called `print_status()` every `0.20` seconds whenever a latest
build existed. This produced an unbounded duplicate stream even though the
status tuple was unchanged.

### Why This Is Bad

It floods the terminal, hides build errors and room-code output, and makes the
development loop look like it is repeatedly doing work when it is only idle.

### Wrong Code

File:
`devscripts/dev-loop.py`

```python
if self.change_event.wait(0.20):
    self.build_pending = True
if self.latest_build is not None:
    self.print_status()
```

### Confirmed Cause

The idle branch invoked the renderer on every poll. The existing tuple guard
was not sufficient protection for the observed running version, so the idle
call itself was removed rather than relying on repeated polling.

### Attempted Fix 1

Time:
`2026-09-27T01:16:26Z`

Change:

- Removed the idle-loop call to `print_status()`.
- Kept status printing at meaningful state transitions: successful publish,
  launch, manual switch, and auto-restart changes.

Result:

A controlled startup printed the status menu once after build publication.
Human long-running idle review is still pending.

### Corrected Code

File:
`devscripts/dev-loop.py`

```python
if self.change_event.wait(0.20):
    self.build_pending = True
```

Status is now printed by the transition handlers rather than by this idle
poll.

### Proof

- `python -m py_compile devscripts/dev-loop.py`: passed.
- Controlled startup from `C:\mimita-v9` showed one status block for the
  unchanged state.

Human review is still required for a longer idle run.

### Solution

Not confirmed yet.
