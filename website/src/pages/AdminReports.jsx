import { useEffect, useState } from "react"
import Layout from "../components/Layout"
import { apiRequest } from "../lib/api"

const STATUS_LABELS = {
    new: "New",
    reviewing: "Reviewing",
    action_taken: "Action Taken",
    no_action: "No Action",
    duplicate: "Duplicate",
    escalated: "Escalated"
}

const STATUS_COLORS = {
    new: "#ffd900",
    reviewing: "#00d9d9",
    action_taken: "#4caf50",
    no_action: "#888",
    duplicate: "#ff9800",
    escalated: "#f44336"
}

export default function AdminReports() {
    const [reports, setReports] = useState([])
    const [loading, setLoading] = useState(true)
    const [counts, setCounts] = useState({})
    const [filter, setFilter] = useState("")

    useEffect(() => {
        loadReports()
    }, [filter])

    async function loadReports() {
        setLoading(true)
        try {
            const url = filter
                ? `/api/admin/moderation/reports?status=${filter}`
                : `/api/admin/moderation/reports`
            const [data, countData] = await Promise.all([
                apiRequest(url),
                apiRequest("/api/admin/moderation/reports/counts")
            ])
            if (data?.success) setReports(data.reports || [])
            if (countData?.success) setCounts(countData.counts || {})
        } catch {}
        setLoading(false)
    }

    async function updateStatus(reportId, status) {
        try {
            await apiRequest(`/api/admin/moderation/reports/${reportId}`, {
                method: "PATCH",
                body: JSON.stringify({ status })
            })
            setReports(prev => prev.map(r =>
                r.id === reportId ? { ...r, status, reviewed_at: new Date().toISOString() } : r
            ))
        } catch {}
    }

    function formatDate(dateStr) {
        if (!dateStr) return ""
        return new Date(dateStr).toLocaleDateString([], { month: "short", day: "numeric", hour: "2-digit", minute: "2-digit" })
    }

    const totalReports = Object.values(counts).reduce((a, b) => a + b, 0)

    return (
        <Layout>
            <section className="adminPage">
                <h1 className="pageHeading">Moderation Queue</h1>
                <p className="adminSubtitle">{totalReports} total reports</p>

                <div className="adminFilters">
                    <button
                        className={`filterBtn ${filter === "" ? "active" : ""}`}
                        onClick={() => setFilter("")}
                    >
                        All ({totalReports})
                    </button>
                    {Object.entries(STATUS_LABELS).map(([key, label]) => (
                        <button
                            key={key}
                            className={`filterBtn ${filter === key ? "active" : ""}`}
                            onClick={() => setFilter(key)}
                        >
                            {label} ({counts[key] || 0})
                        </button>
                    ))}
                </div>

                {loading ? (
                    <p>Loading reports...</p>
                ) : reports.length === 0 ? (
                    <p className="adminEmpty">No reports{filter ? " with this status" : ""}.</p>
                ) : (
                    <div className="reportsList">
                        {reports.map(report => (
                            <div key={report.id} className="reportCard">
                                <div className="reportHeader">
                                    <span className="reportId">#{report.id}</span>
                                    <span
                                        className="reportStatus"
                                        style={{ color: STATUS_COLORS[report.status] }}
                                    >
                                        {STATUS_LABELS[report.status] || report.status}
                                    </span>
                                    <span className="reportTime">{formatDate(report.created_at)}</span>
                                </div>
                                <div className="reportUsers">
                                    <span className="reportLabel">Reporter:</span>
                                    <a href={`/u/${report.reporter_username}`}>{report.reporter_username}</a>
                                </div>
                                <div className="reportUsers">
                                    <span className="reportLabel">Reported:</span>
                                    <a href={`/u/${report.reported_username}`}>{report.reported_username}</a>
                                </div>
                                <div className="reportReason">
                                    <span className="reportLabel">Reason:</span>
                                    <p>{report.reason}</p>
                                </div>
                                {report.server_id && (
                                    <div className="reportMeta">
                                        <span className="reportLabel">Server:</span> {report.server_id}
                                    </div>
                                )}
                                {report.match_id && (
                                    <div className="reportMeta">
                                        <span className="reportLabel">Match:</span> {report.match_id}
                                    </div>
                                )}
                                <div className="reportActions">
                                    {Object.entries(STATUS_LABELS).map(([key, label]) => (
                                        <button
                                            key={key}
                                            className={`reportActionBtn ${report.status === key ? "current" : ""}`}
                                            onClick={() => updateStatus(report.id, key)}
                                            disabled={report.status === key}
                                        >
                                            {label}
                                        </button>
                                    ))}
                                </div>
                            </div>
                        ))}
                    </div>
                )}
            </section>
        </Layout>
    )
}
