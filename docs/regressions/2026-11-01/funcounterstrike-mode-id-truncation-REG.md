# Fun Counter Strike mode id truncated in DuelStatePacket

Time created: 2026-11-01T01:54:00Z
Time last updated: 2026-11-01T01:54:00Z

Status: ATTEMPTED FIX (1)

Related specification:
`docs/specs/gamemodes/funcounterstrike.md`,
`docs/specs/gamemodes/objective-hud.md`

Related changelog:
`docs/changelog/2026-11-01/20261101_015253-fun-counterstrike-hud-deathball-damage.md`

---

## Regression Occurrence 1

### Observed

Time: `2026-11-01T01:09:54Z` (runtime journal `20261101_010944`)

The Fun Counter Strike client received the gamemode id as
`"funcounterstrik"` and rendered no objective/round HUD.

### Expected Behavior

The client receives the full authoring id `"funcounterstrike"`, selects the
`funcounterstrike` HUD section from `config/gui/gamemode-meta-gui.json`, and
renders phase, score, round, timer, objective state, team hint, prompt, result,
and countdown elements.

### Actual Behavior

`DuelStatePacket::matchMode` was `char[16]`. `"funcounterstrike"` is 16
characters, so the server's `strncpy(..., sizeof - 1)` produced
`"funcounterstrik"`. The client then looked up a nonexistent layout section and
drew nothing.

### Why This Is Bad

The HUD contract is not satisfied. The same truncation also changes client-side
mode identity for other systems that key off the replicated mode id.

### Specification

`docs/specs/gamemodes/funcounterstrike.md` requires the dedicated
`funcounterstrike` layout and its phase, score, round, timer, objective, hint,
prompt, and result elements.

`docs/specs/gamemodes/objective-hud.md` requires the HUD to answer phase, time,
objective state, action prompt, and round result.

### Wrong Code

File:

`src/network/packets.h`

```cpp
char matchMode[16] = {};           // "duel", "ffa", "tdm"
```

File:

`src/network/server-gamemode.cpp`

```cpp
std::strncpy(pkt.matchMode, d.matchMode.c_str(), sizeof(pkt.matchMode) - 1);
```

### Confirmed Cause

The fixed-size replicated field was one byte too small for the 16-character
mode id plus its null terminator. Evidence:
`logs/2026-11-01/20261101_010944/events.jsonl` `network.mode.start` client event
contained `"mode":"funcounterstrik"`.

### Attempted Fix 1

Time: `2026-11-01T01:50:57Z`

Change:

- Increased `matchMode` to 32 bytes and bumped `PROTOCOL_VERSION` 46 -> 47.
- Added a rate-limited `network.mode.truncated` error if a future id exceeds the
  field.
- Added a one-per-load `hud.layout.loaded` GUI event with the mode, section,
  load result, element count, and element ids.

### Corrected Code

File:

`src/network/packets.h`

```cpp
constexpr uint16_t PROTOCOL_VERSION = 47;

char matchMode[32] = {};
```

File:

`src/network/server-gamemode.cpp`

```cpp
if (d.matchMode.size() >= sizeof(pkt.matchMode)) {
    static std::string lastTruncatedMode;
    if (lastTruncatedMode != d.matchMode) {
        lastTruncatedMode = d.matchMode;
        StructuredLogger::instance().writeEvent(
            StructuredCategory::Network, StructuredLevel::Errors,
            "network.mode.truncated", d.matchMode,
            "replicated gamemode id exceeds DuelStatePacket::matchMode",
            0,
            {{"mode", d.matchMode},
             {"mode_bytes", d.matchMode.size()},
             {"field_bytes", sizeof(pkt.matchMode)}});
    }
}
std::strncpy(pkt.matchMode, d.matchMode.c_str(), sizeof(pkt.matchMode) - 1);
```

### Fix

The packet now carries the full mode id, and the client selects the correct HUD
section. The diagnostic makes any future oversized id visible immediately.

### Proof

Runtime journal `20261101_015151`, build 13:

- client `network.mode.start` reported `"mode":"funcounterstrike"`;
- client `hud.layout.loaded` reported `section:"funcounterstrike"`,
  `loaded:true`, `element_count:12`, including `phaseText`, `scoreText`,
  `roundText`, `matchTime`, `objectiveStatus`, `objectiveHint`,
  `objectivePrompt`, `objectiveResultText`, `roundOverText`, and
  `countdownText`;
- round start/countdown/go/active events were present and the active round
  reported `round_end_tick:6480`.

The same source built as build 15 (`.dev/builds/0015/mimita.exe`) and passed
`--canonical-contact-selftest`, `--objective-selftest`, and
`--map-config-selftest`.

### Human Review Still Required

Visual confirmation that the HUD is readable, does not overlap other required
elements, and shows the 90-second timer and objective switching correctly on the
live client.
