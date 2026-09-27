# Fix dedicated-server startup crash from commented gamemode JSON

Time: 2026-09-27 12:24:36 EST  
Result: PASS_WITH_HUMAN_REVIEW

## Outcome

The dedicated server no longer crashes before the room-code handshake because
`config/gamemodes/retrograd.json` is now strict JSON. Its leading `//` notes
were removed; the gamemode data itself was unchanged.

## Investigation

Build 32 repeatedly exited with `541541187` (`0x20474343`) after loading
`config/onlinemodes.json` and `config/weaponsets.json`. Isolated startup probes
showed that removing `retrograd.json` allowed the server to continue, and
removing only its comment header also allowed it to continue. The corrected
server reached UDP transport, ICE gathering, room registration, and wrote room
code `8THPU7N` during a five-second runtime probe.

This was independent of the client-only projectile rifle trail and the
Counter-Strike config slice. The server scans every gamemode at startup, so an
invalid JSON file in an otherwise unused mode still blocked sandbox startup.

## Files

- `config/gamemodes/retrograd.json`: removed seven `//` comment lines before
  the JSON object.
- `docs/regressions/2026-09-27/dev-loop-server-crash-retry-and-rainbow-REG.md`:
  recorded the confirmed startup cause and runtime proof.

## Validation

- PowerShell JSON parse: passed.
- Existing `.dev/builds/0032/mimita.exe` runtime probe: passed; survived five
  seconds and registered an ICE room.
- No source rebuild was needed because this was a runtime configuration-only
  correction.

## Human review still needed

Restart the existing dev-loop Python process so it loads the current one-shot
fallback guard, then press `1` or wait for the automatic launch and confirm the
room-code client joins visibly. The previous repeated fallback messages came
from the already-running loop process, not from the corrected JSON.
