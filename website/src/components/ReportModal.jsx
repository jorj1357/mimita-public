import { useState } from "react"
import { apiRequest } from "../lib/api"

export default function ReportModal({ userId, username, onClose }) {
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
                body: JSON.stringify({
                    reportedId: userId,
                    reason: reason.trim()
                })
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
                    <p>Thank you. Your report against <strong>{username}</strong> has been submitted for review.</p>
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
                        placeholder="Describe why you're reporting this user..."
                        maxLength={2000}
                        rows={4}
                        required
                    />
                    {error && <p className="modalError">{error}</p>}
                    <div className="modalActions">
                        <button type="button" className="modalCancelBtn" onClick={onClose}>Cancel</button>
                        <button type="submit" className="modalSubmitBtn" disabled={!reason.trim() || submitting}>
                            {submitting ? "Submitting..." : "Submit Report"}
                        </button>
                    </div>
                </form>
            </div>
        </div>
    )
}
