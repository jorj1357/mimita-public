# issue-9129efc0dda7: Allow dashboard client launch without an active server

## Human report (verbatim, immutable)

> can u edit dev loop gui to make it so client can launch even without an active server


## Triage

### 2026-11-27T23:18:27.823Z — codex-devloop-gui-client-standalone

Summary: Permit managed standalone client launch from dashboard while preserving server-backed defaults.
Owner: codex-devloop-gui-client-standalone
Priority: medium

## Review (pass)

### 2026-11-27T23:22:11.147Z — codex-devloop-gui-client-standalone

Reviewer: codex-devloop-gui-client-standalone

Source review: start-client now auto-targets server-01 only when healthy; otherwise it omits --room/--connect and starts a managed standalone client from the latest published artifact. The dashboard process-panel and checkout-row controls both request this fallback. Standalone clients retain per-instance journal/process lifecycle and report stale rather than frozen when no server snapshots are expected. Runtime launch has not been exercised; keep behavior proof unverified and issue open pending real GUI control/journal evidence.
