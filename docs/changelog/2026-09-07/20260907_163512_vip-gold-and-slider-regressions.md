2026-09-07T16:35:12Z

Branch: 8292026stash
HEAD at inspection: 6c2b08218571b54e8a3f834e8c7722d684893a01
Purpose: Document the Full access VPS recovery as gold behavior and record the unresolved VIP slider/lifetime configuration regressions.

Pre-existing changes: unrelated repository changes and earlier VIP deployment documentation were preserved. This session made documentation-only changes; no application source, Stripe data, VPS files, environment values, or database values were changed.

Files added:
- `docs/gold/2026-09-07-full-access-vps-deployment.md`: records `2026-09-07T17:30:26Z`, Windows Codex desktop, GPT-5.6, Full access mode, the exact deployment commit, and the exact proof changelog path `C:\mimita-priv-v8\docs\changelog\2026-09-07\20260907_162530_vip-style-revision-migration.md`.
- `docs/regressions/regressions-v1.md`: appended the Full access recovery behavior, the unresolved prepaid slider/Stripe amount mismatch, and the unresolved lifetime buttons “Stripe is not configured” issue.

Technical findings documented:
- Prepaid checkout is intended to use server-calculated inline Stripe `price_data.unit_amount` for the selected month count. A 7-month selection showing the fixed 12-month amount requires request/session evidence before fixing.
- Lifetime button configuration is controlled by the API’s environment presence for `MIMITA_STRIPE_PRICE_VIP_LIFETIME`, `MIMITA_STRIPE_PRICE_SUPER_VIP_LIFETIME`, and `MIMITA_STRIPE_PRICE_ULTRA_VIP_LIFETIME`; local `.env` values do not automatically exist on the VPS.

Validation: documentation search and source-path inspection completed. No runtime fix was claimed. Human review remains required for the exact 7-month checkout request/Stripe session and VPS lifetime environment-key presence.
