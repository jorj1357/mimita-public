// 08 03 2026, 17 20
/* purpose
* Owns VIP tier, purchase, badge, and style validation constants for the website API.
* Centralizes Stripe Price environment-variable names and user-selectable name-style limits.
* Provides small pure helpers reused by VIP routes, game APIs, and tests.
* DOES NOT contact Stripe or Postgres directly.
* DOES NOT grant entitlements or mutate user account state.
* DOES NOT render React components or game UI.
*/

export const VIP_TIERS = ["free", "vip", "super_vip", "ultra_vip"]
export const PAID_VIP_TIERS = ["vip", "super_vip", "ultra_vip"]

export const VIP_TIER_RANK = {
    free: 0,
    vip: 1,
    super_vip: 2,
    ultra_vip: 3
}

export const VIP_BADGES = {
    vip: "/assets/images/mimita%20vip%202.png",
    super_vip: "/assets/images/mimita%20super%20vip.png",
    ultra_vip: "/assets/images/mimita%20ultra%20vip.png"
}

export const STAFF_ROLE_COLORS = {
    owner: "#000000",
    admin: "#191919",
    moderator: "#ff0000"
}

export const VIP_COLORS = {
    free: "#9a9a9a",
    vip: "#40e0d0"
}

export const VIP_PURCHASE_TYPES = ["one_month", "monthly_subscription", "twelve_month"]

export const VIP_PRICE_CONFIG = {
    vip: {
        one_month: {
            label: "VIP one month",
            amount_cents: 333,
            currency: "usd",
            mode: "payment",
            calendar_months: 1,
            price_env: "MIMITA_STRIPE_PRICE_VIP_ONE_MONTH"
        },
        monthly_subscription: {
            label: "VIP monthly",
            amount_cents: 333,
            currency: "usd",
            mode: "subscription",
            price_env: "MIMITA_STRIPE_PRICE_VIP_MONTHLY"
        },
        twelve_month: {
            label: "VIP twelve months",
            amount_cents: 1999,
            currency: "usd",
            mode: "payment",
            calendar_months: 12,
            price_env: "MIMITA_STRIPE_PRICE_VIP_TWELVE_MONTH"
        }
    },
    super_vip: {
        one_month: {
            label: "Super VIP one month",
            amount_cents: 888,
            currency: "usd",
            mode: "payment",
            calendar_months: 1,
            price_env: "MIMITA_STRIPE_PRICE_SUPER_VIP_ONE_MONTH"
        },
        monthly_subscription: {
            label: "Super VIP monthly",
            amount_cents: 888,
            currency: "usd",
            mode: "subscription",
            price_env: "MIMITA_STRIPE_PRICE_SUPER_VIP_MONTHLY"
        },
        twelve_month: {
            label: "Super VIP twelve months",
            amount_cents: 5333,
            currency: "usd",
            mode: "payment",
            calendar_months: 12,
            price_env: "MIMITA_STRIPE_PRICE_SUPER_VIP_TWELVE_MONTH"
        }
    },
    ultra_vip: {
        one_month: {
            label: "Ultra VIP one month",
            amount_cents: 1777,
            currency: "usd",
            mode: "payment",
            calendar_months: 1,
            price_env: "MIMITA_STRIPE_PRICE_ULTRA_VIP_ONE_MONTH"
        },
        monthly_subscription: {
            label: "Ultra VIP monthly",
            amount_cents: 1777,
            currency: "usd",
            mode: "subscription",
            price_env: "MIMITA_STRIPE_PRICE_ULTRA_VIP_MONTHLY"
        },
        twelve_month: {
            label: "Ultra VIP twelve months",
            amount_cents: 10666,
            currency: "usd",
            mode: "payment",
            calendar_months: 12,
            price_env: "MIMITA_STRIPE_PRICE_ULTRA_VIP_TWELVE_MONTH"
        }
    }
}

export const VIP_STYLE_LIMITS = {
    maxJsonBytes: 4096,
    maxPerLetterColors: 32,
    maxGradientColors: 8,
    minRainbowSpeed: 0.15,
    maxRainbowSpeed: 2.0,
    maxPresetCount: 12,
    maxPresetNameLength: 40
}

