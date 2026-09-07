2026-09-07T16:45:00Z

Branch: 8292026stash
Local HEAD: bc3987ae0320d9bc8bee81260e801f7d6b42d91a
VPS HEAD: 6c2b08218571b54e8a3f834e8c7722d684893a01
Purpose: Compare local and VPS website VIP implementation and configuration for lifetime-price and prepaid-slider behavior.

Pre-existing changes: local replay/config/documentation changes and untracked documentation files were preserved. This inspection did not modify source code, VPS files, Stripe settings, or environment values.

Comparison result:
- The VPS is behind the local repository's latest overall commit, but the website VIP implementation files inspected are present on the deployed commit and match the local website implementation relevant to this task.
- Local `website/.env` contains lifetime Price ID key names and values.
- VPS `/root/mimita-site/website/.env` contains `STRIPE_SECRET_KEY` and `STRIPE_VIP_WEBHOOK_SECRET`, but no `MIMITA_STRIPE_PRICE_*` keys were present, including the three lifetime keys.
- No secret values were printed or recorded.

Lifetime finding: `publicVipConfig()` marks lifetime configured only when its tier-specific `MIMITA_STRIPE_PRICE_*_LIFETIME` key exists. The missing VPS keys therefore make the lifetime buttons disabled even though the Stripe account has lifetime Prices.

Slider finding: `Vip.jsx` sends the selected integer `months`; `vip-payments.js` calls `prepaidPurchaseDefinition(tier, months)`, records the server amount, and uses inline Stripe `price_data` when no fixed Price ID exists. A 7-month checkout charging the fixed 12-month amount still requires a captured request body, server checkout log, and Stripe session line-item comparison; this session did not change that path.

Required next action: add the three lifetime Price ID keys to the VPS `.env` using IDs from the same Stripe mode as the VPS secret key, restart `mimita-api` with updated environment, and verify `/api/vip/config` reports lifetime options configured. Then capture one prepaid checkout at a non-12-month value to prove the Stripe amount equals the server-calculated amount.
