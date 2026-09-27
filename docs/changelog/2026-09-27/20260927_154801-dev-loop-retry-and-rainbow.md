# Dev-loop retry, fallback, and rainbow terminal output

Improved the development loop after build 26 linked successfully but its
server process exited before room-code publication.

Changes:

- The loop now notices when no tracked MiMITA process remains, regardless of
  the legacy AUTO-RESTART toggle.
- The latest server is retried up to three times with a short delay.
- If the server continues to crash, one fallback MiMITA client is launched so
  the session does not remain at zero processes. That fallback launch is
  latched, so an immediately crashing fallback cannot produce an endless
  launch-message loop.
- AUTO-RESTART is now permanently ON for the development loop; the old toggle
  no longer permits a stopped session.
- Dev-loop and build-child output uses a moving full-rainbow color cycle over
  five seconds when attached to a real terminal. The live status block is
  redrawn during the cycle so already-visible status text changes color too.

The build-26 game-side exception (`0x20474343`) is documented separately and
is not being hidden by the fallback behavior.

Validation:

- `python -m py_compile devscripts/dev-loop.py`: passed.
- `python devscripts/dev-loop.py --help`: passed.
- Windows Application Error evidence captured for the build-26 server crash.

Related regression:
`docs/regressions/2026-09-27/dev-loop-server-crash-retry-and-rainbow-REG.md`
