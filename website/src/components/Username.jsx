const TIER_COLORS = {
    free: "#9e9e9e",
    vip: "#40e0d0",
    super_vip: "#00008b",
    ultra_vip: "ultra_vip",
    moderator: "#800080",
    admin: "#000000",
    owner: "#ffd700"
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

export default function Username({ user, size = "md" }) {
    if (!user) return null

    const tier = user.supporter_tier || "free"
    const color = TIER_COLORS[tier] || TIER_COLORS.free

    const style = {
        fontWeight: 700,
        fontSize: size === "sm" ? "0.85rem" : size === "lg" ? "1.5rem" : "1rem"
    }

    if (tier === "ultra_vip") {
        style.background = "linear-gradient(90deg, #ff0000, #ff7700, #ffff00, #00ff00, #0000ff, #8b00ff, #ff0000)"
        style.backgroundSize = "200% auto"
        style.webkitBackgroundClip = "text"
        style.webkitTextFillColor = "transparent"
        style.animation = "rainbow 3s linear infinite"
    }
    else {
        style.color = color
    }

    return (
        <span className="username" style={style} title={TIER_LABELS[tier] || undefined}>
            {user.username}
            {TIER_LABELS[tier] && (
                <span className="tierBadge" style={{
                    fontSize: "0.65em",
                    marginLeft: "0.4em",
                    padding: "0.1em 0.4em",
                    borderRadius: "3px",
                    backgroundColor: color,
                    color: tier === "super_vip" || tier === "admin" || tier === "owner" ? "#fff" : "#000",
                    verticalAlign: "middle"
                }}>
                    {TIER_LABELS[tier]}
                </span>
            )}
        </span>
    )
}

export { TIER_COLORS, TIER_LABELS }