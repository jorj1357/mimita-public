09-07-2026, 19 50 EST

Branch: 8292026stash
Commit at review: 4e0a1c3
Purpose: Make the VIP purchase UI present prepaid, monthly subscription, and lifetime as three distinct modes per tier.

Pre-existing worktree changes: replay source/config edits and the existing untracked replay changelog were not touched. The VIP API/config/lifetime implementation was already present in the current HEAD commit; this task changed only the VIP page styling/layout and the VIP configuration documentation.

Changed files and behavior:
- website/src/pages/Vip.jsx: replaced the shared slider plus generic purchase-button list with one prepaid box per tier, an integer 1-12 month range/number control, a separate monthly subscription button, and a separate lifetime button. Each tier now stores its selected month count independently.
- website/src/styles/vip.css: added the prepaid box, month controls, savings display, purchase-mode buttons, and lifetime styling.
- website/VIP_CONFIGURATION.md: documented the lifetime price environment keys and the 1-12 prepaid/lifetime model.

Reasoning: the prior page rendered all purchase types through one generic button map and used one global month state, so the slider was visually outside the purchase mode and changing one tier changed every tier. The new client sends prepaid months explicitly and leaves monthly/lifetime checkout types separate. The server remains the authority and rejects non-integer or out-of-range prepaid months.

Documents read: AGENTS.md, docs/ROUTER.md, docs/specs/vip/vip.md, docs/regressions/regressions-v1.md, docs/skills/spec-behavior-review-v1.md.

Validation:
- npm run build: PASS.
- node --test server/vip-config.test.js server/vip-entitlements.test.js: PASS in the combined focused run.
- node --test server/vip-payments.test.js server/vip-entitlements.test.js server/vip-config.test.js: 36 passed, 2 pre-existing subscription fixture failures involving expired/date-sensitive test state; no new failure was introduced by the UI change.
- git diff --check -- website: PASS, with only line-ending conversion warnings.

Deployment and human review remaining: no VPS files or services were changed. The SSH-tunnel development script still uses the VPS API, so end-to-end prepaid/lifetime acceptance requires deploying the reviewed API commit after branch/commit confirmation, then testing Stripe test mode, webhook delivery, database entitlement/email records, refunds, and subscription management.
