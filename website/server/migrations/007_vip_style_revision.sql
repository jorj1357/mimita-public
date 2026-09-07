-- Add the VIP style revision column for databases created before the style-revision bootstrap update.
ALTER TABLE vip_name_styles
    ADD COLUMN IF NOT EXISTS style_revision INT NOT NULL DEFAULT 1;
