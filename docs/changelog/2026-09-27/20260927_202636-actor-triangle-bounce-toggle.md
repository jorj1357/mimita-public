# Actor triangle bounce toggle

- Date: 2026-09-27
- Scope: authoritative actor triangle collision response
- Status: implemented and self-tested

## Changes

- Reconnected the actor-triangle solver to the existing global `bounce.enabled` collision setting.
- When `bounce.enabled` is `true`, every actor body-part and weapon triangle contact uses the shared bounce response and updates the authoritative root player velocity.
- When `bounce.enabled` is `false`, the same contacts only remove inward velocity and do not launch the player outward.
- Kept the existing bounce strength, friction, speed limits, minimum push, cooldown, and per-contact deduplication as the single response policy.
- Updated the actor solver self-test to validate the configured behavior.

## Validation

- `mimita-20260927T202500.exe --actor-triangle-solve-selftest`: PASS
- `mimita-20260927T202500.exe --moving-crate-selftest`: PASS
- Unique executable build/link: PASS

## Runtime setting

Use `config/collision.json`:

```json
"bounce": {
    "enabled": true
}
```

The collision configuration is hot-reloadable. Set it to `false` to return to no-bounce projection for all actor triangle contacts.
