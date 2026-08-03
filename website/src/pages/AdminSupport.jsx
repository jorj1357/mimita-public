import { useEffect, useState } from "react"
import { useNavigate } from "react-router-dom"
import { apiRequestRaw } from "../lib/api.js"
import Layout from "../components/Layout"
import "../styles/banner.css"

const TOPIC_LABELS = {
    user_issue: "User issue",
    game_issue: "Game issue",
    payment_finance: "Payment or finance",
    security: "Security",
    other: "Other"
}

export default function AdminSupport() {
    const navigate = useNavigate()
    const [requests, setRequests] = useState([])
    const [topic, setTopic] = useState("")
    const [loading, setLoading] = useState(true)
    const [error, setError] = useState("")
    const [message, setMessage] = useState("")

    useEffect(() => {
        fetchRequests()
    }, [])

    async function fetchRequests() {
        setLoading(true)
        setError("")
        try {
            const path = topic ? `/api/admin/support?topic=${encodeURIComponent(topic)}` : "/api/admin/support"
            const { response, data } = await apiRequestRaw(path)
            if (response.status === 401) { navigate("/admin/login"); return }
            if (response.status === 403) { navigate("/admin/no-permission"); return }
            if (data?.success) setRequests(data.requests || [])
            else setError(data?.message || "unable to load support requests")
        }
        catch {
            setError("unable to load support requests")
        }
        finally {
            setLoading(false)
        }
    }

    async function setStatus(id, status) {
        setMessage("")
        const { response, data } = await apiRequestRaw(`/api/admin/support/${id}`, { method: "PATCH", body: JSON.stringify({ status }) })
        if (response.status === 401) { navigate("/admin/login"); return }
        if (response.status === 403) { navigate("/admin/no-permission"); return }
        if (data?.success) { setMessage("updated"); fetchRequests() }
        else setError(data?.message || "failed")
    }

    return (
        <Layout>
            <div className="adminPage">
                <div className="adminHeader">
                    <h1 className="adminTitle">support</h1>
                    <select className="adminSearchInput" style={{ width: 240 }} value={topic} onChange={e => { setTopic(e.target.value); fetchRequests() }}>
                        <option value="">all topics</option>
                        {Object.entries(TOPIC_LABELS).map(([value, label]) => <option key={value} value={value}>{label}</option>)}
                    </select>
                </div>

                {error && <p className="adminError">{error}</p>}
                {message && <p className="adminEditorMessage">{message}</p>}

                {loading ? (
                    <p className="adminEmpty">loading...</p>
                ) : requests.length === 0 ? (
                    <p className="adminEmpty">no support requests</p>
                ) : (
                    <div className="adminFeedbackFeed">
                        {requests.map(r => (
                            <div key={r.id} className="adminFeedbackItem" style={{ borderLeft: `3px solid ${r.topic === "security" ? "#f87171" : r.topic === "payment_finance" ? "#fbbf24" : "rgba(255,255,255,0.2)"}` }}>
                                <span className="adminFeedbackTime" style={{ minWidth: 70 }}>#{r.id}<br />{new Date(r.created_at).toLocaleString()}</span>
                                <span className="adminFeedbackUser" style={{ flexDirection: "column", alignItems: "flex-start" }}>
                                    <span className="adminFeedbackUsername">{r.username || "guest"}</span>
                                    <span className="adminFeedbackText" style={{ fontSize: 11, opacity: 0.7 }}>{r.email}</span>
                                </span>
                                <span className="adminFeedbackText" style={{ flex: 2 }}>
                                    <strong>[{TOPIC_LABELS[r.topic] || r.topic}]</strong> {r.subject}
                                    <br />
                                    {r.message}
                                    {r.url && <span className="adminFeedbackPage"> url: {r.url}</span>}
                                    {r.banner_order_id && <span className="adminFeedbackPage"> banner order: {r.banner_order_id}</span>}
                                </span>
                                <span className="adminFeedbackPage" style={{ minWidth: 90 }}>{r.status}</span>
                                <span className="bannerAdminActions">
                                    {["new", "open", "in_progress", "resolved"].map(s => (
                                        <button key={s} className="bannerAdminBtn" onClick={() => setStatus(r.id, s)}>{s}</button>
                                    ))}
                                </span>
                            </div>
                        ))}
                    </div>
                )}
            </div>
        </Layout>
    )
}
