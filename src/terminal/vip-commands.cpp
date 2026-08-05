// 08 05 2026, 00 00
/* purpose
* Registers terminal commands for VIP status inspection and local visual preview.
* Reports authenticated account tier, style, color, and flags from AuthSystem.
* Lets developers preview compact tier defaults on the local player only.
* DOES NOT verify payments, issue join tickets, or call Stripe.
* DOES NOT change dedicated-server authority or remote-player snapshots.
* DOES NOT persist account style changes.
*/

#include "terminal/vip-commands.h"

#include "auth/auth-system.h"
#include "debug/debug-log.h"
#include "devtools/terminal.h"
#include "terminal/terminal-state.h"
#include "vip/vip-appearance.h"

#include <string>
#include <vector>

namespace {

std::string styleName(uint8_t styleKind)
{
    switch (styleKind)
    {
    case MimitaVip::VIP_STYLE_TURQUOISE: return "turquoise";
    case MimitaVip::VIP_STYLE_RAINBOW: return "rainbow";
    case MimitaVip::VIP_STYLE_SOLID: return "solid";
    case MimitaVip::VIP_STYLE_ANIMATED_RAINBOW: return "animated_rainbow";
    case MimitaVip::VIP_STYLE_PER_LETTER: return "per_letter";
    case MimitaVip::VIP_STYLE_COLOR_CYCLE: return "color_cycle";
    default: return "none";
    }
}

void logAppearance(const char* prefix, const MimitaVip::VipAppearance& vip)
{
    Terminal& term = Terminal::instance();
    term.addLog(std::string(prefix) +
        " tier=" + MimitaVip::tierToString(vip.tier) +
        " style=" + styleName(vip.styleKind) +
        " rgb=(" + std::to_string((int)vip.colorR) + "," +
        std::to_string((int)vip.colorG) + "," +
        std::to_string((int)vip.colorB) + ")" +
        " active=" + (vip.active() ? "1" : "0") +
        " staff=" + (vip.staffOverride() ? "1" : "0"));
}

void vipStatus()
{
    const AuthUser& user = AuthSystem::instance().user();
    Terminal& term = Terminal::instance();
    term.addLog("[VIP] account=" + (user.username.empty() ? std::string("guest") : user.username));
    logAppearance("[VIP] appearance", user.vipAppearance);
    if (gpPlayer)
        logAppearance("[VIP] local-player", gpPlayer->vipAppearance);
}

} // namespace

void registerVipCommands()
{
    Terminal::instance().registerCommand({
        "vipstatus", "Show local account VIP appearance", "vipstatus",
        [](const std::vector<std::string>&) {
            vipStatus();
        },
        "08 05 2026",
        CommandCategory::Player
    });

    Terminal::instance().registerCommand({
        "viptest", "Preview a local VIP tier appearance", "viptest <off|vip|super_vip|ultra_vip>",
        [](const std::vector<std::string>& args) {
            Terminal& term = Terminal::instance();
            if (!gpPlayer)
            {
                term.addLog("[VIP] no local player is available");
                return;
            }
            if (args.empty())
            {
                term.addLog("[VIP] Usage: viptest <off|vip|super_vip|ultra_vip>");
                return;
            }

            const std::string tierArg = args[0] == "off" ? "free" : args[0];
            const uint8_t tier = MimitaVip::tierFromString(tierArg);
            gpPlayer->vipAppearance = MimitaVip::tierDefaultAppearance(tier);
            Debug::warn(Debug::Category::Vip,
                "[VIP TEST] local preview tier=%s\n",
                MimitaVip::tierToString(gpPlayer->vipAppearance.tier));
            logAppearance("[VIP] preview", gpPlayer->vipAppearance);
            term.addLog("[VIP] preview is local-only and does not grant online VIP");
        },
        "08 05 2026",
        CommandCategory::Player
    });

    Terminal::instance().registerCommand({
        "vip", "Show VIP command help and current status", "vip",
        [](const std::vector<std::string>&) {
            Terminal& term = Terminal::instance();
            term.addLog("[VIP] Commands: vipstatus, viptest <off|vip|super_vip|ultra_vip>");
            vipStatus();
        },
        "08 05 2026",
        CommandCategory::Player
    });
}
