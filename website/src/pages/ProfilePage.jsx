import { useEffect, useState } from "react"
import { Link, useNavigate } from "react-router-dom"
import Layout from "../components/Layout"
import Avatar from "../components/Avatar"
import Username from "../components/Username"
import { apiRequest } from "../lib/api"

export default function ProfilePage() {
    const navigate = useNavigate()
    const [user, setUser] = useState(null)
    const [loading, setLoading] = useState(true)

    useEffect(() => {
        apiRequest("/api/auth/me")
            .then((data) => {
                setUser(data.user)
                setLoading(false)
            })
            .catch(() => {
                navigate("/signin")
            })
    }, [navigate])

    function formatDate(dateStr) {
        if (!dateStr) return "Unknown"
        return new Date(dateStr).toLocaleDateString("en-US", {
            year: "numeric",
            month: "long",
            day: "numeric"
        })
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

    return (
        <Layout>
            <div className="profilePageContainer">
                <div className="profilePageCard">
                    <div className="profilePageHeader">
                        <Avatar user={user} size="lg" />
                        <div className="profilePageInfo">
                            <Username user={user} size="lg" />
                            <p className="profilePageRole">{formatRole(user.role)}</p>
                            {user.bio && <p className="profilePageBio">{user.bio}</p>}
                        </div>
                    </div>

                    <div className="profilePageMeta">
                        <div className="profilePageMetaItem">
                            <span className="profilePageMetaLabel">Joined</span>
                            <span className="profilePageMetaValue">{formatDate(user.created_at)}</span>
                        </div>
                        {user.supporter_tier && user.supporter_tier !== "free" && (
                            <div className="profilePageMetaItem">
                                <span className="profilePageMetaLabel">Tier</span>
                                <span className="profilePageMetaValue">{user.supporter_tier.replace("_", " ")}</span>
                            </div>
                        )}
                        <div className="profilePageMetaItem">
                            <span className="profilePageMetaLabel">Email</span>
                            <span className="profilePageMetaValue">{user.email}</span>
                        </div>
                    </div>

                    <Link to="/account" className="profilePageEditBtn">
                        Edit Profile
                    </Link>
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
                        <div className="profilePageEmpty">
                            <p>No statistics available yet.</p>
                        </div>
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
