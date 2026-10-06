# Runtime Scenario Validation

## Purpose

Use the real MiMITA executable and the canonical `events.jsonl` journal to
prove behavior. This workflow prevents a passing synthetic self-test from
being mistaken for working gameplay.

## Required loop

For every behavior change or investigation:

1. Read the specification, architecture owner, relevant changelog,
   regression record, and prior attempts.
2. Trace intent/configuration, action/input, fixed-tick simulation, networking
   where applicable, presentation, and logging. Find the first divergence.
3. Add bounded diagnostics at the existing owner using `StructuredLogger`.
   Records should include the scenario/action, tick, expected state, actual
   state, difference, and reason. Do not add a second file logger.
4. Build the newest timestamped executable.
5. Run:

   ```text
   mimita-<timestamp>.exe --versioninfo
   ```

   Save the printed `EVENTS_JSONL_PATH`. It is the authoritative journal for
   that process and must be named in the evidence report.
6. Run the actual game or a bounded runtime scenario using the same command,
   input, simulation, network, and presentation owners as normal play.
7. Inspect the journal live and after completion. Confirm that the changed
   path was reached and locate the first expected/actual mismatch.
8. Adjust only the responsible owner, rebuild, rerun, and compare the same
   scenario and evidence.

## Scenario shape

When behavior needs deterministic timing, use an explicit seed, initial
state, and fixed-tick action schedule. For example:

```text
ticks 1-5: move forward
ticks 6-19: no movement input
ticks 20-25: move right
ticks 26-40: no movement input
```

The scenario must use the real fixed 60 Hz path and record input acceptance,
state before, state after, and final verdict. It must be bounded and exit with
an actionable process status.

## Test policy

Do not create a new synthetic test as the first response to a gameplay bug.
Use existing unit/component tests only for behavior they genuinely isolate,
such as serialization, path construction, or a pure mathematical invariant.
Classify those tests explicitly. Prefer a runtime scenario for gameplay,
networking, configuration loading, animation, collision, audio, and visual
behavior. A runtime log proves that the path executed; human visual or feel
acceptance remains a separate claim.

## Evidence report

Always separate:

- source/ownership evidence;
- build and executable identity;
- exact `EVENTS_JSONL_PATH`;
- runtime scenario inputs and exit status;
- JSONL records and first divergence or success condition; and
- human visual/multiplayer acceptance, if required.
