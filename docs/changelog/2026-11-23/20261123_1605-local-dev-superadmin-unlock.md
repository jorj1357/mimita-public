# Local development superadmin unlock

## Request

Add `adminadmin <password>` so a developer can run server admin commands on the existing dev-loop session, use it to unblock the bounded CPU profiler, keep the development credential in config, and leave a note for future non-centralized identity-based authorization.

## Implementation

- Registered the client terminal command and routed it through the existing server-command packet.
- The server honors the unlock only when its process role is `server`, root dev-loop run metadata is present, the actual listening socket is loopback, and `config/dev-superadmin.local.json` is enabled with scope `dev_loop_localhost`.
- A correct password sets the existing per-player, current-session admin grant. Wrong, missing, or extra arguments return `wrong password` without changing permissions. This grants the normal registered admin command set; it does not execute arbitrary shell commands.
- Server command-result events and terminal history/evidence redact the supplied value. `.gitignore` excludes the local config. The tracked example is disabled and documents future verification by developer identity/capability without requiring a centralized account database.
- Local-only config was created with the user-requested development password. It is ignored and is not part of the tracked source or build artifact.

## Build evidence

Root dev-loop Mode 3 attempt `2026-11-23T15:57:37.033Z-f2552bcf` failed after 108.815s because `debug-commands.cpp` omitted the required command usage field. The retained batch was corrected and retried as `2026-11-23T16:00:36.240Z-177dbcd1`; it published game generation 563 in 70.578s. The timing artifact reports 4 compiled units (3 changed translation units plus generated identity provider), 641 skipped, 34.857s changed-source preflight, 1.705s object-cache scan, 1.677s main compile phase, 21.351s link, 0.192s coherence validation, and 0.597s publish. These values do not meet the 10s objective; linking alone exceeded it. There was no matched build-time CPU sample.

Server PID 27384 subsequently emitted `game.hotness.summary` with generation 563 and 19 NPCs. The first correct-password request was rejected because the server launcher did not set the client-only `MIMITA_DEV_LOOP_SERVER` marker. The root dev-loop server process instead has `MIMITA_PROCESS_ROLE=server` and nonempty `MIMITA_RUN_METADATA`; the guard now uses those existing server-side markers along with the loopback socket check. This correction is pending a coordinated Mode 3 batch with the NPC agent's active source edits.

The root dev-loop restarted only client-01 (PID 20424 → PID 29644), preserving server PID 27384. The server then reported one connected player. A non-host negative test at 16:14:59.839Z confirmed `adminadmin wrong` returned `wrong password` with `is_host=false`, `admin_enabled=false`, and `admin_granted=false`. A correct-password attempt before the marker correction also returned wrong password. The client journal remains stale, so these observations come from the writable server journal. The resource agent was notified; no profiler command has been retried yet.

## Verification still required

After the guard correction publishes, repeat correct-password acceptance on the connected non-host session, run `npc_cpu_probe on 15`, verify server profiler windows, and use `admincmds 0` to revoke access. Ensure no password appears in journals. Client callsite proof is separate and requires a fresh client event sink. Proof task: `task-proof-3ce06469cfdd`.
