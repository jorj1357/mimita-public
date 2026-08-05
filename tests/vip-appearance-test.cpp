// 08 05 2026, 00 00
/* purpose
* Tests compact VIP appearance defaults and byte sanitization.
* Covers the packet-safe tier, style, color, and flag helpers used by client and server.
* Keeps VIP rendering authority regressions catchable without launching the game.
* DOES NOT contact the website, Stripe, database, or multiplayer server.
* DOES NOT render UI or load badge image assets.
* DOES NOT test full JSON preset parsing.
*/

#include "vip/vip-appearance.h"

#include <cassert>
#include <cstdint>
#include <cstring>

int main()
{
    MimitaVip::VipAppearance vip =
        MimitaVip::tierDefaultAppearance(MimitaVip::VIP_TIER_VIP);
    assert(vip.active());
    assert(vip.tier == MimitaVip::VIP_TIER_VIP);
    assert(vip.styleKind == MimitaVip::VIP_STYLE_TURQUOISE);
    assert(vip.colorR == 64);
    assert(vip.colorG == 224);
    assert(vip.colorB == 208);

    MimitaVip::VipAppearance invalid =
        MimitaVip::appearanceFromBytes(99, 99, 1, 2, 3, 255);
    assert(!invalid.active());
    assert(invalid.tier == MimitaVip::VIP_TIER_FREE);
    assert(invalid.styleKind == MimitaVip::VIP_STYLE_NONE);

    MimitaVip::VipAppearance staff =
        MimitaVip::appearanceFromBytes(
            MimitaVip::VIP_TIER_ULTRA_VIP,
            MimitaVip::VIP_STYLE_SOLID,
            25, 25, 25,
            MimitaVip::VIP_APPEARANCE_ACTIVE |
            MimitaVip::VIP_APPEARANCE_STAFF_OVERRIDE);
    assert(staff.active());
    assert(staff.staffOverride());
    assert(std::strcmp(MimitaVip::tierToString(staff.tier), "ultra_vip") == 0);

    uint8_t tier = 0;
    uint8_t style = 0;
    uint8_t r = 0;
    uint8_t g = 0;
    uint8_t b = 0;
    uint8_t flags = 0;
    MimitaVip::copyAppearanceToBytes(staff, tier, style, r, g, b, flags);
    assert(tier == MimitaVip::VIP_TIER_ULTRA_VIP);
    assert(style == MimitaVip::VIP_STYLE_SOLID);
    assert(r == 25 && g == 25 && b == 25);
    assert((flags & MimitaVip::VIP_APPEARANCE_STAFF_OVERRIDE) != 0);

    return 0;
}
