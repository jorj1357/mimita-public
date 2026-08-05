// 08 03 2026, 17 20
/* purpose
* Declares compact UI helpers for drawing server-authoritative VIP names.
* Shares badge/name measurement and drawing across HUD, chat, menus, and overlays.
* Keeps VIP visuals bounded to the packet-safe appearance summary used by the game.
* DOES NOT verify entitlements, read website APIs, or grant VIP locally.
* DOES NOT load Stripe, database, or full preset JSON configuration.
* DOES NOT own player/network state or mutate gameplay identity.
*/

#pragma once

#include <string>

#include "vip/vip-appearance.h"

struct VipNameDrawOptions
{
    float scale = 0.28f;
    float alpha = 1.0f;
    float phase = 0.0f;
    bool drawBadge = true;
    // Optional full style carried from the server. When present and valid it
    // renders the exact user-chosen colors (solid/rainbow/per-letter). When
    // null the compact appearance falls back to the fixed palette. Animation
    // is always client-local and tick-driven via `phase`.
    const MimitaVip::VipStyleDetail* detail = nullptr;
};

float vipMeasureStyledName(const std::string& name,
                           const MimitaVip::VipAppearance& appearance,
                           const VipNameDrawOptions& options = {});

void vipDrawStyledName(const std::string& name,
                       const MimitaVip::VipAppearance& appearance,
                       float x,
                       float y,
                       const VipNameDrawOptions& options = {});

void vipDrawStyledNameCentered(const std::string& name,
                               const MimitaVip::VipAppearance& appearance,
                               float centerX,
                               float y,
                               const VipNameDrawOptions& options = {});
