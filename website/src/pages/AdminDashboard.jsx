import { useEffect, useState, useRef } from "react"
import { useNavigate } from "react-router-dom"
import { apiRequest, apiRequestRaw } from "../lib/api.js"
import DebugPanel from "../components/DebugPanel.jsx"
import ErrorBoundary from "../components/ErrorBoundary.jsx"
import Layout from "../components/Layout"

export default function AdminDashboard() {
    const navigate = useNavigate()
    const [metrics, setMetrics] = useState(null)
    const [feedback, setFeedback] = useState([])
    const [loading, setLoading] = useState(true)
    const [error, setError] = useState("")
    const [refreshing, setRefreshing] = useState(false)
    const [showDebug, setShowDebug] = useState(false)
    const [admins, setAdmins] = useState(null)
    const [adminsSearch, setAdminsSearch] = useState("")
    const [adminsLoading, setAdminsLoading] = useState(true)
    const [adminsError, setAdminsError] = useState("")
    const fetchedRef = useRef(false)

    useEffect(() => {
        if (fetchedRef.current) return
        fetchedRef.current = true
        fetchDashboard()
        fetchFeedback()
        fetchAdmins()
    }, [])

    async function fetchAdmins() {
        setAdminsLoading(true)
        setAdminsError("")
        try {
            const { response, data } = await apiRequestRaw("/api/admin/admins")
            if (response.status === 401) {
                navigate("/admin/login")
                return
            }
            if (response.status === 403) {
                navigate("/admin/no-permission")
                return
            }
            if (data?.success) {
                setAdmins(data.admins || [])
            }
            else {
                setAdminsError(data?.message || "unable to load administrators")
                setAdmins([])
                console.log("[ADMIN] admins API error:", response.status, data)
            }
        }
        catch (err) {
            setAdminsError("unable to load administrators")
            setAdmins([])
            console.log("[ADMIN] admins API exception:", err)
        }
        finally {
            setAdminsLoading(false)
        }
    }

    async function fetchDashboard() {
        try {
            const { response, data } = await apiRequestRaw("/api/admin/dashboard")
            if (response.status === 401) {
                navigate("/admin/login")
                return
            }
            if (response.status === 403) {
                navigate("/admin/no-permission")
                return
            }
            if (data?.success) {
                setMetrics(data.metrics)
            }
            else {
                console.log("[ADMIN] dashboard API error:", response.status, data)
            }
        }
        catch (err) {
            console.log("[ADMIN] dashboard API exception:", err)
        }
        finally {
            setLoading(false)
        }
    }

    async function fetchFeedback() {
        try {
            const data = await apiRequest("/api/admin/feedback?limit=10")
            if (data.success) setFeedback(data.feedback)
        }
        catch (err) {
            console.log("[ADMIN] feedback API error:", err)
        }
    }

    async function handleRefresh() {
        setRefreshing(true)
        try {
            const data = await apiRequest("/api/admin/dashboard/refresh", { method: "POST" })
            if (data.success) setMetrics(data.metrics)
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

    function formatDate(d) {
        if (!d) return "—"
        return new Date(d).toLocaleDateString("en-US", {
            year: "numeric",
            month: "short",
            day: "numeric"
        })
    }

    if (loading && !metrics) {
        return (
            <div className="adminPage">
                <p>loading dashboard...</p>
            </div>
        )
    }

    const M = metrics || {}

    function label(value) {
        return String(value || "—").replaceAll("_", " ")
    }

    function renderList(items, empty = "—") {
        if (!Array.isArray(items) || items.length === 0) return empty
        return items.map((item, i) => (
            <span key={`${item.name}-${i}`}>
                {label(item.name)}: {item.count}
            </span>
        ))
    }

    function card(label, value) {
        if (typeof value === "object" && value !== null && !Array.isArray(value)) {
            value = value.today ?? value.allTime ?? "—"
        }
        return (
            <div className="adminCard">
                <span>{label}</span>
                <span className="adminValue">{value ?? "—"}</span>
            </div>
        )
    }

    function cardObj(label, obj, field = "today") {
        const val = obj ? (obj[field] ?? obj.allTime ?? 0) : 0
        return card(label, val)
    }

    return (
        <ErrorBoundary>
        <Layout>
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

            <p className="adminSubtitle"> Admin... more like </p>

            {error && <p className="adminError">{error}</p>}

            {showDebug && (
                <div className="adminDebugBanner">
                    <DebugPanel />
                </div>
            )}

            <div className="adminGrid">

                {/* Tier S - Growth */}
                <div className="adminSection">
                    <h2>growth (tier s)</h2>
                    {cardObj("visitors today", M.site_visitors_today)}
                    {cardObj("visitors 7d", M.site_visitors_7d)}
                    {cardObj("visitors 30d", M.site_visitors_30d)}
                    {cardObj("visitors all time", M.site_visitors_all, "allTime")}
                    {cardObj("downloads today", M.downloads_today)}
                    {cardObj("downloads all time", M.downloads_all, "allTime")}
                    {cardObj("accounts today", M.accounts_created_today)}
                    {cardObj("accounts 7d", M.accounts_created_7d)}
                    {cardObj("accounts 30d", M.accounts_created_30d)}
                    {cardObj("accounts all time", M.accounts_created_all, "allTime")}
                    {card("total users", M.total_users)}
                    {card("active sessions", M.active_sessions)}
                </div>

                {/* Tier S - Activity */}
                <div className="adminSection">
                    <h2>activity (tier s)</h2>
                    {cardObj("DAU", M.dau)}
                    {cardObj("WAU", M.wau)}
                    {cardObj("MAU", M.mau)}
                    {cardObj("unique players today", M.unique_players_today)}
                    {cardObj("game opens today", M.game_opens_today)}
                    {cardObj("game sessions today", M.game_sessions_today)}
                    {cardObj("game sessions all time", M.game_sessions_all, "allTime")}
                </div>

                {/* Tier S - Engagement */}
                <div className="adminSection">
                    <h2>engagement (tier s)</h2>
                    {cardObj("sessions > 1m", M.sessions_gt_1m)}
                    {cardObj("sessions > 5m", M.sessions_gt_5m)}
                    {cardObj("sessions > 10m", M.sessions_gt_10m)}
                    {cardObj("sessions > 30m", M.sessions_gt_30m)}
                    {cardObj("sessions > 1h", M.sessions_gt_1h)}
                    {cardObj("avg session length", M.avg_session_length_seconds, "today")}
                    <div className="adminCard"><span>retention 1d</span><span className="adminValue">{M.retention_1d?.today ?? 0}%</span></div>
                    <div className="adminCard"><span>retention 7d</span><span className="adminValue">{M.retention_7d?.today ?? 0}%</span></div>
                    <div className="adminCard"><span>retention 30d</span><span className="adminValue">{M.retention_30d?.today ?? 0}%</span></div>
                </div>

                {/* Tier S - Revenue */}
                <div className="adminSection">
                    <h2>revenue (tier s)</h2>
                    {cardObj("revenue today", M.revenue_today)}
                    {cardObj("revenue 30d", M.revenue_30d)}
                    {cardObj("revenue all time", M.revenue_all, "allTime")}
                    {cardObj("feedback today", M.feedback_today)}
                    {cardObj("crashes today", M.crashes_today)}
                </div>

                {/* Server Status */}
                <div className="adminSection">
                    <h2>server (tier s)</h2>
                    {card("server online", M.server_online ?? "—")}
                    {card("response time", M.server_response_time ? `${M.server_response_time}ms` : "—")}
                    {card("latest build", M.latest_build_version || "—")}
                </div>

                {/* Tier A - Maps & Weapons */}
                <div className="adminSection">
                    <h2>maps & weapons (tier a)</h2>
                    <div className="adminCard adminCardList">
                        <span>top maps</span>
                        <span className="adminValueSmall">{renderList(M.top_maps)}</span>
                    </div>
                    <div className="adminCard adminCardList">
                        <span>top weapons</span>
                        <span className="adminValueSmall">{renderList(M.top_weapons)}</span>
                    </div>
                    <div className="adminCard adminCardList">
                        <span>movement stats</span>
                        <span className="adminValueSmall">{renderList(M.movement_stats)}</span>
                    </div>
                </div>

                {/* Tier A - Community */}
                <div className="adminSection">
                    <h2>community (tier a)</h2>
                    {cardObj("replays saved", M.replays_saved, "allTime")}
                    {cardObj("replays shared", M.replays_shared, "allTime")}
                    {cardObj("friend requests", M.friend_requests_total, "allTime")}
                    {cardObj("friend requests today", M.friend_requests_today)}
                    {cardObj("discord joins", M.discord_joins, "allTime")}
                    {cardObj("discord joins today", M.discord_joins_today)}
                </div>

                {/* Tier A - Quality */}
                <div className="adminSection">
                    <h2>quality (tier a)</h2>
                    {cardObj("avg FPS", M.avg_fps_value)}
                    {cardObj("avg ping", M.avg_ping_value)}
                    {card("donation conversion %", M.donation_conversion_rate != null ? `${M.donation_conversion_rate}%` : "—")}
                    {card("most common feedback", M.most_common_feedback || "—")}
                    {card("most common page", M.most_common_page || "—")}
                    <div className="adminCard adminCardList">
                        <span>top referrers</span>
                        <span className="adminValueSmall">{renderList(M.top_referrers)}</span>
                    </div>
                    <div className="adminCard adminCardList">
                        <span>device breakdown</span>
                        <span className="adminValueSmall">{renderList(M.device_breakdown)}</span>
                    </div>
                    <div className="adminCard adminCardList">
                        <span>browser breakdown</span>
                        <span className="adminValueSmall">{renderList(M.browser_breakdown)}</span>
                    </div>
                    <div className="adminCard adminCardList">
                        <span>country breakdown</span>
                        <span className="adminValueSmall">{renderList(M.country_breakdown)}</span>
                    </div>
                </div>

                {/* Feedback Analytics */}
                <div className="adminSection">
                    <h2>feedback</h2>
                    {card("total feedback", M.total_feedback)}
                    {card("feedback today", M.feedback_today)}
                    {card("most common preset", M.most_common_preset || "—")}
                    {card("most common page (feedback)", M.most_common_page || "—")}
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

                {/* Administrators List */}
                <div className="adminSection adminSectionWide">
                    <h2>administrators</h2>
                    {adminsLoading ? (
                        <p className="adminEmpty">loading...</p>
                    ) : adminsError ? (
                        <div className="adminAdminsError">
                            <p className="adminError">{adminsError}</p>
                            <button className="adminRefreshBtn" onClick={fetchAdmins}>retry</button>
                        </div>
                    ) : !Array.isArray(admins) || admins.length === 0 ? (
                        <p className="adminEmpty">no administrators found</p>
                    ) : (
                        <>
                            <input
                                className="adminSearchInput"
                                type="text"
                                placeholder="search by username or email..."
                                value={adminsSearch}
                                onChange={e => setAdminsSearch(e.target.value)}
                            />
                            <div className="adminAdminsList">
                                {admins
                                    .filter(a => {
                                        if (!adminsSearch) return true
                                        const q = adminsSearch.toLowerCase()
                                        return a.username.toLowerCase().includes(q) ||
                                               a.email.toLowerCase().includes(q)
                                    })
                                    .map(a => (
                                        <a
                                            key={a.id}
                                            className="adminAdminsRow"
                                            href={`/users/${encodeURIComponent(a.username)}`}
                                            onClick={e => {
                                                e.preventDefault()
                                                window.open(`/users/${encodeURIComponent(a.username)}`, "_blank")
                                            }}
                                        >
                                            <span className="adminAdminsAvatar">
                                                {a.avatar_url ? (
                                                    <img src={a.avatar_url} alt="" className="adminAdminsAvatarImg" />
                                                ) : (
                                                    <span className="adminAdminsAvatarPlaceholder">
                                                        {a.username[0]?.toUpperCase() || "?"}
                                                    </span>
                                                )}
                                            </span>
                                            <span className="adminAdminsUsername">{a.username}</span>
                                            <span className="adminAdminsEmail">{a.email}</span>
                                            <span className="adminAdminsRole">{a.role}</span>
                                            <span className="adminAdminsVerified">
                                                {a.email_verified_at ? "Yes" : "No"}
                                            </span>
                                            <span className="adminAdminsCreated">
                                                {formatDate(a.created_at)}
                                            </span>
                                        </a>
                                    ))}
                            </div>
                        </>
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
                                    <span className="adminFeedbackUser">
                                        {f.avatar_url ? (
                                            <img src={f.avatar_url} alt="" className="adminFeedbackAvatar" />
                                        ) : (
                                            <span className="adminFeedbackAvatarPlaceholder">
                                                {(f.username || "?")[0].toUpperCase()}
                                            </span>
                                        )}
                                        <span className="adminFeedbackUsername">{f.username || "guest"}</span>
                                    </span>
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
        </Layout>
        </ErrorBoundary>
    )
}