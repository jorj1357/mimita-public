---
name: dev-loop-restart
description: Use when the MiMITA dev-loop daemon, watchdog, dashboard, game client/server, test runtime, or wake adapter is stale, wedged, or needs a restart after source changes.
---

# Dev-loop restart

Read `docs/skills/dev-loop-restart-v1.md` and
`docs/specs/dev-loop/dev-loop.md#194a-ai-restart-authority-for-the-dev-loop-always-allowed`.

You may restart checkout-owned dev-loop processes immediately. Restart only
processes owned by this checkout. Preserve unrelated processes and a healthy
matching game pair unless the task requires relaunching it.

Use the control path:

```powershell
$env:MIMITA_AGENT_ID = "<agent>"
python devscripts/dev-loop.py --request stop
python devscripts/dev-loop.py --ensure-running
python devscripts/dev-loop.py --status
```

Verify exactly one watchdog and one daemon, the combined script hash matches,
the control endpoint answers, and the game pair is preserved or intentionally
relaunched. Never finish with the dev loop dead. Record old/new PID, source
hash, status, journal path, and whether the game pair was preserved.
