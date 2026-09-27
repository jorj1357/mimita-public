# Dev-loop server crash status

Improved the development loop failure path when a published server exits
before writing its room code.

The loop now:

- records `server_failed` in `.dev/state.json`;
- preserves the exact child exit code and last message;
- prints the failure clearly; and
- redraws the controls so `[1]` remains available for a deliberate retry.

The observed `3221225477` (`0xC0000005`) is a game-server access violation,
not a compiler failure. Its game-side owner remains to be diagnosed from a
server crash trace or dump.
