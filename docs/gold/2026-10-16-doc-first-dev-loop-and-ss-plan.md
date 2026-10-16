# Gold reference: documentation-first planning for the dev loop and `ss`

- Status: `GOLD / WORKING METHOD REFERENCE` — documentation and planning,
  not runtime acceptance
- User-supplied time marker: `2026-10-16 02:16 America/New_York`
- Repository: `C:\mimita-v9`
- Reported model context: GPT-6 Luna, Medium (user prediction; not independently
  verified by model telemetry)

## The user's prediction

The user predicts that the best next step is to talk through the documentation
and write down the design before attempting a large code fix. Turning ideas
into a clear written contract makes the work feel more understandable and more
doable. The user also wants a command reference because they do not know all
of the dev-loop commands and want to be able to look them up.

This is recorded as the user's prediction and preference, not as an objective
claim about model performance. The useful working method is to make the
specification concrete first, inspect the current owners, separate what exists
from what is only desired, and then use the written contract to scope a narrow
implementation.

## Plain-language problem statement

The dev loop is the Python helper that watches files, builds MiMITA, launches
the game when asked, and lets AI send commands into the running client. The
user wants repeated AI calls such as
`python devscripts/dev-loop.py --command "ss"` to always find a usable loop.
If it is missing, that same command should start one background copy of the
script, wait until it is ready, and then send `ss` exactly once. The user does
not want a separate supervisor application to keep open.

The `ss` issue is about what happens after leaving a room. `ss` is a game
terminal alias for `server_start_preset sandbox_1`; it is not a Python
dev-loop option. `server_leave` disconnects the player while the server may
continue running. Starting the preset again currently attempts a server
launch and can be refused because `launchServerPresetAndJoin()` sees its
external-server flag still set; it does not reconnect to that existing server.
The desired one-command behavior is: if already connected to any game as host
or client, do nothing and say so; otherwise reuse the healthy registered
sandbox server left running, or start one if needed, then join once as a client
through room code/coordinator/ICE. Leave disconnects the player only; the
server remains running and joinable. Do not stop a server whose ownership is
unclear.

The source also distinguishes room-code publication from the local join
address: the GUI launcher collects a room code, but
`launchServerPresetAndJoin()` currently puts a localhost direct address into
the pending client connection. That is still technically client/server, but
it skips the room discovery and ICE path. The requested `ss` flow must use
room code/coordinator/ICE, consistent with the user clarification in networking
section 48.4 and the room-code-bypass regression. A localhost connection still
uses client/server packets but only tests two processes on one computer; it
does not prove online reachability. Keep room-lookup failures distinct from
identity failures.

“Room identity not found” is the user's description of the message, not a
verified event string from the current investigation. A fresh journal is
needed to determine whether the room code was missing, stale, or belonged to a
different server, or whether the client was still connected.

## Where the existing guidance lives

| Document or owner | What it currently covers | Remaining gap |
|---|---|---|
| `docs/specs/dev-loop/dev-loop.md`, section 19 | Daemon lifetime, command delivery, identity checks, and the reported `ss` sequence. | The self-starting `--command` recovery behavior is specified but not implemented. |
| `docs/specs/dev-loop/dev-loop.md`, section 22 | Dev-loop CLI options and interactive keys. | Must be kept aligned with `devscripts/dev-loop.py` as options change. |
| `docs/specs/networking/networking.md`, sections 48.4, 62, 67, and 67.1 | No-localhost room-code path, disconnect behavior, `server_leave`, and the desired idempotent preset start/rejoin contract. | Runtime proof is still needed; current preset source disagrees with room-code joining and existing-server reuse. |
| `docs/architecture/terminal-commands/terminal-commands.md` | General rule that commands should use shared behavior and expose clear results. | It is not the feature-specific server-preset behavior spec. |
| `config/command-aliases.json` | Maps `ss` to `server_start_preset sandbox_1`. | Configuration proves only the expansion, not a successful room launch/join. |
| `src/terminal/network-commands.cpp` and `src/gui/gui-main.cpp` | Current preset handler and shared server launch/join owner. | The preset owner refuses an already-live external server instead of rejoining it; runtime trace must confirm the user's precise failure. |
| `src/network/multiplayer-tick.cpp` | Rejects a welcome packet with a mismatched expected run/build identity. | Preserve this guard; fix stale ownership or launch/rejoin reconciliation instead. |

No new standalone specification file is needed now. The desired preset
start/rejoin behavior fits under the existing networking server-command
specification, so it was added as section 67.1. The dev-loop spec should link
to that behavior because it defines how AI reaches it, not duplicate the full
networking contract.

## Dev-loop command reference

The current option list is maintained in section 22 of
`docs/specs/dev-loop/dev-loop.md`. The important distinction is:

- `--status` reads the last saved status and does not start the loop.
- `--command` forwards a game-terminal command to an already-running loop and
  ready client today.
- The desired future behavior is for `--command` and `--scenario-file` to
  start one missing loop, wait for readiness, then deliver the request once.
- Game-terminal commands such as `ss` are a separate command layer; they are
  sent through the dev-loop's `--command` option.
- Interactive `Q` shuts down the dev-loop daemon. `A` controls client
  auto-restart, not daemon lifetime.

The documentation must label current behavior and desired behavior separately
so a command example is not mistaken for implemented recovery.

## Safe next implementation path

1. Implement and verify single-daemon bootstrap from `--command` and
   `--scenario-file`, with PID, heartbeat, source hash, and last-exit reason.
2. Verify that repeated calls reuse one daemon and one request is delivered
   once after recovery.
3. With a fresh matching build, run `ss`, leave once, and run `ss` once more.
4. Inspect the live journal for alias expansion, disconnect completion, server
   readiness/room identity, identity validation, and client-connected state.
5. Keep game build evidence, server/client runtime evidence, and human
   acceptance separate. Do not claim the room issue is fixed from a spec edit
   or successful build.

This method favors a small, reviewable written plan before a broader code
change, while still requiring real runtime evidence for the eventual fix.


## Follow-up clarification: server-preset parity and dev-loop speed

The user clarified that `ss` and other preset-start commands should be the
one-command equivalent of the GUI's server setup buttons: apply the selected
preset settings, start a registered server, wait for its room code, and join
through the normal client/server room flow. The `ss` path must not use a
`127.0.0.1` gameplay address or a loopback-only server bind. `localhost` is
not itself a single-player game mode, but it cannot prove an online route.

The user also reports that dev-loop updates feel slow but says it works and
this is currently a low-priority frustration, not a request to rewrite the
script. Record the possible future timing investigation and the unanswered
question of splitting the Python file into modules. Measure watcher, debounce,
build, and publish stages first; retain one persistent `dev-loop.py` entry
point and one daemon.
