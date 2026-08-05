// 08 03 2026, 17 20
/* purpose
* Declares the short-lived on-screen killfeed model and manager.
* Stores killer/victim labels with compact VIP appearance metadata.
* Exposes simple killfeed entry points for live gameplay and replay playback.
* DOES NOT own damage calculation, death authority, or replay file serialization.
* DOES NOT verify VIP entitlements or trust client-provided account data.
* DOES NOT render chat, nameplates, or menu UI.
*/

#pragma once

#include <string>
#include <vector>

#include <glm/glm.hpp>

#include "vip/vip-appearance.h"

struct ReplayKillfeedEvent;

struct KillfeedEntry {
    std::string killerName;
    std::string victimName;
    std::string weaponName;
    MimitaVip::VipAppearance killerVipAppearance;
    MimitaVip::VipAppearance victimVipAppearance;
    MimitaVip::VipStyleDetail killerVipStyleDetail;
    MimitaVip::VipStyleDetail victimVipStyleDetail;
    float age = 0.0f;
    float opacity = 0.0f;
};

class KillfeedManager {
public:
    static KillfeedManager& instance();

    void onKill(const std::string& killerName,
                const std::string& victimName,
                const std::string& weaponName,
                bool fromReplay = false);
    void onKillStyled(const std::string& killerName,
                      const MimitaVip::VipAppearance& killerVipAppearance,
                      const std::string& victimName,
                      const MimitaVip::VipAppearance& victimVipAppearance,
                      const std::string& weaponName,
                      bool fromReplay = false);
    void onKillStyled(const std::string& killerName,
                      const MimitaVip::VipAppearance& killerVipAppearance,
                      const MimitaVip::VipStyleDetail& killerVipStyleDetail,
                      const std::string& victimName,
                      const MimitaVip::VipAppearance& victimVipAppearance,
                      const MimitaVip::VipStyleDetail& victimVipStyleDetail,
                      const std::string& weaponName,
                      bool fromReplay = false);

    void update(float dt);
    void render();

    void clear();

private:
    static constexpr int MAX_ENTRIES = 5;
    static constexpr float STILL_OPACITY = 0.50f;
    static constexpr float STILL_DURATION = 3.0f;
    static constexpr float FADE_DURATION = 1.0f;
    static constexpr float TOTAL_DURATION = STILL_DURATION + FADE_DURATION;
    static constexpr float ENTRY_HEIGHT = 20.0f;
    static constexpr float ENTRY_X_OFFSET = 10.0f;
    static constexpr float ENTRY_Y_OFFSET = 60.0f;
    static constexpr float ENTRY_FONT_SCALE = 0.30f;

    std::vector<KillfeedEntry> mEntries;
};
