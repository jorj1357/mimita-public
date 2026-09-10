import { useState, useEffect } from "react"
import { apiRequest } from "../lib/api"

export default function BannerReactions({ bannerId }) {
    const [reactions, setReactions] = useState([])
    const [loading, setLoading] = useState(true)

    useEffect(() => {
        if (!bannerId) return
        apiRequest(`/api/banner/${bannerId}/reactions`)
            .then(data => {
                if (data?.success) setReactions(data.reactions || [])
            })
            .catch(() => {})
            .finally(() => setLoading(false))
    }, [bannerId])

    async function handleReact(emoji) {
        try {
            const data = await apiRequest(`/api/banner/${bannerId}/react`, {
                method: "POST",
                body: JSON.stringify({ emoji })
            })
            if (data?.success) {
                setReactions(prev => {
                    const existing = prev.find(r => r.emoji === emoji)
                    if (data.action === "added") {
                        if (existing) {
                            return prev.map(r => r.emoji === emoji
                                ? { ...r, count: r.count + 1, userReacted: true }
                                : r
                            )
                        }
                        return [...prev, { emoji, count: 1, userReacted: true }]
                    }
                    if (existing) {
                        if (existing.count <= 1) {
                            return prev.filter(r => r.emoji !== emoji)
                        }
                        return prev.map(r => r.emoji === emoji
                            ? { ...r, count: r.count - 1, userReacted: false }
                            : r
                        )
                    }
                    return prev
                })
            }
        } catch {}
    }

    if (loading || !bannerId) return null

    const EMOJIS = ["&#x1F44D;", "&#x2764;&#xFE0F;", "&#x1F602;", "&#x1F525;", "&#x1F389;"]

    return (
        <div className="bannerReactions">
            <div className="bannerReactionBtns">
                {EMOJIS.map(emoji => (
                    <button
                        key={emoji}
                        className="bannerReactBtn"
                        onClick={() => handleReact(emoji)}
                        dangerouslySetInnerHTML={{ __html: emoji }}
                    />
                ))}
            </div>
            {reactions.length > 0 && (
                <div className="bannerReactionCounts">
                    {reactions.map(r => (
                        <span
                            key={r.emoji}
                            className={`bannerReactionChip ${r.userReacted ? "reacted" : ""}`}
                            dangerouslySetInnerHTML={{ __html: `${r.emoji} ${r.count}` }}
                        />
                    ))}
                </div>
            )}
        </div>
    )
}
