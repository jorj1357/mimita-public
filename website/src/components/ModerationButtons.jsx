import { useState, useEffect } from "react"
import { apiRequest } from "../lib/api"

export default function ModerationButtons({ userId, username }) {
    const [blocked, setBlocked] = useState(false)
    const [muted, setMuted] = useState(false)
    const [showReport, setShowReport] = useState(false)
    const [statusLoaded, setStatusLoaded] = useState(false)

    useEffect(() => {
        apiRequest(`/api/moderation/status/${userId}`)
            .then(data => {
                if (data?.success) {
                    setBlocked(data.blocked)
                    setMuted(data.muted)
                }
                setStatusLoaded(true)
            })
            .catch(() => setStatusLoaded(true))
    }, [userId])

    async function toggleBlock() {
        try {
            if (blocked) {
                await apiRequest(`/api/moderation/block/${userId}`, { method: "DELETE" })
                setBlocked(false)
            } else {
                await apiRequest(`/api/moderation/block/${userId}`, { method: "POST" })
                setBlocked(true)
            }
        } catch {}
    }

    async function toggleMute() {
        try {
            if (muted) {
                await apiRequest(`/api/moderation/mute/${userId}`, { method: "DELETE" })
                setMuted(false)
            } else {
                await apiRequest(`/api/moderation/mute/${userId}`, { method: "POST" })
                setMuted(true)
            }
        } catch {}
    }

    if (!statusLoaded) return null

    return (
        <div className="moderationButtons">
            <button className="modBtn reportBtn" onClick={() => setShowReport(true)}>
                Report
            </button>
            <button className={`modBtn ${muted ? "unmuteBtn" : "muteBtn"}`} onClick={toggleMute}>
                {muted ? "Unmute" : "Mute"}
            </button>
            <button className={`modBtn ${blocked ? "unblockBtn" : "blockBtn"}`} onClick={toggleBlock}>
                {blocked ? "Unblock" : "Block"}
            </button>

            {showReport && (
                <ReportModalInline
                    userId={userId}
                    username={username}
                    onClose={() => setShowReport(false)}
                />
            )}
        </div>
    )
}

function ReportModalInline({ userId, username, onClose }) {
    const [reason, setReason] = useState("")
    const [submitting, setSubmitting] = useState(false)
    const [submitted, setSubmitted] = useState(false)
    const [error, setError] = useState("")

    async function handleSubmit(e) {
        e.preventDefault()
        if (!reason.trim() || submitting) return
        setSubmitting(true)
        setError("")
        try {
            const data = await apiRequest("/api/moderation/report", {
                method: "POST",
                body: JSON.stringify({ reportedId: userId, reason: reason.trim() })
            })
            if (data?.success) {
                setSubmitted(true)
            } else {
                setError(data?.message || "Failed to submit report")
            }
        } catch (err) {
            setError(err.message || "Failed to submit report")
        }
        setSubmitting(false)
    }

    if (submitted) {
        return (
            <div className="modalOverlay" onClick={onClose}>
                <div className="modalContent" onClick={e => e.stopPropagation()}>
                    <h2>Report Submitted</h2>
                    <p>Thank you. Your report against <strong>{username}</strong> has been submitted.</p>
                    <button className="modalCloseBtn" onClick={onClose}>Close</button>
                </div>
            </div>
        )
    }

    return (
        <div className="modalOverlay" onClick={onClose}>
            <div className="modalContent" onClick={e => e.stopPropagation()}>
                <h2>Report {username}</h2>
                <form onSubmit={handleSubmit}>
                    <textarea
                        value={reason}
                        onChange={e => setReason(e.target.value)}
                        placeholder="Why are you reporting this user?"
                        maxLength={2000}
                        rows={4}
                        required
                    />
                    {error && <p className="modalError">{error}</p>}
                    <div className="modalActions">
                        <button type="button" className="modalCancelBtn" onClick={onClose}>Cancel</button>
                        <button type="submit" className="modalSubmitBtn" disabled={!reason.trim() || submitting}>
                            {submitting ? "Submitting..." : "Submit"}
                        </button>
                    </div>
                </form>
            </div>
        </div>
    )
}
