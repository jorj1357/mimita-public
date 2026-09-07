09-07-2026, 20 05 EST

Branch: 8292026stash
Commit: 4e0a1c3
Purpose: Record the unresolved mismatch between the VIP source file and the page shown through the SSH-tunnel development script.

Pre-existing changes: unrelated replay/config work and the prior VIP purchase-mode changelog were preserved. No application code, VPS files, services, or secrets were changed in this investigation.

Evidence checked:
- website/npm-run-dev-ssh-v2.bat starts an SSH tunnel for port 3002 and separately starts local Vite with `npm run dev`.
- website/src/pages/Vip.jsx contains the prepaid range input and purchase-mode markup.
- The prepaid markup is conditional on the `/api/vip/config` response containing a `prepaid` purchase.
- Recent VIP changelog entries describe the frontend/API split and the need to rebuild deployed frontend output.
- docs/regressions/regressions-v1.md now has an appended unresolved VIP runtime mismatch entry.

Result: unresolved. The browser screenshot shows only monthly buttons even though the repository source contains the slider. The next diagnostic must compare the browser's `/api/vip/config` response, the Vite-served frontend source/bundle, the process owning ports 5173/3002, and the VPS API/deployed commit. Do not mark the regression fixed until the slider is visibly present in the browser.

Validation: repository searches completed; no build or deployment was performed because this session recorded the unresolved issue rather than changing behavior.
