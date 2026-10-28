# Replace remote branch with local history

- Time: 2026-10-28T02:12:22Z
- Branch: `20261011zt`.
- Request: user explicitly confirmed replacing GitHub's branch with the local branch, including removal of remote-only commits.

Fetched `origin/20261011zt` before pushing. At fetch time, local `HEAD` was `4f40075c` with seven local-only commits; remote `origin/20261011zt` was `ad3f757e` with one remote-only documentation commit. The worktree was clean after restoring config changes produced by the dev-loop/game session. The push uses `--force-with-lease` pinned to `ad3f757e`, so a newer remote update will cause it to fail instead of being overwritten.

No application source, configuration, or gameplay behavior changed in this task. Validation is Git commit and remote-ref verification only.
