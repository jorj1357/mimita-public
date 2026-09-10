// 09 06 2026, 14 43
/* purpose
* Resolve public accounts by ID or username and display their profile.
* Reuse shared persisted statistics and account presentation components.
* DOES NOT save progression or expose private account settings.
*/
import { useEffect, useState } from "react"
import { useParams, Link } from "react-router-dom"

import Layout from "../components/Layout"
import Username from "../components/Username"
import Avatar from "../components/Avatar"
import ProfileStats from "../components/ProfileStats"
import UserOnlineBadge from "../components/UserOnlineBadge"
import ModerationButtons from "../components/ModerationButtons"
import { apiRequest } from "../lib/api"

export default function UserProfile() {
    const { username, id } = useParams()
    const [result, setResult] = useState(null)
    const [currentUser, setCurrentUser] = useState(null)
    const [friendStatus, setFriendStatus] = useState(null)
    const [friendBusy, setFriendBusy] = useState(false)
    const route = id ? `/api/users/id/${encodeURIComponent(id)}` : `/api/users/${encodeURIComponent(username)}`
    const user = result?.route === route ? result.user : null
    const message = result?.route === route ? result.message : "loading profile..."

    useEffect(() => {
        const controller = new AbortController()
        apiRequest(route, { signal: controller.signal })
            .then((data) => {
                if (!data?.user) throw new Error("Profile unavailable")
                if (!controller.signal.aborted) setResult({ route, user: data.user, message: "" })
            })
            .catch((error) => {
                if (!controller.signal.aborted) setResult({ route, user: null, message: error.message })
            })
        return () => controller.abort()
    }, [route])

    useEffect(() => {
        apiRequest("/api/auth/me")
            .then(data => {
                if (data?.success) setCurrentUser(data.user)
            })
            .catch(() => {})
    }, [])

    useEffect(() => {
        if (!user || !currentUser || user.id === currentUser.id) return
        apiRequest(`/api/friends/status/${user.id}`)
            .then(data => {
                if (data?.success) setFriendStatus(data)
            })
            .catch(() => {})
    }, [user, currentUser])

    async function handleFriendAction() {
        if (!user || friendBusy) return
        setFriendBusy(true)
        try {
            if (friendStatus?.status === "none") {
                const data = await apiRequest(`/api/friends/request/${user.id}`, { method: "POST" })
                if (data?.success) {
                    setFriendStatus({ status: "pending", direction: "outgoing" })
                }
            } else if (friendStatus?.status === "pending" && friendStatus?.direction === "incoming") {
                const data = await apiRequest(`/api/friends/accept/${user.id}`, { method: "POST" })
                if (data?.success) {
                    setFriendStatus({ status: "accepted" })
                }
            } else if (friendStatus?.status === "accepted") {
                await apiRequest(`/api/friends/${user.id}`, { method: "DELETE" })
                setFriendStatus({ status: "none" })
            }
        } catch {}
        setFriendBusy(false)
    }

    function getFriendButtonLabel() {
        if (!friendStatus || friendStatus.status === "none") return "Add Friend"
        if (friendStatus.status === "pending" && friendStatus.direction === "outgoing") return "Request Sent"
        if (friendStatus.status === "pending" && friendStatus.direction === "incoming") return "Accept Request"
        if (friendStatus.status === "accepted") return "Remove Friend"
        return "Add Friend"
    }

    function formatDate(dateStr) {
        if (!dateStr) return "Unknown"
        const d = new Date(dateStr)
        const mm = String(d.getMonth() + 1).padStart(2, "0")
        const dd = String(d.getDate()).padStart(2, "0")
        const yyyy = d.getFullYear()
        return `${mm}-${dd}-${yyyy}`
    }

    const isOwnProfile = currentUser && user && currentUser.id === user.id

    return (
        <Layout>
            <section className="profilePage">
                {user ? (
                    <div className="profileCard">
                        <Avatar user={user} size="lg" />

                        <h1 className="profileUsername">
                            <Username user={user} size="lg" />
                            <UserOnlineBadge lastSeenAt={user.last_seen_at} size="md" />
                        </h1>

                        {user.bio && <p className="profileBio">{user.bio}</p>}

                        <p className="profileJoined">
                            Joined {formatDate(user.created_at)}
                        </p>

                        {!isOwnProfile && currentUser && (
                            <div className="profileActions">
                                <button
                                    className={`friendActionBtn ${friendStatus?.status === "accepted" ? "isFriend" : ""}`}
                                    onClick={handleFriendAction}
                                    disabled={friendBusy}
                                >
                                    {friendBusy ? "..." : getFriendButtonLabel()}
                                </button>
                                <Link to="/messages" className="messageActionBtn"
                                    onClick={async (e) => {
                                        e.preventDefault()
                                        try {
                                            const data = await apiRequest("/api/messages/conversations/dm", {
                                                method: "POST",
                                                body: JSON.stringify({ userId: user.id })
                                            })
                                            if (data?.success) {
                                                window.location.href = "/messages"
                                            }
                                        } catch {}
                                    }}
                                >
                                    Message
                                </Link>
                            </div>
                        )}

                        {!isOwnProfile && currentUser && (
                            <ModerationButtons userId={user.id} username={user.username} />
                        )}

                        <div className="profilePageSection">
                            <h2 className="profilePageSectionTitle">Statistics</h2>
                            <ProfileStats key={user.id} userId={user.id} />
                        </div>

                        {user.achievements && user.achievements.length > 0 && (
                            <div className="profileAchievements">
                                <h3>Achievements</h3>
                                <ul className="achievementsList">
                                    {user.achievements.map((ach) => (
                                        <li key={ach} className="achievementItem">
                                            {ach === "confirmed_email" ? "✅ Confirmed Email" : ach}
                                        </li>
                                    ))}
                                </ul>
                            </div>
                        )}
                    </div>
                ) : (
                    <p>{message}</p>
                )}
            </section>
        </Layout>
    )
}
