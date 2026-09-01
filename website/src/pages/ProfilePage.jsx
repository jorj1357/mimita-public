import { useEffect, useState } from "react"
import { Link, useNavigate } from "react-router-dom"
import Layout from "../components/Layout"
import Avatar from "../components/Avatar"
import Username from "../components/Username"
import NameStyleEditor from "../components/NameStyleEditor"
import { normalizeStyle } from "../lib/vipStyle"
import { apiRequest } from "../lib/api"

export default function ProfilePage() {
    const navigate = useNavigate()
    const [user, setUser] = useState(null)
    const [vip, setVip] = useState(null)
    const [config, setConfig] = useState(null)
    const [style, setStyle] = useState(normalizeStyle(null))
    const [busy, setBusy] = useState("")
    const [message, setMessage] = useState("")
    const [loading, setLoading] = useState(true)
    const [gameStats, setGameStats] = useState(null)

    useEffect(() => {
        let alive = true
        let statsTimer = null
        Promise.all([
            apiRequest("/api/auth/me"),
            apiRequest("/api/vip/me"),
            apiRequest("/api/vip/config")
        ])
            .then(([me, vipMe, cfg]) => {
                if (!alive) return
                setUser(me.user)
                setVip(vipMe.vip)
                setConfig(cfg.config)
                setStyle(normalizeStyle(vipMe.vip?.name_style))
                setLoading(false)
                if (me.user?.id > 0) {
                    const refreshStats = () => {
                        apiRequest(`/api/profile/${me.user.id}`)
                            .then(data => {
                                if (alive && data.profile)
                                    setGameStats(data.profile)
                            })
                            .catch(() => {})
                    }
                    // The game batches persistence roughly once per minute;
                    // refresh at the same low rate so the profile catches up
                    // without creating load on the API.
                    refreshStats()
                    statsTimer = setInterval(refreshStats, 60 * 1000)
                }
            })
            .catch(() => {
                if (alive) navigate("/signin")
            })
        return () => {
            alive = false
            if (statsTimer) clearInterval(statsTimer)
        }
    }, [navigate])

    async function saveStyle() {
        setBusy("save-style")
        setMessage("")
        try {
            const data = await apiRequest("/api/vip/style", {
                method: "PATCH",
                body: JSON.stringify({ style })
            })
            setVip(data.vip)
            setStyle(normalizeStyle(data.vip?.name_style))
            setMessage("style saved")
        }
        catch (error) {
            setMessage(error.message)
        }
        finally {
            setBusy("")
        }
    }

    async function resetStyle() {
        setBusy("reset-style")
        setMessage("")
        try {
            const data = await apiRequest("/api/vip/style/reset", { method: "POST" })
            setVip(data.vip)
            setStyle(normalizeStyle(data.vip?.name_style))
            setMessage("style reset")
        }
        catch (error) {
            setMessage(error.message)
        }
        finally {
            setBusy("")
        }
    }

    function formatDate(dateStr) {
        if (!dateStr) return "Unknown"
        const d = new Date(dateStr)
        const mm = String(d.getMonth() + 1).padStart(2, "0")
        const dd = String(d.getDate()).padStart(2, "0")
        const yyyy = d.getFullYear()
        return `${mm}-${dd}-${yyyy}`
    }

    function formatRole(role) {
        if (!role || role === "user") return "Member"
        return role.charAt(0).toUpperCase() + role.slice(1)
    }

    if (loading) {
        return (
            <Layout>
                <div className="profilePageContainer">
                    <p className="profileLoading">Loading profile...</p>
                </div>
            </Layout>
        )
    }

    if (!user) return null

    const activeTier = vip?.active_tier || user.supporter_tier

    return (
        <Layout>
            <div className="profilePageContainer">
                <div className="profilePageCard">
                    <div className="profilePageHeader">
                        <Avatar user={user} size="lg" />
                        <div className="profilePageInfo">
                            <Username user={{ ...user, vip }} size="lg" />
                            <p className="profilePageRole">{formatRole(user.role)}</p>
                            {user.bio && <p className="profilePageBio">{user.bio}</p>}
                        </div>
                    </div>

                    <div className="profilePageMeta">
                        <div className="profilePageMetaItem">
                            <span className="profilePageMetaLabel">Joined</span>
                            <span className="profilePageMetaValue">{formatDate(user.created_at)}</span>
                        </div>
                        {activeTier && activeTier !== "free" && (
                            <div className="profilePageMetaItem">
                                <span className="profilePageMetaLabel">Tier</span>
                                <span className="profilePageMetaValue">{activeTier.replace("_", " ")}</span>
                            </div>
                        )}
                        {user.email_visible && (
                            <div className="profilePageMetaItem">
                                <span className="profilePageMetaLabel">Email</span>
                                <span className="profilePageMetaValue">{user.email}</span>
                            </div>
                        )}
                    </div>

                    <Link to="/account" className="profilePageEditBtn">
                        Edit Profile
                    </Link>
                </div>

                <div className="profilePageSection">
                    <h2 className="profilePageSectionTitle">VIP Name Style</h2>
                    <div className="profileVipEditor">
                        {message && <p className="vipNotice">{message}</p>}
                        {activeTier === "free" ? (
                            <div className="profilePageEmpty">
                                <p>You don't have VIP yet, but you can still preview the name styles below.</p>
                                <p><Link to="/vip">get VIP to unlock custom name colors</Link></p>
                            </div>
                        ) : (
                            <p className="vipNotice">This is how your name looks right now. Changes save live.</p>
                        )}
                        <NameStyleEditor
                            user={user}
                            vip={vip}
                            style={style}
                            onChange={setStyle}
                            onSave={saveStyle}
                            onReset={resetStyle}
                            busy={busy}
                            limits={config?.style_limits}
                        />
                    </div>
                </div>

                <div className="profilePageSection">
                    <h2 className="profilePageSectionTitle">Recent Activity</h2>
                    <div className="profilePageEmpty">
                        <p>No recent activity to show.</p>
                    </div>
                </div>

                <div className="profilePageGrid">
                    <div className="profilePageSection">
                        <h2 className="profilePageSectionTitle">Statistics</h2>
                        {gameStats ? (
                            <div className="profileStats">
                                <div className="profileStatRow">
                                    <span className="profileStatLabel">Level</span>
                                    <span className="profileStatValue" style={{ color: "#00d9d9" }}>{gameStats.level}</span>
                                </div>
                                <div className="profileStatRow">
                                    <span className="profileStatLabel">Total XP</span>
                                    <span className="profileStatValue" style={{ color: "#00d9d9" }}>{(gameStats.totalXp || 0).toLocaleString()}</span>
                                </div>
                                <div className="profileStatRow">
                                    <span className="profileStatLabel">Gold</span>
                                    <span className="profileStatValue" style={{ color: "#ffd900" }}>{(gameStats.gold || 0).toLocaleString()}</span>
                                </div>
                                <div className="profileStatDivider" />
                                <div className="profileStatRow">
                                    <span className="profileStatLabel">Player Kills</span>
                                    <span className="profileStatValue">{(gameStats.playerKills || 0).toLocaleString()}</span>
                                </div>
                                <div className="profileStatRow">
                                    <span className="profileStatLabel">NPC Kills</span>
                                    <span className="profileStatValue">{(gameStats.npcKills || 0).toLocaleString()}</span>
                                </div>
                                <div className="profileStatRow">
                                    <span className="profileStatLabel">Deaths</span>
                                    <span className="profileStatValue">{(gameStats.deaths || 0).toLocaleString()}</span>
                                </div>
                                <div className="profileStatDivider" />
                                <div className="profileStatRow">
                                    <span className="profileStatLabel">Matches</span>
                                    <span className="profileStatValue">{(gameStats.matchesPlayed || 0).toLocaleString()}</span>
                                </div>
                                <div className="profileStatRow">
                                    <span className="profileStatLabel">Wins</span>
                                    <span className="profileStatValue" style={{ color: "#4caf50" }}>{(gameStats.wins || 0).toLocaleString()}</span>
                                </div>
                                <div className="profileStatRow">
                                    <span className="profileStatLabel">Losses</span>
                                    <span className="profileStatValue" style={{ color: "#f44336" }}>{(gameStats.losses || 0).toLocaleString()}</span>
                                </div>
                                <div className="profileStatDivider" />
                                <div className="profileStatRow">
                                    <span className="profileStatLabel">FFA Played</span>
                                    <span className="profileStatValue">{(gameStats.ffa?.played || 0).toLocaleString()}</span>
                                </div>
                                <div className="profileStatRow">
                                    <span className="profileStatLabel">FFA Wins</span>
                                    <span className="profileStatValue" style={{ color: "#4caf50" }}>{(gameStats.ffa?.wins || 0).toLocaleString()}</span>
                                </div>
                                <div className="profileStatRow">
                                    <span className="profileStatLabel">TDM Played</span>
                                    <span className="profileStatValue">{(gameStats.tdm?.played || 0).toLocaleString()}</span>
                                </div>
                                <div className="profileStatRow">
                                    <span className="profileStatLabel">TDM Wins</span>
                                    <span className="profileStatValue" style={{ color: "#4caf50" }}>{(gameStats.tdm?.wins || 0).toLocaleString()}</span>
                                </div>
                            </div>
                        ) : (
                            <div className="profilePageEmpty">
                                <p>No statistics available yet.</p>
                            </div>
                        )}
                    </div>

                    <div className="profilePageSection">
                        <h2 className="profilePageSectionTitle">Achievements</h2>
                        {user.achievements && user.achievements.length > 0 ? (
                            <ul className="achievementsList">
                                {user.achievements.map((ach) => (
                                    <li key={ach} className="achievementItem">
                                        {ach === "confirmed_email" ? "✅ Confirmed Email" : ach}
                                    </li>
                                ))}
                            </ul>
                        ) : (
                            <div className="profilePageEmpty">
                                <p>No achievements earned yet.</p>
                            </div>
                        )}
                    </div>
                </div>
            </div>
        </Layout>
    )
}
