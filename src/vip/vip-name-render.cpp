// 08 03 2026, 17 20
/* purpose
* Implements packet-safe VIP name and badge drawing for the game UI.
* Renders solid, rainbow, animated, and per-letter compact styles deterministically.
* Keeps per-frame work small by drawing from the bounded appearance summary only.
* DOES NOT trust client-selected VIP tiers or fetch website profile data.
* DOES NOT render website badge image assets or load user preset JSON.
* DOES NOT own chat, nameplate, menu, or player-list layout.
*/

#include "vip/vip-name-render.h"

#include <algorithm>
#include <cmath>

#include "gui/ui-system.h"

namespace {

glm::vec4 withAlpha(glm::vec4 color, float alpha)
{
    color.a *= std::clamp(alpha, 0.0f, 1.0f);
    return color;
}

glm::vec4 rainbowColor(int index, float phase, float alpha)
{
    constexpr glm::vec4 colors[] = {
        {1.00f, 0.00f, 0.27f, 1.0f},
        {1.00f, 0.80f, 0.00f, 1.0f},
        {0.00f, 1.00f, 0.40f, 1.0f},
        {0.00f, 0.80f, 1.00f, 1.0f},
        {0.60f, 0.27f, 1.00f, 1.0f}
    };
    const int count = (int)(sizeof(colors) / sizeof(colors[0]));
    const int offset = (int)std::floor(phase);
    int colorIndex = (index + offset) % count;
    if (colorIndex < 0)
        colorIndex += count;
    return withAlpha(colors[colorIndex], alpha);
}

bool shouldDrawLetters(const MimitaVip::VipAppearance& appearance)
{
    if (!appearance.active() || appearance.staffOverride())
        return false;
    return appearance.styleKind == MimitaVip::VIP_STYLE_RAINBOW ||
           appearance.styleKind == MimitaVip::VIP_STYLE_ANIMATED_RAINBOW ||
           appearance.styleKind == MimitaVip::VIP_STYLE_PER_LETTER ||
           appearance.styleKind == MimitaVip::VIP_STYLE_COLOR_CYCLE;
}

float badgeWidth(const MimitaVip::VipAppearance& appearance,
                 const VipNameDrawOptions& options)
{
    if (!options.drawBadge || appearance.tier == MimitaVip::VIP_TIER_FREE)
        return 0.0f;
    return 18.0f * (options.scale / 0.28f);
}

const char* badgeImagePath(uint8_t tier)
{
    switch (tier)
    {
    case MimitaVip::VIP_TIER_VIP: return "assets/ui/vip-badge-vip.png";
    case MimitaVip::VIP_TIER_SUPER_VIP: return "assets/ui/vip-badge-super-vip.png";
    case MimitaVip::VIP_TIER_ULTRA_VIP: return "assets/ui/vip-badge-ultra-vip.png";
    default: return "";
    }
}

void drawBadge(const MimitaVip::VipAppearance& appearance,
               float x,
               float y,
               const VipNameDrawOptions& options)
{
    const char* badge = MimitaVip::badgeLabel(appearance.tier);
    if (!options.drawBadge || !badge || !badge[0])
        return;

    const float scale = options.scale * 0.78f;
    const float w = badgeWidth(appearance, options);
    const char* imagePath = badgeImagePath(appearance.tier);
    if (imagePath && imagePath[0])
    {
        uiDrawImageFit(imagePath, {x, y - 1.0f, w, w}, false,
                       {1.0f, 1.0f, 1.0f, options.alpha});
        return;
    }

    glm::vec4 badgeColor = MimitaVip::nameColor(appearance, options.alpha);
    badgeColor.a = std::max(0.12f, badgeColor.a * 0.28f);
    uiDrawRect({x, y + 2.0f, w + 18.0f, w}, badgeColor, "vip-badge-bg");
    uiDrawRectOutline({x, y + 2.0f, w + 18.0f, w},
                      {1.0f, 1.0f, 1.0f, 0.42f * options.alpha},
                      "vip-badge-border");
    uiDrawText(badge, x + 5.0f, y + 2.0f, scale,
               {1.0f, 1.0f, 1.0f, options.alpha});
}

} // namespace

float vipMeasureStyledName(const std::string& name,
                           const MimitaVip::VipAppearance& appearance,
                           const VipNameDrawOptions& options)
{
    const float badgeW = badgeWidth(appearance, options);
    const float gap = badgeW > 0.0f ? 5.0f : 0.0f;
    return badgeW + gap + uiMeasureText(name.c_str(), options.scale);
}

void vipDrawStyledName(const std::string& name,
                       const MimitaVip::VipAppearance& appearance,
                       float x,
                       float y,
                       const VipNameDrawOptions& options)
{
    const float badgeW = badgeWidth(appearance, options);
    float cursor = x;
    if (badgeW > 0.0f)
    {
        drawBadge(appearance, cursor, y, options);
        cursor += badgeW + 5.0f;
    }

    if (!shouldDrawLetters(appearance))
    {
        uiDrawText(name.c_str(), cursor, y, options.scale,
                   MimitaVip::nameColor(appearance, options.alpha));
        return;
    }

    char glyph[2] = {};
    for (size_t i = 0; i < name.size(); ++i)
    {
        glyph[0] = name[i];
        const glm::vec4 color = rainbowColor((int)i, options.phase, options.alpha);
        uiDrawText(glyph, cursor, y, options.scale, color);
        cursor += uiMeasureText(glyph, options.scale);
    }
}

void vipDrawStyledNameCentered(const std::string& name,
                               const MimitaVip::VipAppearance& appearance,
                               float centerX,
                               float y,
                               const VipNameDrawOptions& options)
{
    const float w = vipMeasureStyledName(name, appearance, options);
    vipDrawStyledName(name, appearance, centerX - w * 0.5f, y, options);
}
