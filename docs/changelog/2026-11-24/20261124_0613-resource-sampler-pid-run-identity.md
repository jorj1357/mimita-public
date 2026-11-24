# Resource sampler PID and run identity fix

The resource sampler previously accepted a PID-to-run manifest entry when per-instance process-start fields were missing. Reused PIDs could therefore inherit a stale run role and journal.

The sampler now uses an instance launch timestamp, or a run creation timestamp as a fallback, and accepts the fallback only when the process started within 120 seconds after it. It compares executable basenames when both sides provide them, skips adopted root wrappers whose runtime run ID differs from the wrapper manifest, prefers the closest run-created time for duplicate candidates, and records the identity match basis and deltas.

Verification: `python -m unittest devscripts.quality.test_build_resource_sampler` passed 10 tests. Offline replay against the retained cold no-op build sidecar and current manifests mapped PID32184 to server run20261124_053435 with a fresh journal; PID33892 was left unassigned because there is no current manifest/process evidence for that client. This does not prove a current connected pair, runtime performance, or DLL latency. No build or game process was started or changed.
