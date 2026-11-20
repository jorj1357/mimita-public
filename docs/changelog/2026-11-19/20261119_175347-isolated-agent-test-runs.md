# Isolated agent test runs

- Date: 2026-11-19T17:53:47Z
- Owner: Codex
- Status: IMPLEMENTED; RUNTIME NOT TESTED
- Request: Let agents launch separate gameplay runs with their own journals
  while preserving the active map-editor run and the root build authority.

## Changes

- Added `devscripts/dev_loop/parallel_test_runs.py` to pin a validated,
  published executable and launch an isolated loopback server/client pair.
  Each run records the build and gameplay DLL identities, process start times,
  command queue, run manifest, and per-process journals. Commands can be
  queued to a specific test-run client and stop is scoped to verified PIDs.
- Added `--start-test-run`, `--list-test-runs`, `--run-id`, and
  `--stop-test-run` entry points to `devscripts/dev_loop/cli.py` and routed them
  through the canonical `devscripts/dev-loop.py` CLI.
- Updated the root process reconciler to preserve only active parallel-test
  clients whose PID, process start time, and executable match the run manifest.
- Updated `AGENTS.md`, the build-authority and multi-instance specs, and the
  multi-client/runtime workflows to allow non-overlapping file ownership and
  document isolated runtime runs under one shared build authority.

## Evidence

- `python -m py_compile devscripts/dev_loop/parallel_test_runs.py
  devscripts/dev_loop/cli.py devscripts/dev-loop.py`: passed.
- `git diff --check` on edited files: passed.
- `python devscripts/dev-loop.py --start-test-run --test-profile
  tdm-validation --test-launch-mode 41`: safely refused before launching
  processes because the published artifact reports
  `source_changed_during_build=true`. No gameplay run or melee hit evidence was
  produced. The active primary run was left untouched.

## Limitations

The controller launches a shared published EXE into an independent process
pair; it does not build a separate EXE for each agent. Runtime proof of the
launcher, melee registration, and server/client event agreement remains
pending a clean current artifact. Human visual/feel acceptance is also pending.
