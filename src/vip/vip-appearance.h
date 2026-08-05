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

enum VipStyleAnimationKind : uint8_t
{
    VIP_ANIMATION_NONE = 0,
    VIP_ANIMATION_CYCLE = 1,
    VIP_ANIMATION_PULSE = 2
};

enum VipStyleDirection : uint8_t
{
    VIP_DIRECTION_LTR = 0,
    VIP_DIRECTION_RTL = 1
};

constexpr int VIP_STYLE_MAX_COLORS = 32;

// Full client-rendered name style. The dedicated server relays this from the
// verified website style. Rendering is always client-local and tick-driven;
// no animation state crosses the network.
struct VipStyleDetail
{
    uint8_t styleKind = VIP_STYLE_NONE;
    uint8_t animation = VIP_ANIMATION_NONE;
    uint8_t direction = VIP_DIRECTION_LTR;
    float rainbowSpeed = 1.0f;
    glm::vec4 solidColor{0.62f, 0.62f, 0.62f, 1.0f};
    std::vector<glm::vec4> colors;
    uint32_t styleEpoch = 0;

    bool valid() const { return styleKind != VIP_STYLE_NONE; }
    size_t colorCount() const { return colors.empty() ? 0 : colors.size(); }
};

inline bool animationAdvances(uint8_t styleKind)
{
    return styleKind == VIP_STYLE_ANIMATED_RAINBOW ||
           styleKind == VIP_STYLE_COLOR_CYCLE;
}

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

// Builds a full render style from the compact packet-safe wire fields.
// colorRgb points to colorCount RGB triples; empty/zero colorCount means the
// detail carries no per-player color data (free or no style).
inline VipStyleDetail styleDetailFromWire(uint8_t styleKind,
                                          uint8_t animation,
                                          uint8_t direction,
                                          float rainbowSpeed,
                                          const uint8_t* colorRgb,
                                          uint8_t colorCount,
                                          uint32_t styleEpoch)
{
    VipStyleDetail out;
    out.styleKind = sanitizeStyleKindByte(styleKind);
    if (out.styleKind == VIP_STYLE_NONE)
        return out;
    out.animation = (animation <= VIP_ANIMATION_PULSE) ? animation : VIP_ANIMATION_NONE;
    out.direction = (direction <= VIP_DIRECTION_RTL) ? direction : VIP_DIRECTION_LTR;
    out.rainbowSpeed = std::clamp(rainbowSpeed, 0.25f, 4.0f);
    out.styleEpoch = styleEpoch;
    if (colorRgb && colorCount > 0)
    {
        const uint8_t count = std::min<uint8_t>(colorCount, (uint8_t)VIP_STYLE_MAX_COLORS);
        out.colors.reserve(count);
        for (uint8_t i = 0; i < count; ++i)
            out.colors.push_back(colorFromBytes(colorRgb[i * 3], colorRgb[i * 3 + 1], colorRgb[i * 3 + 2]));
    }
    if (!out.colors.empty())
        out.solidColor = out.colors[0];
    return out;
}

// Copies the full style into the compact packet-safe wire fields.
// colorRgb must point to at least VIP_STYLE_MAX_COLORS * 3 bytes.
inline uint8_t copyStyleDetailToWire(const VipStyleDetail& detail,
                                     uint8_t& styleKind,
                                     uint8_t& animation,
                                     uint8_t& direction,
                                     float& rainbowSpeed,
                                     uint8_t* colorRgb)
{
    styleKind = detail.styleKind;
    animation = detail.animation;
    direction = detail.direction;
    rainbowSpeed = detail.rainbowSpeed;
    const size_t count = std::min<size_t>(detail.colors.size(), VIP_STYLE_MAX_COLORS);
    for (size_t i = 0; i < count; ++i)
    {
        const glm::vec4& c = detail.colors[i];
        colorRgb[i * 3] = (uint8_t)std::clamp((int)std::lround(c.r * 255.0f), 0, 255);
        colorRgb[i * 3 + 1] = (uint8_t)std::clamp((int)std::lround(c.g * 255.0f), 0, 255);
        colorRgb[i * 3 + 2] = (uint8_t)std::clamp((int)std::lround(c.b * 255.0f), 0, 255);
    }
    return (uint8_t)count;
}

} // namespace MimitaVip
