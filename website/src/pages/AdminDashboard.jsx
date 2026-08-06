import { useEffect, useState, useRef } from "react"
import { useNavigate, Link } from "react-router-dom"
import { apiRequest, apiRequestRaw } from "../lib/api.js"
import DebugPanel from "../components/DebugPanel.jsx"
import ErrorBoundary from "../components/ErrorBoundary.jsx"
import Layout from "../components/Layout"
import NameStyleEditor from "../components/NameStyleEditor"
import { normalizeStyle } from "../lib/vipStyle"

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
    const [flagged, setFlagged] = useState([])
    const [errorLog, setErrorLog] = useState([])
    const [errorLogLoading, setErrorLogLoading] = useState(true)
    const [vipSearch, setVipSearch] = useState("")
    const [vipLookup, setVipLookup] = useState(null)
    const [vipStyle, setVipStyle] = useState(normalizeStyle(null))
    const [grantTier, setGrantTier] = useState("vip")
    const [grantMonths, setGrantMonths] = useState("1")
    const [vipBusy, setVipBusy] = useState("")
    const [vipMsg, setVipMsg] = useState("")
    const fetchedRef = useRef(false)

    useEffect(() => {
        if (fetchedRef.current) return
        fetchedRef.current = true
        fetchDashboard()
        fetchFeedback()
        fetchAdmins()
        fetchFlagged()
        fetchErrorLog()
    }, [])

    async function fetchFlagged() {
        try {
            const data = await apiRequest("/api/admin/flagged-accounts")
            if (data.success) setFlagged(data.flagged)
        } catch (e) { console.log("[ADMIN] fetch flagged error:", e.message) }
    }

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

    async function fetchErrorLog() {
        try {
            const data = await apiRequest("/api/admin/error-log?limit=30")
            if (data.success) setErrorLog(data.errors || [])
        } catch (e) { console.log("[ADMIN] error-log fetch:", e.message) }
        finally { setErrorLogLoading(false) }
    }

    async function lookupVipUser() {
        const queryText = vipSearch.trim()
        if (!queryText) {
            setVipMsg("enter a username, email, or user id")
            return
        }
        setVipBusy("lookup")
        setVipMsg("")
        try {
            const data = await apiRequest(`/api/admin/vip/lookup?query=${encodeURIComponent(queryText)}`)
            setVipLookup(data)
            setVipStyle(normalizeStyle(data.state?.name_style))
        }
        catch (error) {
            setVipLookup(null)
            setVipMsg(error.message)
        }
        finally {
            setVipBusy("")
        }
    }

    async function refreshVipLookup() {
        if (!vipLookup) return
        try {
            const data = await apiRequest(`/api/admin/vip/lookup?query=${encodeURIComponent(vipLookup.user.username)}`)
            setVipLookup(data)
            setVipStyle(normalizeStyle(data.state?.name_style))
        }
        catch (error) {
            setVipMsg(error.message)
        }
    }

    async function adminGrant() {
        if (!vipLookup) return
        setVipBusy("grant")
        setVipMsg("")
        try {
            await apiRequest("/api/admin/vip/grant", {
                method: "POST",
                body: JSON.stringify({
                    user_id: vipLookup.user.id,
                    tier: grantTier,
                    months: Number(grantMonths) || 1
                })
            })
            setVipMsg(`granted ${grantTier} for ${grantMonths} month(s)`)
            await refreshVipLookup()
        }
        catch (error) {
            setVipMsg(error.message)
        }
        finally {
            setVipBusy("")
        }
    }

    async function adminRevoke() {
        if (!vipLookup) return
        if (!window.confirm(`Revoke ALL VIP from ${vipLookup.user.username}? This overrides any active subscription entitlement.`)) return
        setVipBusy("revoke")
        setVipMsg("")
        try {
            await apiRequest("/api/admin/vip/revoke", {
                method: "POST",
                body: JSON.stringify({ user_id: vipLookup.user.id })
            })
            setVipMsg("revoked")
            await refreshVipLookup()
        }
        catch (error) {
            setVipMsg(error.message)
        }
        finally {
            setVipBusy("")
        }
    }

    async function adminSaveStyle() {
        if (!vipLookup) return
        setVipBusy("save-style")
        setVipMsg("")
        try {
            const data = await apiRequest("/api/admin/vip/style", {
                method: "POST",
                body: JSON.stringify({ user_id: vipLookup.user.id, style: vipStyle })
            })
            setVipMsg("style saved")
            setVipLookup(current => current ? { ...current, state: data.vip } : current)
        }
        catch (error) {
            setVipMsg(error.message)
        }
        finally {
            setVipBusy("")
        }
    }

    async function adminResetStyle() {
        if (!vipLookup || !vipLookup.state?.default_style) return
        setVipBusy("reset-style")
        setVipMsg("")
        try {
            const data = await apiRequest("/api/admin/vip/style", {
                method: "POST",
                body: JSON.stringify({ user_id: vipLookup.user.id, style: vipLookup.state.default_style })
            })
            setVipStyle(normalizeStyle(data.vip?.name_style))
            setVipMsg("style reset")
            setVipLookup(current => current ? { ...current, state: data.vip } : current)
        }
        catch (error) {
            setVipMsg(error.message)
        }
        finally {
            setVipBusy("")
        }
    }

    async function adminResync() {
        if (!vipLookup) return
        setVipBusy("resync")
        setVipMsg("")
        try {
            const data = await apiRequest("/api/admin/vip/resync", {
                method: "POST",
                body: JSON.stringify({ user_id: vipLookup.user.id })
            })
            setVipMsg(`resync done (${data.created || 0} entitlement(s) created)`)
            await refreshVipLookup()
        }
        catch (error) {
            setVipMsg(error.message)
        }
        finally {
            setVipBusy("")
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

                {/* VIP Management */}
                <div className="adminSection adminSectionWide">
                    <h2>VIP Management</h2>
                    <div className="adminVipSearch">
                        <input
                            className="adminSearchInput"
                            type="text"
                            placeholder="username, email, or user id..."
                            value={vipSearch}
                            onChange={e => setVipSearch(e.target.value)}
                            onKeyDown={e => { if (e.key === "Enter") lookupVipUser() }}
                        />
                        <button className="adminButton" onClick={lookupVipUser} disabled={vipBusy === "lookup"}>
                            search
                        </button>
                    </div>

                    {vipMsg && <p className="adminError">{vipMsg}</p>}

                    {vipLookup && (
                        <div className="adminVipCard">
                            <div className="adminVipUserRow">
                                {vipLookup.user.avatar_url ? (
                                    <img src={vipLookup.user.avatar_url} alt="" className="adminAdminsAvatarImg" />
                                ) : (
                                    <span className="adminAdminsAvatarPlaceholder">
                                        {(vipLookup.user.username || "?")[0].toUpperCase()}
                                    </span>
                                )}
                                <div className="adminVipUserInfo">
                                    <strong className="adminVipUsername">{vipLookup.user.username}</strong>
                                    <span className="adminVipRole">{vipLookup.user.role}</span>
                                </div>
                                <span className="adminVipUserEmail">{vipLookup.user.email}</span>
                            </div>

                            {vipLookup.desync && (
                                <div className="adminVipHugeFlag">
                                    <p>
                                        ⚠️ THIS USER HAS AN ACTIVE SUBSCRIPTION BUT ISN'T GETTING VIP!
                                    </p>
                                    <p>
                                        They're paying for <strong>{vipLookup.subscription_tier}</strong> but showing{" "}
                                        <strong>{vipLookup.state.active_tier}</strong>. This is a desync.
                                    </p>
                                    <button
                                        type="button"
                                        onClick={adminResync}
                                        disabled={vipBusy === "resync"}
                                    >
                                        RESYNC THEIR VIP NOW
                                    </button>
                                </div>
                            )}

                            {!vipLookup.desync && vipLookup.has_active_subscription && (
                                <div className="adminVipFlag">
                                    This user has an active subscription (
                                    {vipLookup.subscription_tier}). Manual grant/revoke overrides it.
                                </div>
                            )}

                            <div className="adminVipState">
                                <span>tier: <strong>{vipLookup.state.active_tier}</strong></span>
                                <span>expires: <strong>{vipLookup.state.expires_at ? new Date(vipLookup.state.expires_at).toLocaleString() : "none"}</strong></span>
                                <span>subscription: <strong>{vipLookup.state.subscription?.status || "none"}</strong></span>
                            </div>

                            <div className="adminVipActions">
                                <div className="adminVipGrantRow">
                                    <select value={grantTier} onChange={e => setGrantTier(e.target.value)}>
                                        <option value="vip">VIP</option>
                                        <option value="super_vip">Super VIP</option>
                                        <option value="ultra_vip">Ultra VIP</option>
                                    </select>
                                    <input
                                        type="number"
                                        min="1"
                                        max="24"
                                        value={grantMonths}
                                        onChange={e => setGrantMonths(e.target.value)}
                                    />
                                    <button
                                        type="button"
                                        onClick={adminGrant}
                                        disabled={vipBusy === "grant"}
                                    >
                                        grant VIP (months)
                                    </button>
                                    <button
                                        type="button"
                                        className="adminVipRevokeBtn"
                                        onClick={adminRevoke}
                                        disabled={vipBusy === "revoke"}
                                    >
                                        revoke VIP
                                    </button>
                                </div>
                            </div>

                            <div className="adminVipStyle">
                                <h3>set name style</h3>
                                <NameStyleEditor
                                    admin
                                    user={{ id: vipLookup.user.id, username: vipLookup.user.username, role: vipLookup.user.role, vip: vipLookup.state }}
                                    vip={vipLookup.state}
                                    style={vipStyle}
                                    onChange={setVipStyle}
                                    onSave={adminSaveStyle}
                                    onReset={adminResetStyle}
                                    busy={vipBusy}
                                />
                            </div>
                        </div>
                    )}
                </div>

                {/* Tier S - Growth */}
                <div className="adminSection">
                    <h2>growth (tier s)</h2>
                    {cardObj("page loads today", M.page_loads_today)}
                    {cardObj("page loads 7d", M.page_loads_7d)}
                    {cardObj("page loads 30d", M.page_loads_30d)}
                    {cardObj("page loads all time", M.page_loads_all, "allTime")}
                    {cardObj("unique visitors today", M.unique_visitors_today)}
                    {cardObj("unique visitors 7d", M.unique_visitors_7d)}
                    {cardObj("unique visitors 30d", M.unique_visitors_30d)}
                    {cardObj("unique visitors all time", M.unique_visitors_all, "allTime")}
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

                {/* Flagged Accounts */}
                <div className="adminSection">
                    <h2>flagged accounts</h2>
                    {flagged.length === 0 ? (
                        <p className="adminEmpty">no flagged accounts</p>
                    ) : (
                        <div className="adminAdminsList">
                            {flagged.map(f => (
                                <div key={f.identifier} className="adminAdminsRow" style={{ cursor: "default", padding: "10px" }}>
                                    <span className="adminAdminsUsername" style={{ flex: 2 }}>{f.identifier}</span>
                                    <span className="adminAdminsEmail" style={{ flex: 1 }}>{f.attempts} failed attempts</span>
                                    <span className="adminAdminsRole" style={{ flex: 1, color: "#f87171" }}>
                                        Locked until: {new Date(
                                            new Date(f.first_attempt).getTime() + 15 * 60 * 1000
                                        ).toLocaleTimeString()}
                                    </span>
                                    <span className="adminAdminsCreated" style={{ flex: 1 }}>
                                        Last: {new Date(f.last_attempt).toLocaleString()}
                                    </span>
                                </div>
                            ))}
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

                {/* Article Editor */}
                <div className="adminSection">
                    <h2>Articles</h2>
                    <div className="adminCard">
                        <p>Write and publish articles in markdown. Supports [rainbow]text[/rainbow] effects.</p>
                        <Link to="/admin/articles" className="adminButton" style={{ display: "inline-block", marginTop: "8px", textDecoration: "none" }}>
                            Open Article Editor
                        </Link>
                    </div>
                </div>

                {/* Email Campaigns */}
                <div className="adminSection">
                    <h2>Email Campaigns</h2>
                    <div className="adminCard">
                        <p>Create, preview, and send email campaigns to registered users.</p>
                        <Link to="/admin/email-campaigns" className="adminButton" style={{ display: "inline-block", marginTop: "8px", textDecoration: "none" }}>
                            Open Email Campaign Dashboard
                        </Link>
                    </div>
                </div>

                {/* Site Banners */}
                <div className="adminSection">
                    <h2>Site Banner</h2>
                    <div className="adminCard">
                        <p>Review every banner ever made, disable or delete them, and place your own.</p>
                        <Link to="/admin/banners" className="adminButton" style={{ display: "inline-block", marginTop: "8px", marginRight: "8px", textDecoration: "none" }}>
                            Open Banner Dashboard
                        </Link>
                        <Link to="/banner/status" className="adminButton" style={{ display: "inline-block", marginTop: "8px", textDecoration: "none" }}>
                            Banner Status
                        </Link>
                    </div>
                </div>

                {/* Support */}
                <div className="adminSection">
                    <h2>Support</h2>
                    <div className="adminCard">
                        <p>Review support requests, filter by topic, and update status.</p>
                        <Link to="/admin/support" className="adminButton" style={{ display: "inline-block", marginTop: "8px", textDecoration: "none" }}>
                            Open Support Dashboard
                        </Link>
                    </div>
                </div>

                {/* Error Log */}
                <div className="adminSection adminSectionWide">
                    <h2>error log</h2>
                    {errorLogLoading ? (
                        <p className="adminEmpty">loading...</p>
                    ) : errorLog.length === 0 ? (
                        <p className="adminEmpty">no errors logged</p>
                    ) : (
                        <div className="adminFeedbackFeed">
                            {errorLog.map(e => (
                                <div key={e.id} className="adminFeedbackItem" style={{ borderLeft: `3px solid ${e.level === "error" ? "#f87171" : "#fbbf24"}` }}>
                                    <span className="adminFeedbackTime" style={{ minWidth: "60px", fontSize: "11px" }}>
                                        {new Date(e.timestamp).toLocaleTimeString([], { hour: "2-digit", minute: "2-digit", second: "2-digit" })}
                                    </span>
                                    <span className="adminFeedbackUser" style={{ minWidth: "80px" }}>
                                        <span className={`adminLogBadge adminLogBadge--${e.category}`}>{e.category}</span>
                                    </span>
                                    <span className="adminFeedbackText" style={{ flex: 1, fontSize: "12px" }}>
                                        {e.message}
                                        {e.path && <span className="adminFeedbackPage">{e.method} {e.path}</span>}
                                    </span>
                                </div>
                            ))}
                        </div>
                    )}
                    <button className="adminRefreshBtn" onClick={fetchErrorLog} style={{ marginTop: "8px" }}>
                        refresh error log
                    </button>
                </div>

            </div>
        </div>
        </Layout>
        </ErrorBoundary>
    )
}