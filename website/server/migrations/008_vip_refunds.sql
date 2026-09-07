ALTER TABLE vip_orders
    ADD COLUMN IF NOT EXISTS refund_status TEXT NOT NULL DEFAULT '',
    ADD COLUMN IF NOT EXISTS stripe_refund_id TEXT NOT NULL DEFAULT '',
    ADD COLUMN IF NOT EXISTS refund_requested_at TIMESTAMPTZ,
    ADD COLUMN IF NOT EXISTS refunded_at TIMESTAMPTZ,
    ADD COLUMN IF NOT EXISTS refund_error TEXT NOT NULL DEFAULT '',
    ADD COLUMN IF NOT EXISTS refund_email_status TEXT NOT NULL DEFAULT 'not_attempted',
    ADD COLUMN IF NOT EXISTS refund_email_sent_at TIMESTAMPTZ,
    ADD COLUMN IF NOT EXISTS refund_email_error TEXT NOT NULL DEFAULT '';

ALTER TABLE vip_orders DROP CONSTRAINT IF EXISTS vip_orders_refund_status_check;
ALTER TABLE vip_orders ADD CONSTRAINT vip_orders_refund_status_check
    CHECK (refund_status IN ('','requested','succeeded','failed'));
