# Melee Action ID Use After Erase

Time created: 2026-10-18T16:45:01Z
Time last updated: 2026-10-18T16:45:01Z

Status: ATTEMPTED FIX (1)

Related specifications:

- `docs/specs/gamemodes/supersmashbrosmelee.md`
- `docs/specs/debug-logging/debug-logging.md`

Related changelog:
`docs/changelog/2026-10-18/20261018_164501-melee-action-id.md`

## Regression occurrence 1

### Observed

At `2026-10-18T16:34:40Z`, dedicated server PID 27324 exited while running Melee
in run `20261018_163359`. Its crash report was
`%LOCALAPPDATA%/MiMITA/crashes/crash-2026-10-18_12-34-40-27324.txt`.
The report's resolved stack enters `StructuredLogger::writeEvent` from
`MimitaNet::tickServerMeleeActions` while serializing JSON. The server journal
ends at tick 2307 during active Melee play.

### Expected behavior

Completing a move emits its end event and the server continues its fixed-tick
loop.

### Actual behavior

The server terminated with an uncaught MinGW C++ exception while the logger
serialized the move-end event.

### Confirmed cause

`tickServerMeleeActions` takes `action` from
`g_meleeActionSnapshots[attacker.id]` when the action snapshot exists
(lines 601-603). On completion, the code erased that map entry and then read
`action->id` to emit the end event (previous lines 618-622). Erasing the entry
destroyed the `MeleeAction`, leaving `action` dangling. Reading its `std::string`
was undefined behavior. The crash stack's JSON string-escaping frame matches
the logger encountering the corrupted event string. The report did not retain
the thrown exception message, so its exact exception type is unknown.

### Wrong code

```cpp
attacker.meleeActionActive = false;
g_meleeActionSnapshots.erase(attacker.id);
emitMeleeActionEvent(attacker, "melee.action.end",
                     "melee action completed", tick, action->id);
```

### Corrected code

```cpp
const std::string completedActionId = action->id;
attacker.meleeActionActive = false;
g_meleeActionSnapshots.erase(attacker.id);
emitMeleeActionEvent(attacker, "melee.action.end",
                     "melee action completed", tick, completedActionId);
```

### Fix and proof

The action ID is copied while the action is alive, before erasing the snapshot;
the logger receives the independent copy. Debug build 2587 compiled the changed
translation unit and was published successfully. Its `--versioninfo` identity
check passed. A new live Melee run through multiple move completions has not yet
been performed, so this remains an attempted fix pending runtime/human
confirmation.
