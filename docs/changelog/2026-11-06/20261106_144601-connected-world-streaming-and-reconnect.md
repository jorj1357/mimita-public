# Connected World streaming and reconnect recovery

## Changes

- Added the `connected_world` joinable gamemode and online mode, a world-generation profile, and dev-loop launch mode 52 using `funworld3`'s editor-authored map data.
- Changed authored chunk streaming to keep servicing the desired neighborhood every frame, install at most two chunks per update, and prioritize the nearest missing cells. Added bounded queue/update and slow-install diagnostics. Previously, returning early when the player stayed in one cell could strand neighborhoods larger than the per-update installation cap.
- Fixed reconnect health transitions so a reconnect acknowledgement does not count as recovered gameplay state. The client stays reconnecting until a fresh authoritative snapshot arrives. The reconnect accept handler keeps transport sends available while waiting for that snapshot, without setting the health state to Connected.

## Validation and evidence

- Dev-loop build passed: build ID `2026-11-06T14:36:49.078685Z:c3e9840542e16a256d7dc42c13f0d5e878f56a4f563b9c65853c6e35d127af06`.
- Dev-loop run `20261106_143703` used that build and connected two clients to `connected_world`. Under bad-connection preset 10, both clients entered Reconnecting after snapshot age exceeded 2.5 seconds while other packets were arriving. Reconnect accepts no longer caused rapid Connected/Reconnecting flapping.
- The reconnect acceptance run did not recover: the server continued accepting reconnect requests, but clients received no fresh authoritative snapshots and reached ReconnectFailed at the 60-second grace deadline. The server journal recorded healthy 60 Hz ticks. The reconnect loop is suppressed, but recovery remains open and needs server/client snapshot-delivery tracing.
- Earlier run `20261106_141646` showed the client drained a nine-cell authored-chunk neighborhood at two installations per update, and the local avatar identity resolved to `abusiveboy`. Its stream records still showed `whole_world=false` and `spacing_mode=center`; they do not prove that the new mode profile was applied. No visual avatar review or two-client remote-avatar review was completed.
- `python devscripts/gamemode_json_sync.py --check` passed. Repository-wide `git diff --check` still reports pre-existing whitespace in `config/aimbody.json` and `docs/specs/open-gameplay-issues.md`.

## Remaining work

- Connected World currently previews client-side chunks; canonical server chunk identities, server-authoritative edits, and generated-chunk server collision are not integrated.
- The mode profile was not active in the runtime evidence. Streaming still performed synchronous render-thread installation; the two-chunk budget limits bursts but does not guarantee a frame-time ceiling.
- Editor save/reload, connected multi-chunk authoring, fast-travel stress, multiplayer avatar visibility, reconnect after fresh state resumes, and human visual/gameplay review remain unverified.
