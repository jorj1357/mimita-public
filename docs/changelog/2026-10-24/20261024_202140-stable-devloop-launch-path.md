# Stable DevLoop launch path

- Recorded: `2026-10-24T20:21:40Z` (display: `2026-10-24 16:21:40 EDT`, `America/New_York`)
- Branch: `20261011zt`
- HEAD before and after: `0472121baef8d96342508d82aef5598de97daff7`
- Result: `PASS_WITH_RUNTIME_EVIDENCE`

## Request and source trace

Keep version numbers on every published game build while giving Windows one
permanent executable path for Firewall's app-specific rule. Starting a new
server after closing the previous one must keep server and client build
identities aligned.

Before this change, DevLoop launched `.dev/builds/<number>/mimita.exe` for both
processes. The existing server marker could therefore point at build 3094 while
the selected artifact was build 3097. The launch path comparison correctly
rejected that server, but each numbered path also appeared to Windows as a
different app. DevLoop now launches the selected immutable artifact through
`.dev/current/mimita.exe`, stores its build ID with the server marker, and
checks that ID before reusing a same-path server.

## Exact source changes

### `devscripts/dev-loop.py`

Before, server and client process arguments used the numbered artifact path:

```python
server_args = self.build_server_args(exe, map_name, self.room_file_path)
client_args = [
    str(exe), "--room", room_code,
```

Now, the selected artifact is staged as a hardlinked bundle at the stable path,
then both processes use that path while metadata retains the numbered artifact:

```python
launch_exe = STABLE_LAUNCH_EXE
server_args = self.build_server_args(launch_exe, map_name, self.room_file_path)
client_args = [str(launch_exe)]
```

The server-owner marker now retains build ID, build identity, local gameplay
port, and room code. A restarted daemon can adopt a matching server, and a
server from a different numbered build is replaced even when the executable
path is the same. DevLoop status also shows the current room code and labels a
stable-path server with its numbered build.

### Documentation

There was no Windows stable-path contract in the DevLoop specification. Added:

> For Windows launches, DevLoop keeps that numbered artifact immutable and starts its server and client through the fixed path `.dev/current/mimita.exe`.

The build-and-EXE operation guide now documents the same launch path and the
separate versioned artifact identity.

## Build and runtime evidence

- `python -m py_compile devscripts/dev-loop.py`: passed.
- `git diff --check`: passed; only existing line-ending warnings were reported.
- Canonical DevLoop build request completed successfully and published build
  3096. After the watchdog reloaded the changed DevLoop script, its current
  source build published 3097. Both build logs reported `Nothing changed.`;
  the C++ executable hash stayed `28c63f57...211d35`. The numbered artifacts
  remain separately identifiable by build number.
- DevLoop started build 3097 in run `20261024_201650` through
  `C:\mimita-v9\.dev\current\mimita.exe`. The server and client became ready,
  build IDs matched, and room code `Z3RKLQA` was published.
- Repeated the stop-server then launch sequence. In run
  `20261024_201919`, the server and client again used the stable path and
  matched build ID
  `3097:28c63f572facf4aeedc18de99b8ee1b82358c32a1e5360c5795d913bff211d35`.
  DevLoop reported `builds_match=true`; the journal contained no identity- or
  build-mismatch text. It recorded local gameplay readiness at
  `20:19:24.128Z`, launched the client at `20:19:24.182Z`, and recorded the
  client logger at `20:19:24.283Z`. The coordinator room code `CYLMKDL` arrived
  later at `20:19:29.378Z` and the client HUD source updated at `20:19:32.882Z`.
- Windows process inspection confirmed both live process image paths were the
  stable executable. DevLoop daemon PID 26592 and watchdog PID 10944 were
  alive; the daemon's loaded script hash matched the on-disk script.
- Created and verified persistent inbound UDP allow rule
  `MiMITA Dev Server UDP stable launch`, scoped to
  `C:\mimita-v9\.dev\current\mimita.exe` (Profile Any). The live run reported
  `FIREWALL_CLEAR`.

No visual screen review was performed. The second runtime used launch mode 12
(`zombietower4`); this validates the stable executable and restart identity
behavior, not the separate mode-4 sandbox configuration.

## Other worktree changes preserved

The following existing worktree changes were preserved and were not part of
this change: `config/accounts/default.json`, `config/analytics.json`,
`config/audio/music-settings.json`, `config/hitfx.json`,
`config/playervisuals.json`, `devscripts/dev-launch-modes.json`,
`docs/architecture/collision/collision.md`,
`docs/specs/open-gameplay-issues.md`, and existing changes under `src/engine/`,
`src/gui/`, `src/input/`, `src/network/`, and `src/physics/movement/`.
Previously untracked changelogs were also left untouched.
