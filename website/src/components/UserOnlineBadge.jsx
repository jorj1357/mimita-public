import { useEffect, useState } from "react"

function formatLastSeen(lastSeenAt) {
    if (!lastSeenAt) return null
    const now = new Date()
    const then = new Date(lastSeenAt)
    const diffMs = now - then
    const diffSec = Math.floor(diffMs / 1000)
    const diffMin = Math.floor(diffSec / 60)
    const diffHr = Math.floor(diffMin / 60)
    const diffDay = Math.floor(diffHr / 24)

    if (diffSec < 300) return "online"
    if (diffMin < 60) return `${diffMin}m ago`
    if (diffHr < 24) return `${diffHr}h ago`
    if (diffDay < 7) return `${diffDay}d ago`
    return then.toLocaleDateString([], { month: "short", day: "numeric" })
}

export default function UserOnlineBadge({ lastSeenAt, size = "sm" }) {
    const label = formatLastSeen(lastSeenAt)
    if (!label) return null

    const isOnline = label === "online"
    const dotClass = isOnline ? "online-dot" : "offline-dot"

    return (
        <span className={`onlineBadge ${size}`}>
            <span className={dotClass} />
            <span className="onlineLabel">{label}</span>
        </span>
    )
}
