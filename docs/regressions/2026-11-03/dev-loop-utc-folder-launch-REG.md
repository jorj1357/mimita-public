# Dev-loop UTC artifact folder launch mismatch

**Status:** FIX IMPLEMENTED — dashboard launch and live server/client identity verified; rendered gameplay still needs human visual review.

## Failure

The dashboard reported `BUILD IDENTITY INVALID` for the newest published build, even though the artifact existed. The public artifact identity is an ISO timestamp such as `2026-11-03T14:05:27.123456Z`, while the Windows-safe directory replaces `:` with `-`. Treating the public ID as a directory name therefore selected a path that does not exist.

## Wrong and corrected behavior

The build completion path previously stored the public ID alone (`self.latest_build = str(result["artifact_id"])`) and launch resolved `BUILD_ROOT / self.latest_build / "mimita.exe"`.

The loop now keeps the identities separately:

```python
self.latest_build = str(result["artifact_id"])
self.latest_build_folder = str(result["folder_name"])
```

Executable lookup uses `latest_build_folder`; user-facing status continues to show the canonical UTC ID. Launch verifies the sidecar timestamp ID and executable SHA-256, and selects the newest valid earlier artifact if the newest one fails validation.

## Evidence and remaining acceptance

- `devscripts/dev_loop/test_artifacts.py` verifies UTC collision handling, safe folder names, ordering alongside legacy numeric folders, sidecar/SHA validation, and fallback selection.
- The live loop published `2026-11-03T14:11:32.294522Z` into `.dev/builds/2026-11-03T14-11-32.294522Z`; status reported SHA-256 `f5eaf35e57a3f82496c039a22977295c0cef65ab3bb02f122ac635b5148d58ef` and a valid identity.
- The first launch request was falsely blocked because the detector treated any title containing `Windows Security` as a firewall prompt. Its journal showed two background windows titled exactly `Windows Security` while Blender was the foreground window; no consent popup was visible to the user.
- The detector now requires a specific firewall-alert title or consent wording, and the pre-launch observation no longer blocks launch. On retry, the journal recorded `firewall_prompt_detected: false` despite those generic background windows.
- Error reporting exposed two extraction omissions during launch: stable-path activation referenced the moved `_EXECUTABLE_IDENTITY_CACHE` global, and server startup called `tempfile.mkstemp()` without importing `tempfile`. The runtime module now owns an explicit cache invalidation helper, and the entry point imports `tempfile`.
- The confirmed GUI request `dashboard-06c570e9` is recorded in `logs/2026-11-03/20261103_142636/events.jsonl`. It launched artifact `2026-11-03T14:24:42.089012Z` with SHA-256 `f5eaf35e57a3f82496c039a22977295c0cef65ab3bb02f122ac635b5148d58ef`; server PID 3596 and client PID 31572 reached readiness, the client reached `Connected`, managed mode reached `ACTIVE`, server/client build IDs matched, and the final control result was `completed`. Firewall monitoring reported `FIREWALL_CLEAR`.

See [the 2026-11-03 changelog](../../changelog/2026-11-03/20261103_141217-utc-build-dashboard-launch-organization.md) and [dev-loop specification](../../specs/dev-loop/dev-loop.md).
