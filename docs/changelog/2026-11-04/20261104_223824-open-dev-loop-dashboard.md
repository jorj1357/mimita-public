# Open dev-loop dashboard

- Local time: 2026-11-04 22:38:24 EST
- UTC: 2026-11-05T03:38:24Z
- Branch: `20261011zt`
- HEAD at start: `bb1df89d5a6743205def70507e5e3d911191943e`
- Result: `PASS_WITH_HUMAN_REVIEW` for the launcher; repository gate remains red
  for unrelated existing checks.

## Scope

Added one Python entry point whose sole job is to start the visible native
dev-loop dashboard. It starts the canonical `devscripts/dev-loop.py` with
`--dashboard --no-terminal-relaunch`, using `pythonw.exe` on Windows so the
dashboard appears without leaving a console window. It does not build, launch,
or change game processes directly.

## Source change

Added `devscripts/open-dev-loop-dashboard.py` (lines 1–53). There was no prior
launcher file, so the old content was absent. The complete new content is:

```python
"""Open the visible Tk dashboard attached to the existing dev loop."""

from __future__ import annotations

import os
import shutil
import subprocess
import sys
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
DASHBOARD_SCRIPT = ROOT / "devscripts" / "dev-loop.py"


def _windowless_python() -> str:
    if os.name != "nt":
        return sys.executable

    sibling = Path(sys.executable).with_name("pythonw.exe")
    if sibling.is_file():
        return str(sibling)
    installed = shutil.which("pythonw.exe")
    if installed:
        return installed
    raise FileNotFoundError("Could not find pythonw.exe to open the dashboard without a console")


def main() -> int:
    creationflags = 0
    if os.name == "nt":
        creationflags = (subprocess.DETACHED_PROCESS
                         | subprocess.CREATE_NEW_PROCESS_GROUP)

    subprocess.Popen(
        [
            _windowless_python(),
            str(DASHBOARD_SCRIPT),
            "--dashboard",
            "--no-terminal-relaunch",
        ],
        cwd=ROOT,
        stdin=subprocess.DEVNULL,
        stdout=subprocess.DEVNULL,
        stderr=subprocess.DEVNULL,
        close_fds=True,
        creationflags=creationflags,
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
```

The existing dev-loop daemon, its current game run, and all pre-existing work
were left in place.

## Documents and review

- Read `AGENTS.md`, `docs/ROUTER.md`,
  `docs/specs/dev-loop/dev-loop.md`,
  `docs/workflows/multi-client-dev-testing.md`,
  `docs/architecture/time-and-formatting/time-and-formatting.md`,
  `docs/operations/task-completion/task-completion.md`, and
  `docs/skills/documentation-checker-v1.md`.
- The dev-loop specification says the desktop entry should open or attach one
  visible native dashboard while keeping the canonical dev-loop as the owner
  of daemon and game state. The new file follows that path.
- The documentation skill was reviewed; no documentation or specification
  changes were needed.

## Validation and evidence

- `python -m py_compile devscripts/open-dev-loop-dashboard.py`: passed.
- Ran `python devscripts/open-dev-loop-dashboard.py`; Windows opened a visible
  `MiMITA Dev Loop Dashboard — mimita-v9` window under `pythonw.exe`. The
  computer-use window inventory confirmed it. The older manually opened
  dashboard was closed; the launcher-opened dashboard remained visible.
- `python devscripts/quality/agent-gate.py`: completed with `RED` (exit code 1).
  The final gate summary reported four failures: the lint baseline
  (`damage-authority` has 82 findings against a baseline of 81), and the
  `grenade_tick_contract_v1`, `npc_wall_containment_v1`, and
  `scenario_record_roundtrip_v1` runtime scenarios. Their first wait steps did
  not observe the expected live events. The gate's JSON sync checks and
  scenario-roundtrip check passed. No gate baseline or unrelated source was
  changed to hide these findings.
- No visual or gameplay acceptance is claimed for the game; this task only
  opens the dashboard.

## Pre-existing and concurrent work

At the start, `git status` already showed modified avatar images and metadata,
`config/analytics.json`, `config/audio/music-settings.json`, the
`devscripts/blender-ai` submodule, and `docs/specs/gamemodes/vehicle-arena.md`.
More modified documentation files and
`docs/changelog/2026-11-05/20261105_033145-rocket-league-reference-baseline.md`
appeared during the gate run. They were left untouched and are not attributed
to this launcher task. The working tree was not clean, so no commit was made.

## Build and deployment

- Standalone game build: not applicable; no C++ or game source changed.
- Deployment: not performed.
