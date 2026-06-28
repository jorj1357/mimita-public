import { useState } from "react"

const SIZE_MAP = { sm: 28, md: 64, lg: 128 }

function fallbackSrc(user) {
    const encoded = encodeURIComponent(user?.username || "?")
    return `/api/avatar/initials?name=${encoded}&size=128`
}

export default function Avatar({ user, size = "md", className = "" }) {
    const [imgError, setImgError] = useState(false)
    if (!user) return null

    const pixelSize = SIZE_MAP[size] || 64
    const cacheBust = user.avatar_updated_at
        ? new Date(user.avatar_updated_at).getTime()
        : Date.now()
    const avatarUrl = user.avatar_url || fallbackSrc(user)
    const src = `${avatarUrl}?_=${cacheBust}`

    const imgStyle = {
        width: pixelSize,
        height: pixelSize,
        fontSize: pixelSize * 0.4,
        lineHeight: pixelSize + "px",
        borderRadius: "50%",
        overflow: "hidden",
        flexShrink: 0,
        objectFit: "cover"
    }

    const placeholderStyle = {
        width: pixelSize,
        height: pixelSize,
        fontSize: pixelSize * 0.4,
        lineHeight: pixelSize + "px",
        borderRadius: "50%",
        overflow: "hidden",
        flexShrink: 0,
        display: "flex",
        alignItems: "center",
        justifyContent: "center",
        background: "rgba(255,255,255,0.1)",
        fontWeight: 700,
        color: "rgba(255,255,255,0.4)"
    }

    if (src && !imgError) {
        return (
            <img
                src={src}
                alt=""
                className={`avatar avatarImg avatar-${size} ${className}`}
                style={imgStyle}
                onError={() => setImgError(true)}
            />
        )
    }

    const fallback = fallbackSrc(user)

    return (
        <img
            src={fallback}
            alt=""
            className={`avatar avatarImg avatar-${size} ${className}`}
            style={imgStyle}
        />
    )
}

