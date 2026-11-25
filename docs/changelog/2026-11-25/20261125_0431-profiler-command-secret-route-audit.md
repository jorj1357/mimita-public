# Profiler command route: credential persistence blocker

## Goal context

The active goal requires function-level CPU attribution, followed by one evidence-selected CPU treatment per CPU-goal DLL build and matched running-executable proof. This pass did not start the probe because the existing authorization route would persist its password.

## Source evidence

- `devscripts/dev_loop/parallel_test_runs.py:392-400` writes the raw command string into an append-only client command queue.
- `devscripts/dev_loop/parallel_test_runs.py:1024-1028` writes that same raw command into the `dev-loop.command.request` event payload.
- The C++ server/terminal handlers redact `adminadmin` from their own logs, but that does not redact these Python queue/event fields.
- Sending `adminadmin <password>` through `dev-loop.py --run-id ... --command` would therefore persist the credential in local developer evidence. No password or profiler command was sent in this pass.

## Effect on the CPU experiment

The target Juggernaut run remains host DLL generation 635 with NPC module generation 632, and recent available-memory snapshots were below the 2 GiB build gate. The probe also needs the other test-run owners to release the two Bloodtest pairs for a clean matched window. These blockers are recorded in `issue-ed3ac775283d` and cross-referenced from `issue-136d4553ff92`.

The current blocker is the command transport, not a reason to abandon function attribution. Before the next probe, the coordination owner should provide a secret-safe unlock route that does not persist credential text in the queue or event payload. A no-argument dev-only unlock that resolves the local secret inside the client is one possible design; a safe queue protocol is another. Keep that measurement-access change separate from the eventual CPU-treatment build.

## Session outcome

No source code, configuration, or runtime state was changed. No build, profiler command, or secret-bearing command was run. The finding was appended to the existing CPU and DLL-latency issue discussions. The CPU goal remains active and incomplete.