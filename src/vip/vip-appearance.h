// 08 03 2026, 17 20
/* purpose
* Defines the compact, server-authoritative VIP appearance data shared by game systems.
* Converts tiers, style kinds, and RGB colors into safe rendering defaults.
* Keeps offline, missing-ticket, and invalid-ticket players on the free gray appearance.
* DOES NOT grant entitlements, verify Stripe payments, or trust locally claimed VIP.
* DOES NOT store full Ultra preset/color arrays in movement snapshots.
* DOES NOT own website API transport or database state.
*/

#pragma once

#include <algorithm>
#include <cstdint>
#include <string>

#include <glm/glm.hpp>

namespace MimitaVip {

enum VipTier : uint8_t
{
    VIP_TIER_FREE = 0,
    VIP_TIER_VIP = 1,
    VIP_TIER_SUPER_VIP = 2,
    VIP_TIER_ULTRA_VIP = 3
};

enum VipStyleKind : uint8_t
{
    VIP_STYLE_NONE = 0,
    VIP_STYLE_TURQUOISE = 1,
    VIP_STYLE_RAINBOW = 2,
    VIP_STYLE_SOLID = 3,
    VIP_STYLE_ANIMATED_RAINBOW = 4,
    VIP_STYLE_PER_LETTER = 5,
    VIP_STYLE_COLOR_CYCLE = 6
};

enum VipAppearanceFlags : uint8_t
{
    VIP_APPEARANCE_ACTIVE = 1 << 0,
    VIP_APPEARANCE_STAFF_OVERRIDE = 1 << 1
};

struct VipAppearance
{
    uint8_t tier = VIP_TIER_FREE;
    uint8_t styleKind = VIP_STYLE_NONE;
    uint8_t colorR = 158;
    uint8_t colorG = 158;
    uint8_t colorB = 158;
    uint8_t flags = 0;

    bool active() const { return (flags & VIP_APPEARANCE_ACTIVE) != 0; }
    bool staffOverride() const { return (flags & VIP_APPEARANCE_STAFF_OVERRIDE) != 0; }
};

inline uint8_t tierFromString(const std::string& tier)
{
    if (tier == "vip") return VIP_TIER_VIP;
    if (tier == "super_vip") return VIP_TIER_SUPER_VIP;
    if (tier == "ultra_vip") return VIP_TIER_ULTRA_VIP;
    return VIP_TIER_FREE;
}

inline const char* tierToString(uint8_t tier)
{
    switch (tier)
    {
    case VIP_TIER_VIP: return "vip";
    case VIP_TIER_SUPER_VIP: return "super_vip";
    case VIP_TIER_ULTRA_VIP: return "ultra_vip";
    default: return "free";
    }
}

inline uint8_t styleKindFromString(const std::string& kind)
{
    if (kind == "vip_turquoise") return VIP_STYLE_TURQUOISE;
    if (kind == "rainbow") return VIP_STYLE_RAINBOW;
    if (kind == "solid") return VIP_STYLE_SOLID;
    if (kind == "animated_rainbow") return VIP_STYLE_ANIMATED_RAINBOW;
    if (kind == "per_letter") return VIP_STYLE_PER_LETTER;
    if (kind == "color_cycle") return VIP_STYLE_COLOR_CYCLE;
    return VIP_STYLE_NONE;
}

inline glm::vec4 colorFromBytes(uint8_t r, uint8_t g, uint8_t b, float alpha = 1.0f)
{
    return {
        (float)r / 255.0f,
        (float)g / 255.0f,
        (float)b / 255.0f,
        std::clamp(alpha, 0.0f, 1.0f)
    };
}

inline uint8_t sanitizeTierByte(uint8_t tier)
{
    return tier <= VIP_TIER_ULTRA_VIP ? tier : VIP_TIER_FREE;
}

inline uint8_t sanitizeStyleKindByte(uint8_t styleKind)
{
    return styleKind <= VIP_STYLE_COLOR_CYCLE ? styleKind : VIP_STYLE_NONE;
}

inline VipAppearance freeAppearance();

inline VipAppearance appearanceFromBytes(uint8_t tier,
                                         uint8_t styleKind,
                                         uint8_t r,
                                         uint8_t g,
                                         uint8_t b,
                                         uint8_t flags)
{
    VipAppearance out;
    out.tier = sanitizeTierByte(tier);
    out.styleKind = sanitizeStyleKindByte(styleKind);
    out.colorR = r;
    out.colorG = g;
    out.colorB = b;
    out.flags = flags & (VIP_APPEARANCE_ACTIVE | VIP_APPEARANCE_STAFF_OVERRIDE);
    if (out.tier == VIP_TIER_FREE)
        out.flags &= ~VIP_APPEARANCE_ACTIVE;
    if (!out.active() && !out.staffOverride())
        return freeAppearance();
    return out;
}

inline void copyAppearanceToBytes(const VipAppearance& source,
                                  uint8_t& tier,
                                  uint8_t& styleKind,
                                  uint8_t& r,
                                  uint8_t& g,
                                  uint8_t& b,
                                  uint8_t& flags)
{
    VipAppearance safe = appearanceFromBytes(
        source.tier, source.styleKind, source.colorR, source.colorG, source.colorB,
        source.flags);
    tier = safe.tier;
    styleKind = safe.styleKind;
    r = safe.colorR;
    g = safe.colorG;
    b = safe.colorB;
    flags = safe.flags;
}

inline VipAppearance freeAppearance()
{
    return {};
}

inline VipAppearance tierDefaultAppearance(uint8_t tier)
{
    VipAppearance out;
    out.tier = tier;
    if (tier == VIP_TIER_FREE)
        return out;

    out.flags = VIP_APPEARANCE_ACTIVE;
    if (tier == VIP_TIER_VIP)
    {
        out.styleKind = VIP_STYLE_TURQUOISE;
        out.colorR = 64;
        out.colorG = 224;
        out.colorB = 208;
    }
    else if (tier == VIP_TIER_SUPER_VIP)
    {
        out.styleKind = VIP_STYLE_RAINBOW;
        out.colorR = 255;
        out.colorG = 204;
        out.colorB = 0;
    }
    else
    {
        out.styleKind = VIP_STYLE_ANIMATED_RAINBOW;
        out.colorR = 153;
        out.colorG = 68;
        out.colorB = 255;
    }
    return out;
}

inline glm::vec4 nameColor(const VipAppearance& appearance, float alpha = 1.0f)
{
    if (!appearance.active() && !appearance.staffOverride())
        return colorFromBytes(158, 158, 158, alpha);
    return colorFromBytes(appearance.colorR, appearance.colorG, appearance.colorB, alpha);
}

inline const char* badgeLabel(uint8_t tier)
{
    switch (tier)
    {
    case VIP_TIER_VIP: return "VIP";
    case VIP_TIER_SUPER_VIP: return "SVIP";
    case VIP_TIER_ULTRA_VIP: return "UVIP";
    default: return "";
    }
}

} // namespace MimitaVip
