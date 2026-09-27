# Live JSONL probes

Hot C++ code may expose values through a named probe:

```cpp
LiveProbeScope probe("collision.resolve");
probe.field("contact_count", contacts.size());
```

The probe sends structured data to the existing `StructuredLogger`; it does
not open a file itself. The logger writes the process `events.jsonl` stream.

Probe rules are reloadable through `config/debuglogger.json` while the game is
running. Adding a new probe call requires a hot DLL generation. Changing its
enabled state, sampling rule, or threshold requires no restart or DLL build.

Probe records may contain JSON numbers, booleans, strings, IDs, positions,
velocities, durations, revisions, and code generations. Every live-collaboration
record should include local and remote generation provenance when available.
