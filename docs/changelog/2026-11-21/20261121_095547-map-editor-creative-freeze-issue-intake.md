# Map editor creative-action frame-freeze issue intake

- Recorded user report as `issue-ba939787ffa3`, preserving the full wording in the append-only issue record and shared user-context record.
- The report covers intermittent map reload, save, and copy/paste stalls; Payload route copy/paste was reported to freeze for about 0.5 seconds. The user wants the existing 240 FPS target applied to creative workflows.
- Linked the focused editor issue to `issue-549680e53b26` (Frame consistency: no frame below 240 FPS).
- Runtime verification was not performed. The issue records the user report as unverified and calls for pinned editor runtime measurements and owner-level timing scopes.