export const VIP_STYLE_KINDS = {
    none: { minTier: "free" },
    vip_turquoise: { minTier: "vip" },
    rainbow: { minTier: "super_vip" },
    solid: { minTier: "super_vip" },
    animated_rainbow: { minTier: "ultra_vip" },
    per_letter: { minTier: "ultra_vip" },
    color_cycle: { minTier: "ultra_vip" }
}

export const VIP_RAINBOW_DIRECTIONS = ["ltr", "rtl"]
export const VIP_ANIMATIONS = ["none", "cycle", "pulse"]

const DEFAULT_RAINBOW = [
    "#ff0044",
    "#ffcc00",
    "#00ff66",
    "#00ccff",
    "#9944ff"
]

export function normalizeTier(tier) {
    const value = String(tier || "free").trim().toLowerCase()
    return VIP_TIERS.includes(value) ? value : "free"
}

export function tierRank(tier) {
    return VIP_TIER_RANK[normalizeTier(tier)] || 0
}

export function tierIncludes(activeTier, requiredTier) {
    return tierRank(activeTier) >= tierRank(requiredTier)
}

export function highestTier(tiers) {
    let best = "free"
    for (const tier of tiers || []) {
        const normalized = normalizeTier(tier)
        if (tierRank(normalized) > tierRank(best)) best = normalized
    }
    return best
}

export function badgeForTier(tier) {
    const normalized = normalizeTier(tier)
    return VIP_BADGES[normalized] || ""
}

export function getPurchaseDefinition(tier, purchaseType) {
    const normalizedTier = normalizeTier(tier)
    const normalizedType = String(purchaseType || "").trim()
    return VIP_PRICE_CONFIG[normalizedTier]?.[normalizedType] || null
}

export function getStripePriceId(tier, purchaseType, env = process.env) {
    const def = getPurchaseDefinition(tier, purchaseType)
    if (!def) return ""
    return String(env[def.price_env] || "").trim()
}

export function publicVipConfig(env = process.env) {
    return {
        tiers: PAID_VIP_TIERS.map(tier => ({
            tier,
            rank: tierRank(tier),
            badge_url: badgeForTier(tier),
            default_style: defaultStyleForTier(tier),
            purchases: VIP_PURCHASE_TYPES.map(type => {
                const def = getPurchaseDefinition(tier, type)
                return {
                    type,
                    label: def.label,
                    amount_cents: def.amount_cents,
                    currency: def.currency,
                    mode: def.mode,
                    configured: Boolean(getStripePriceId(tier, type, env))
                }
            })
        })),
        reserved_staff_colors: STAFF_ROLE_COLORS,
        style_limits: VIP_STYLE_LIMITS
    }
}

