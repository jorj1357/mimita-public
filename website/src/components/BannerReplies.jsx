import { useState, useEffect } from "react"
import Avatar from "./Avatar"
import Username from "./Username"
import { apiRequest } from "../lib/api"

export default function BannerReplies({ bannerId }) {
    const [replies, setReplies] = useState([])
    const [loading, setLoading] = useState(true)
    const [newReply, setNewReply] = useState("")
    const [sending, setSending] = useState(false)

    useEffect(() => {
        if (!bannerId) return
        apiRequest(`/api/banner/${bannerId}/replies`)
            .then(data => {
                if (data?.success) setReplies(data.replies || [])
            })
            .catch(() => {})
            .finally(() => setLoading(false))
    }, [bannerId])

    async function handleReply(e) {
        e.preventDefault()
        if (!newReply.trim() || sending) return
        setSending(true)
        try {
            const data = await apiRequest(`/api/banner/${bannerId}/replies`, {
                method: "POST",
                body: JSON.stringify({ body: newReply.trim() })
            })
            if (data?.success && data.reply) {
                setReplies(prev => [...prev, data.reply])
                setNewReply("")
            }
        } catch {}
        setSending(false)
    }

    if (!bannerId) return null

    function formatTime(dateStr) {
        if (!dateStr) return ""
        const d = new Date(dateStr)
        const now = new Date()
        const diff = now - d
        if (diff < 86400000) {
            return d.toLocaleTimeString([], { hour: "2-digit", minute: "2-digit" })
        }
        return d.toLocaleDateString([], { month: "short", day: "numeric" })
    }

    return (
        <div className="bannerReplies">
            {loading ? (
                <p className="bannerRepliesLoading">Loading replies...</p>
            ) : (
                <>
                    {replies.length > 0 && (
                        <div className="bannerRepliesList">
                            {replies.map(reply => (
                                <div key={reply.id} className="bannerReply">
                                    <Avatar user={reply} size="xs" />
                                    <div className="bannerReplyContent">
                                        <span className="bannerReplyAuthor">
                                            <Username user={reply} size="sm" />
                                        </span>
                                        <span className="bannerReplyTime">{formatTime(reply.created_at)}</span>
                                        <p className="bannerReplyBody">{reply.body}</p>
                                    </div>
                                </div>
                            ))}
                        </div>
                    )}
                    <form className="bannerReplyForm" onSubmit={handleReply}>
                        <input
                            type="text"
                            value={newReply}
                            onChange={e => setNewReply(e.target.value)}
                            placeholder="Write a reply..."
                            maxLength={1000}
                            disabled={sending}
                        />
                        <button type="submit" disabled={!newReply.trim() || sending}>
                            {sending ? "..." : "Reply"}
                        </button>
                    </form>
                </>
            )}
        </div>
    )
}
