const SIZE_MAP = { sm: 28, md: 64, lg: 128 }

export default function Avatar({ user, size = "md", className = "" }) {
    if (!user) return null

    const pixelSize = SIZE_MAP[size] || 64
    const src = user.avatar_url
        ? `${user.avatar_url}?v=${user.avatar_updated_at || ""}`
        : null

    const style = {
        width: pixelSize,
        height: pixelSize,
        fontSize: pixelSize * 0.4,
        lineHeight: pixelSize + "px",
        borderRadius: "50%",
        overflow: "hidden",
        flexShrink: 0
    }

    if (src) {
        return (
            <img
                src={src}
                alt=""
                className={`avatar avatarImg avatar-${size} ${className}`}
                style={style}
            />
        )
    }

    return (
        <div
            className={`avatar avatarPlaceholder avatar-${size} ${className}`}
            style={{
                ...style,
                display: "flex",
                alignItems: "center",
                justifyContent: "center",
                background: "rgba(255,255,255,0.1)",
                fontWeight: 700,
                color: "rgba(255,255,255,0.4)"
            }}
        >
            {user.username?.[0]?.toUpperCase() || "?"}
        </div>
    )
}