export function normalizeHexColor(value) {
    const raw = String(value || "").trim()
    if (!/^#[0-9a-fA-F]{6}$/.test(raw)) return ""
    return raw.toLowerCase()
}

export function isReservedStaffColor(color) {
    const hex = normalizeHexColor(color)
    return Boolean(hex && Object.values(STAFF_ROLE_COLORS).includes(hex))
}

export function staffStyleForRole(role) {
    const normalized = String(role || "user").trim().toLowerCase()
    const color = STAFF_ROLE_COLORS[normalized]
    if (!color) return null
    return {
        role: normalized,
        color,
        overrides_vip_name: true
    }
}

export function defaultStyleForTier(tier) {
    const normalized = normalizeTier(tier)
    if (normalized === "ultra_vip") {
        return {
            version: 1,
            kind: "animated_rainbow",
            colors: DEFAULT_RAINBOW,
            rainbow_speed: 1.0,
            rainbow_direction: "ltr",
            animation: "cycle"
        }
    }
    if (normalized === "super_vip") {
        return {
            version: 1,
            kind: "rainbow",
            colors: DEFAULT_RAINBOW,
            rainbow_speed: 0.75,
            rainbow_direction: "ltr",
            animation: "none"
        }
    }
    if (normalized === "vip") {
        return {
            version: 1,
            kind: "vip_turquoise",
            solid_color: VIP_COLORS.vip,
            animation: "none"
        }
    }
    return {
        version: 1,
        kind: "none",
        solid_color: VIP_COLORS.free,
        animation: "none"
    }
}

function normalizeColorList(value, maxCount) {
    if (!Array.isArray(value)) return []
    const colors = []
    for (const item of value) {
        const hex = normalizeHexColor(item)
        if (!hex) return null
        colors.push(hex)
        if (colors.length > maxCount) return null
    }
    return colors
}

export function validateNameStyle(input, options = {}) {
    const activeTier = normalizeTier(options.activeTier || options.tier)
    const role = String(options.role || "user").trim().toLowerCase()
    const style = input && typeof input === "object" ? input : {}
    const rawBytes = Buffer.byteLength(JSON.stringify(style), "utf8")
    if (rawBytes > VIP_STYLE_LIMITS.maxJsonBytes) {
        return { ok: false, message: "style payload is too large" }
    }

    const kind = String(style.kind || defaultStyleForTier(activeTier).kind).trim()
    const kindDef = VIP_STYLE_KINDS[kind]
    if (!kindDef) {
        return { ok: false, message: "unsupported VIP name style" }
    }
    if (!tierIncludes(activeTier, kindDef.minTier)) {
        return { ok: false, message: "that style is locked for your VIP tier" }
    }

    const result = {
        version: 1,
        kind,
        animation: "none"
    }

    if (kind === "solid") {
        const solid = normalizeHexColor(style.solid_color)
        if (!solid) return { ok: false, message: "solid color must be #RRGGBB" }
        if (!staffStyleForRole(role) && isReservedStaffColor(solid)) {
            return { ok: false, message: "that color is reserved for staff" }
        }
        result.solid_color = solid
    }
    else if (kind === "vip_turquoise") {
        result.solid_color = VIP_COLORS.vip
    }

    if (["rainbow", "animated_rainbow", "color_cycle"].includes(kind)) {
        const colors = normalizeColorList(style.colors || DEFAULT_RAINBOW, VIP_STYLE_LIMITS.maxGradientColors)
        if (!colors || colors.length < 2) {
            return { ok: false, message: "rainbow styles need 2 to 8 colors" }
        }
        result.colors = colors
    }

    if (kind === "per_letter") {
        const colors = normalizeColorList(style.colors || [], VIP_STYLE_LIMITS.maxPerLetterColors)
        if (!colors || colors.length < 1) {
            return { ok: false, message: "per-letter styles need at least one color" }
        }
        result.colors = colors
    }

    if (["rainbow", "animated_rainbow", "per_letter", "color_cycle"].includes(kind)) {
        const speed = Number(style.rainbow_speed ?? defaultStyleForTier(activeTier).rainbow_speed ?? 1)
        if (!Number.isFinite(speed) ||
            speed < VIP_STYLE_LIMITS.minRainbowSpeed ||
            speed > VIP_STYLE_LIMITS.maxRainbowSpeed) {
            return { ok: false, message: "rainbow speed is outside the safe range" }
        }
        const direction = String(style.rainbow_direction || "ltr")
        if (!VIP_RAINBOW_DIRECTIONS.includes(direction)) {
            return { ok: false, message: "unsupported rainbow direction" }
        }
        const animation = String(style.animation || (kind === "animated_rainbow" ? "cycle" : "none"))
        if (!VIP_ANIMATIONS.includes(animation)) {
            return { ok: false, message: "unsupported animation type" }
        }
        result.rainbow_speed = Math.round(speed * 100) / 100
        result.rainbow_direction = direction
        result.animation = animation
    }

    return { ok: true, style: result }
}

export function safeStyleForTier(input, options = {}) {
    const activeTier = normalizeTier(options.activeTier || options.tier)
    const validated = validateNameStyle(input, { ...options, activeTier })
    if (validated.ok) return validated.style
    return defaultStyleForTier(activeTier)
}
