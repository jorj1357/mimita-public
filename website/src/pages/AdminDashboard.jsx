import { useEffect, useState } from "react"
import { useNavigate } from "react-router-dom"
import { apiRequest, apiRequestRaw } from "../lib/api.js"
import DebugPanel from "../components/DebugPanel.jsx"

export default function AdminDashboard() {
    const navigate = useNavigate()
    const [metrics, setMetrics] = useState(null)
    const [feedback, setFeedback] = useState([])
    const [loading, setLoading] = useState(true)
    const [error, setError] = useState("")
    const [refreshing, setRefreshing] = useState(false)
    const [showDebug, setShowDebug] = useState(false)

    useEffect(() => {
        fetchDashboard()
        fetchFeedback()
    }, [])

    async function fetchDashboard() {
        try {
            const { response, data } = await apiRequestRaw("/api/admin/dashboard")
            if (response.status === 401) {
                navigate("/admin/login")
                return
            }
            if (data?.success) {
                setMetrics(data.metrics)
            }
            else {
                setError(data?.message || "failed to load dashboard")
            }
        }
        catch {
            setError("unable to reach server")
        }
        finally {
            setLoading(false)
        }
    }

    async function fetchFeedback() {
        try {
            const data = await apiRequest("/api/admin/feedback?limit=10")
            if (data.success) {
                setFeedback(data.feedback)
            }
        }
        catch {
            // silent
        }
    }

    async function handleRefresh() {
        setRefreshing(true)
        try {
            const data = await apiRequest("/api/admin/dashboard/refresh", { method: "POST" })
            if (data.success) {
                setMetrics(data.metrics)
            }
        }
        catch {
            setError("refresh failed")
        }
        finally {
            setRefreshing(false)
        }
    }

    async function handleLogout() {
        await apiRequest("/api/admin/logout", { method: "POST" })
        navigate("/admin/login")
    }

    function formatTime(d) {
        return new Date(d).toLocaleTimeString([], { hour: "2-digit", minute: "2-digit" })
    }

    if (loading) {
        return (
            <div className="adminPage">
                <p>loading dashboard...</p>
            </div>
        )
    }

    if (error && !metrics) {
        return (
            <div className="adminPage">
                <p className="adminError">{error}</p>
            </div>
        )
    }

    const M = metrics || {}

    return (
        <div className="adminPage">
            <div className="adminHeader">
                <h1 className="adminTitle">mimita admin</h1>
                <div className="adminHeaderActions">
                    <button className="adminDebugBtn" onClick={() => setShowDebug(d => !d)}>
                        {showDebug ? "hide debug" : "debug"}
                    </button>
                    <button className="adminRefreshBtn" onClick={handleRefresh} disabled={refreshing}>
                        {refreshing ? "refreshing..." : "refresh analytics"}
                    </button>
                    <button className="adminLogoutBtn" onClick={handleLogout}>sign out</button>
                </div>
            </div>

            <p className="adminSubtitle">single source of truth</p>

            {error && <p className="adminError">{error}</p>}

            {showDebug && (
                <div className="adminDebugBanner">
                    <DebugPanel />
                </div>
            )}

            <div className="adminGrid">

                {/* Growth */}
                <div className="adminSection">
                    <h2>growth</h2>
                    <div className="adminCard">
                        <span>site visitors today</span>
                        <span className="adminValue">{M.site_visitors_today?.today || 0}</span>
                    </div>
                    <div className="adminCard">
                        <span>site visitors 7d</span>
                        <span className="adminValue">{M.site_visitors_7d?.today || 0}</span>
                    </div>
                    <div className="adminCard">
                        <span>site visitors 30d</span>
                        <span className="adminValue">{M.site_visitors_30d?.today || 0}</span>
                    </div>
                    <div className="adminCard">
                        <span>site visitors all time</span>
                        <span className="adminValue">{M.site_visitors_all?.allTime || 0}</span>
                    </div>
                    <div className="adminCard">
                        <span>downloads today</span>
                        <span className="adminValue">{M.downloads_today?.today || 0}</span>
                    </div>
                    <div className="adminCard">
                        <span>downloads all time</span>
                        <span className="adminValue">{M.downloads_all?.allTime || 0}</span>
                    </div>
                    <div className="adminCard">
                        <span>download conversion %</span>
                        <span className="adminValue">{M.download_conversion_pct ?? "—"}%</span>
                    </div>
                    <div className="adminCard">
                        <span>accounts today</span>
                        <span className="adminValue">{M.accounts_created_today?.today || 0}</span>
                    </div>
                    <div className="adminCard">
                        <span>accounts all time</span>
                        <span className="adminValue">{M.accounts_created_all?.allTime || 0}</span>
                    </div>
                    <div className="adminCard">
                        <span>account conversion %</span>
                        <span className="adminValue">{M.account_conversion_pct ?? "—"}%</span>
                    </div>
                    <div className="adminCard">
                        <span>total users</span>
                        <span className="adminValue">{M.total_users || 0}</span>
                    </div>
                    <div className="adminCard">
                        <span>active sessions</span>
                        <span className="adminValue">{M.active_sessions || 0}</span>
                    </div>
                </div>

                {/* Activity */}
                <div className="adminSection">
                    <h2>activity</h2>
                    <div className="adminCard">
                        <span>DAU</span>
                        <span className="adminValue">{M.dau?.today || 0}</span>
                    </div>
                    <div className="adminCard">
                        <span>WAU</span>
                        <span className="adminValue">{M.wau?.today || 0}</span>
                    </div>
                    <div className="adminCard">
                        <span>MAU</span>
                        <span className="adminValue">{M.mau?.today || 0}</span>
                    </div>
                    <div className="adminCard">
                        <span>unique players today</span>
                        <span className="adminValue">{M.unique_players_today?.today || 0}</span>
                    </div>
                    <div className="adminCard">
                        <span>game opens today</span>
                        <span className="adminValue">{M.game_opens_today?.today || 0}</span>
                    </div>
                </div>

                {/* Engagement */}
                <div className="adminSection">
                    <h2>engagement</h2>
                    <div className="adminCard"><span>sessions &gt; 1m</span><span className="adminValue">{M.sessions_gt_1m?.today || 0}</span></div>
                    <div className="adminCard"><span>sessions &gt; 5m</span><span className="adminValue">{M.sessions_gt_5m?.today || 0}</span></div>
                    <div className="adminCard"><span>sessions &gt; 10m</span><span className="adminValue">{M.sessions_gt_10m?.today || 0}</span></div>
                    <div className="adminCard"><span>sessions &gt; 30m</span><span className="adminValue">{M.sessions_gt_30m?.today || 0}</span></div>
                    <div className="adminCard"><span>sessions &gt; 1h</span><span className="adminValue">{M.sessions_gt_1h?.today || 0}</span></div>
                    <div className="adminCard"><span>avg session length</span><span className="adminValue">{M.avg_session_length_seconds?.today || 0}s</span></div>
                    <div className="adminCard"><span>matches played</span><span className="adminValue">{M.matches_played?.allTime || 0}</span></div>
                    <div className="adminCard"><span>avg matches per user</span><span className="adminValue">{M.avg_matches_per_user?.today || 0}</span></div>
                    <div className="adminCard"><span>first match played</span><span className="adminValue">{M.first_match_played_count?.allTime || 0}</span></div>
                </div>

                {/* Retention */}
                <div className="adminSection">
                    <h2>retention</h2>
                    <div className="adminCard"><span>1 day retention</span><span className="adminValue">{M.retention_1d?.today || 0}%</span></div>
                    <div className="adminCard"><span>7 day retention</span><span className="adminValue">{M.retention_7d?.today || 0}%</span></div>
                    <div className="adminCard"><span>30 day retention</span><span className="adminValue">{M.retention_30d?.today || 0}%</span></div>
                    <div className="adminCard"><span>90 day retention</span><span className="adminValue">{M.retention_90d?.today || 0}%</span></div>
                    <div className="adminCard"><span>365 day retention</span><span className="adminValue">{M.retention_365d?.today || 0}%</span></div>
                    <div className="adminCard"><span>returning users today</span><span className="adminValue">{M.returning_users_today?.today || 0}</span></div>
                </div>

                {/* Community */}
                <div className="adminSection">
                    <h2>community</h2>
                    <div className="adminCard"><span>discord joins</span><span className="adminValue">{M.discord_joins?.allTime || 0}</span></div>
                    <div className="adminCard"><span>messages sent</span><span className="adminValue">{M.messages_sent?.allTime || 0}</span></div>
                    <div className="adminCard"><span>friend requests</span><span className="adminValue">{M.friend_requests?.allTime || 0}</span></div>
                    <div className="adminCard"><span>profiles viewed</span><span className="adminValue">{M.profiles_viewed?.allTime || 0}</span></div>
                    <div className="adminCard"><span>replays saved</span><span className="adminValue">{M.replays_saved?.allTime || 0}</span></div>
                    <div className="adminCard"><span>replays shared</span><span className="adminValue">{M.replays_shared?.allTime || 0}</span></div>
                </div>

                {/* Revenue */}
                <div className="adminSection">
                    <h2>revenue</h2>
                    <div className="adminCard"><span>revenue today</span><span className="adminValue">${M.revenue_today?.today || 0}</span></div>
                    <div className="adminCard"><span>revenue 30d</span><span className="adminValue">${M.revenue_30d?.today || 0}</span></div>
                    <div className="adminCard"><span>revenue all time</span><span className="adminValue">${M.revenue_all?.allTime || 0}</span></div>
                    <div className="adminCard"><span>donation page visits</span><span className="adminValue">{M.donation_page_visits?.allTime || 0}</span></div>
                    <div className="adminCard"><span>donations today</span><span className="adminValue">{M.donations_today?.today || 0}</span></div>
                    <div className="adminCard"><span>donations all time</span><span className="adminValue">{M.donations_all?.allTime || 0}</span></div>
                    <div className="adminCard"><span>donors count</span><span className="adminValue">{M.donors_count?.allTime || 0}</span></div>
                    <div className="adminCard"><span>avg donation</span><span className="adminValue">${M.avg_donation?.allTime || 0}</span></div>
                    <div className="adminCard"><span>donation conversion %</span><span className="adminValue">{M.donation_conversion_pct ?? "—"}%</span></div>
                    <div className="adminCard"><span>donation page exit rate</span><span className="adminValue">{M.donation_page_exit_rate?.today || 0}%</span></div>
                </div>

                {/* Quality */}
                <div className="adminSection">
                    <h2>quality</h2>
                    <div className="adminCard"><span>crashes today</span><span className="adminValue">{M.crashes_today?.today || 0}</span></div>
                    <div className="adminCard"><span>disconnects today</span><span className="adminValue">{M.disconnects_today?.today || 0}</span></div>
                    <div className="adminCard"><span>failed downloads</span><span className="adminValue">{M.failed_downloads?.allTime || 0}</span></div>
                    <div className="adminCard"><span>failed logins</span><span className="adminValue">{M.failed_logins?.allTime || 0}</span></div>
                    <div className="adminCard"><span>avg ping</span><span className="adminValue">{M.avg_ping?.today || 0}ms</span></div>
                    <div className="adminCard"><span>avg FPS</span><span className="adminValue">{M.avg_fps?.today || 0}</span></div>
                </div>

                {/* Feedback Analytics */}
                <div className="adminSection">
                    <h2>feedback</h2>
                    <div className="adminCard"><span>total feedback</span><span className="adminValue">{M.total_feedback || 0}</span></div>
                    <div className="adminCard"><span>feedback today</span><span className="adminValue">{M.feedback_today || 0}</span></div>
                    <div className="adminCard"><span>most common preset</span><span className="adminValue">{M.most_common_preset || "—"}</span></div>
                    <div className="adminCard"><span>most common page</span><span className="adminValue">{M.most_common_page || "—"}</span></div>
                    {M.feedback_by_category && M.feedback_by_category.length > 0 && (
                        <div className="adminCard adminCardList">
                            <span>by category</span>
                            <span className="adminValueSmall">
                                {M.feedback_by_category.map((c, i) => (
                                    <span key={i}>{c.category}: {c.count}</span>
                                ))}
                            </span>
                        </div>
                    )}
                </div>

                {/* Recent Feedback Feed */}
                <div className="adminSection adminSectionWide">
                    <h2>recent feedback</h2>
                    {feedback.length === 0 ? (
                        <p className="adminEmpty">no feedback yet</p>
                    ) : (
                        <div className="adminFeedbackFeed">
                            {feedback.map(f => (
                                <div key={f.id} className="adminFeedbackItem">
                                    <span className="adminFeedbackTime">{formatTime(f.created_at)}</span>
                                    <span className="adminFeedbackText">
                                        {f.custom_feedback ||
                                         (f.selected_presets && f.selected_presets.length > 0
                                             ? f.selected_presets.join(", ")
                                             : "—")}
                                    </span>
                                    {f.page_url && <span className="adminFeedbackPage">{f.page_url}</span>}
                                </div>
                            ))}
                        </div>
                    )}
                </div>

            </div>
        </div>
    )
}
