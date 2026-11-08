# Visible dev-loop game window

- Fixed launch bookkeeping so the pre-launch window snapshot exists when the dev-loop reuses its already-running server.
- Changed the explicit `visible` policy to restore the game window and request foreground activation. The launch journal now records the foreground window observed immediately afterward.
- Restarted the dev-loop through its watcher and launched Payload on `funworld3` with the published build `2026-11-08T14:12:08.114903Z` (SHA-256 `2f650a9378a562c9b722aaa7aabeae449244125a6bb36cd75590fe0f65b6f3c5`).
- Validation: `python -m py_compile devscripts/dev-loop.py` succeeded; dev-loop launch completed with server/client build identities matching. The window was reported visible, but Windows kept Firefox as the foreground window after the activation request. Taskbar-icon visibility was not confirmed, so this change does not yet establish that the game appears on the taskbar.
- Runtime journals: `logs/2026-11-08/20261108_141222/dev-loop/events.jsonl`, `logs/2026-11-08/20261108_141222/client-01/events.jsonl`, and `logs/2026-11-08/20261108_141222/server-01/events.jsonl`.
