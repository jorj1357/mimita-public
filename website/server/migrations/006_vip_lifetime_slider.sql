ALTER TABLE vip_orders
    ADD COLUMN IF NOT EXISTS purchase_months INT,
    ADD COLUMN IF NOT EXISTS is_lifetime BOOLEAN NOT NULL DEFAULT FALSE,
    ADD COLUMN IF NOT EXISTS confirmation_email TEXT NOT NULL DEFAULT '',
    ADD COLUMN IF NOT EXISTS confirmation_email_status TEXT NOT NULL DEFAULT 'not_attempted',
    ADD COLUMN IF NOT EXISTS confirmation_email_sent_at TIMESTAMPTZ,
    ADD COLUMN IF NOT EXISTS confirmation_email_error TEXT NOT NULL DEFAULT '';

ALTER TABLE vip_orders DROP CONSTRAINT IF EXISTS vip_orders_purchase_type_check;
ALTER TABLE vip_orders ADD CONSTRAINT vip_orders_purchase_type_check
    CHECK (purchase_type IN ('one_month','monthly_subscription','twelve_month','prepaid','lifetime'));

ALTER TABLE vip_entitlements
    ADD COLUMN IF NOT EXISTS is_lifetime BOOLEAN NOT NULL DEFAULT FALSE;

ALTER TABLE vip_entitlements ALTER COLUMN expires_at DROP NOT NULL;
ALTER TABLE vip_entitlements DROP CONSTRAINT IF EXISTS vip_entitlements_expires_at_starts_at_check;
ALTER TABLE vip_entitlements DROP CONSTRAINT IF EXISTS vip_entitlements_expires_at_check;
ALTER TABLE vip_entitlements DROP CONSTRAINT IF EXISTS vip_entitlements_lifetime_or_expiry_check;
ALTER TABLE vip_entitlements ADD CONSTRAINT vip_entitlements_lifetime_or_expiry_check
    CHECK (is_lifetime OR (expires_at IS NOT NULL AND expires_at > starts_at));
