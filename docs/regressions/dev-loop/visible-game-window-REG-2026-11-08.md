# Dev-loop visible launch did not bring the game forward

Recorded: `2026-11-08`  
Status: **PARTIALLY FIXED — launch bookkeeping and restore behavior changed; foreground activation and taskbar presence remain unverified**

## Purpose and scope

This record covers the Windows game window launched by the dev loop when its
visibility policy is `visible`. It explains why the dev loop could report a
visible window while the user could not find or bring the game forward. It
does not define the separate dev-loop dashboard window.

## Expected behavior

With the dev-loop visibility policy set to `visible`, launching the game should
leave a normal, restorable game window available to the user. Window visibility,
foreground activation, and taskbar presence are separate facts and must be
recorded separately; `IsWindowVisible` alone does not prove that the window is
foreground or that it has a taskbar button.

## Observed occurrences

### Run `20261108_140418`

- The dev-loop journal reported client PID `14100`, HWND `6296346`, and
  `visible: true`.
- The same snapshot reported `foreground: false`; another application owned
  the foreground. The user reported that MiMITA did not appear in the taskbar.
- The run later ended. No visual inspection of the taskbar was available, so
  the missing taskbar button is recorded as the user's observation rather than
  independently verified Windows evidence.

### Run `20261108_140449`

- Launching with the already-running server took a different branch from
  starting a new server.
- The client did not reach `Connected` before the launch timeout.
- The dev-loop then raised `UnboundLocalError` at
  `restore_prelaunch_foreground(prelaunch_windows)`: that variable had only
  been assigned in the branch that created a new server.
- The failure was recorded in
  `logs/2026-11-08/20261108_140449/dev-loop/events.jsonl` at
  `2026-11-08T14:05:35.270Z`.

### Run `20261108_141222`

- After the launcher changes, the dev loop completed a Payload / `funworld3`
  launch with matching server and client build identities.
- At `client_ready`, the journal reported client PID `27400` and HWND
  `18746304` as visible, but not foreground.
- The `dev-loop.launch.visibility` record showed the foreground still belonged
  to Firefox immediately after the launcher's activation request. Later window
  monitoring showed ChatGPT in the foreground.
- This confirms that the dev loop requested foreground activation but does
  not prove Windows accepted it. The taskbar button was not visually verified.

Runtime evidence for this occurrence is in:

- `logs/2026-11-08/20261108_141222/dev-loop/events.jsonl`
- `logs/2026-11-08/20261108_141222/client-01/events.jsonl`
- `logs/2026-11-08/20261108_141222/server-01/events.jsonl`

## Confirmed causes

Two launcher defects contributed to the experience:

1. `_set_process_windows_visible()` used Windows `SW_SHOWNA` (show without
   activation). Then `launch_latest()` called `restore_prelaunch_foreground()`,
   deliberately returning focus to the application that had been active before
   launch. The policy name `visible` therefore meant “the window is visible”;
   it did not mean “the game is foreground.” This behavior matches the
   `visible: true` and `foreground: false` journal evidence.
2. `prelaunch_windows` was initialized only when the dev loop started a new
   server. If it reused a server, the later focus-restoration call referenced
   an uninitialized local and aborted launch completion.

The Windows foreground activation request can be denied when the launcher is a
background process. The journal proves the foreground stayed with another
application; it does not establish the exact Windows policy decision or explain
whether the taskbar button itself was absent.

## Changes made

In `devscripts/dev-loop.py`:

- Moved the pre-launch window snapshot to the shared path before the server
  reuse/start branches, so both branches initialize it.
- Changed the visible-window command from `SW_SHOWNA` to `SW_RESTORE`, which
  restores a minimized window.
- For `visible` launches, requests `SetForegroundWindow()` and records the
  foreground window observed afterward.
- Skips restoring the pre-launch foreground when the policy is `visible`.
  Headless launches retain the prior foreground-restoration behavior.

## What works now, and what remains open

The updated Python dev-loop passed `python -m py_compile`. The daemon was
restarted through the watcher, and run `20261108_141222` completed its launch
with server and client on build
`2026-11-08T14:12:08.114903Z` / SHA-256
`2f650a9378a562c9b722aaa7aabeae449244125a6bb36cd75590fe0f65b6f3c5`.
The launch journal confirms that the window was reported visible and that the
foreground request was attempted.

The game did not become foreground in the recorded evidence. Taskbar presence
was not visually checked, and the computer-control bridge was unavailable.
Therefore this is a partial fix, not a confirmed resolution of “no taskbar
icon.” A follow-up must check the actual taskbar and whether the window is
minimized while the run is live. If the user starts the game through the
dashboard, the dashboard's foreground-thread context may also be needed for
Windows to permit foreground activation; that remains an implementation
hypothesis, not a proven cause.

## Related record

- Changelog: `docs/changelog/2026-11-08/20261108_141528-visible-devloop-window.md`
- Owning implementation: `devscripts/dev-loop.py` lines 350, 2049, 2083,
  3224, and 3732.
