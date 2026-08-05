// 08 03 2026, 17 20
/* purpose
* Renders Mimita usernames with server-authoritative VIP style and badge data.
* Supports staff color precedence, solid, rainbow, animated rainbow, and per-letter web previews.
* Falls back to legacy supporter_tier data when older API responses do not include vip.
* DOES NOT calculate entitlement state or unlock paid features on the client.
* DOES NOT create checkout sessions or mutate account settings.
* DOES NOT render game-engine HUD text.
*/

import VipBadge from "./VipBadge"

const TIER_COLORS = {
    free: "#9e9e9e",
    vip: "#40e0d0",
    super_vip: "#40e0d0",
    ultra_vip: "#40e0d0",
    moderator: "#ff0000",
    admin: "#191919",
    owner: "#000000"
}

const TIER_LABELS = {
    free: "",
    vip: "VIP",
    super_vip: "SUPER VIP",
    ultra_vip: "ULTRA VIP",
    moderator: "MOD",
    admin: "ADMIN",
    owner: "OWNER"
}

const FALLBACK_BADGES = {
    vip: "/assets/images/mimita%20vip%202.png",
    super_vip: "/assets/images/mimita%20super%20vip.png",
    ultra_vip: "/assets/images/mimita%20ultra%20vip.png"
}

function normalizeTier(user) {
    return user?.vip?.active_tier || user?.supporter_tier || "free"
}

function displayName(user) {
    return user?.display_name || user?.username || "?"
}

function badgeUrl(user, tier) {
    return user?.vip?.badge_url || FALLBACK_BADGES[tier] || ""
}

function styleFor(user, tier) {
    const vip = user?.vip
    const staffColor = vip?.display?.name_color_override
    if (staffColor) {
        return { kind: "solid", solid_color: staffColor, staff: true }
    }
    if (vip?.name_style) return vip.name_style
    if (tier === "ultra_vip") return { kind: "animated_rainbow", rainbow_speed: 1, rainbow_direction: "ltr" }
    if (tier === "super_vip") return { kind: "rainbow", rainbow_speed: 0.75, rainbow_direction: "ltr" }
    if (tier === "vip") return { kind: "vip_turquoise", solid_color: "#40e0d0" }
    return { kind: "none", solid_color: TIER_COLORS.free }
}

function animationDuration(style) {
    const speed = Number(style?.rainbow_speed || 1)
    const safe = Number.isFinite(speed) ? Math.min(Math.max(speed, 0.15), 2) : 1
    return `${Math.round((3 / safe) * 100) / 100}s`
}

function renderText(name, style, baseClass) {
    if (style.kind === "per_letter" && Array.isArray(style.colors) && style.colors.length) {
        return (
            <span className={`${baseClass} vipNamePerLetter`}>
                {[...name].map((ch, i) => (
                    <span key={`${ch}-${i}`} style={{ color: style.colors[i % style.colors.length] }}>
                        {ch}
                    </span>
                ))}
            </span>
        )
    }

    if (["rainbow", "animated_rainbow", "color_cycle"].includes(style.kind)) {
        const colors = Array.isArray(style.colors) && style.colors.length > 1
            ? style.colors
            : ["#ff0044", "#ffcc00", "#00ff66", "#00ccff", "#9944ff"]
        const directionClass = style.rainbow_direction === "rtl" ? "vipNameReverse" : ""
        const animatedClass = style.kind === "rainbow" ? "" : "vipNameAnimated"
        return (
            <span
                className={`${baseClass} vipNameRainbow ${animatedClass} ${directionClass}`}
                style={{
                    backgroundImage: `linear-gradient(90deg, ${colors.join(", ")}, ${colors[0]})`,
                    animationDuration: animationDuration(style)
                }}
            >
                {name}
            </span>
        )
    }

    return (
        <span
            className={baseClass}
            style={{ color: style.solid_color || TIER_COLORS.free }}
        >
            {name}
        </span>
    )
}

export default function Username({ user, size = "md", showBadge = true, style: styleOverride = null }) {
    if (!user) return null

    const tier = normalizeTier(user)
    const style = styleOverride || styleFor(user, tier)
    const badge = badgeUrl(user, tier)
    const label = TIER_LABELS[tier] || ""
    const sizeClass = size === "sm" ? "vipNameSm" : size === "lg" ? "vipNameLg" : "vipNameMd"

    return (
        <span className={`username vipName ${sizeClass}`} title={label || undefined}>
            {renderText(displayName(user), style, "vipNameText")}
            {showBadge && <VipBadge tier={tier} src={badge} size={size} />}
        </span>
    )
}

export { TIER_COLORS, TIER_LABELS }
