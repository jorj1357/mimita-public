import { useEffect, useState } from "react"
import { Link } from "react-router-dom"
import Layout from "../components/Layout"
import Avatar from "../components/Avatar"
import Username from "../components/Username"
import { apiRequest } from "../lib/api"

export default function Friends() {
    const [friends, setFriends] = useState([])
    const [requests, setRequests] = useState([])
    const [sent, setSent] = useState([])
    const [loading, setLoading] = useState(true)
    const [tab, setTab] = useState("friends")

    useEffect(() => {
        Promise.all([
            apiRequest("/api/friends"),
            apiRequest("/api/friends/requests"),
            apiRequest("/api/friends/sent")
        ])
            .then(([f, r, s]) => {
                if (f?.success) setFriends(f.friends || [])
                if (r?.success) setRequests(r.requests || [])
                if (s?.success) setSent(s.sent || [])
            })
            .catch(() => {})
            .finally(() => setLoading(false))
    }, [])

    async function handleAccept(friendshipId) {
        try {
            await apiRequest(`/api/friends/accept/${friendshipId}`, { method: "POST" })
            setRequests(prev => prev.filter(r => r.id !== friendshipId))
        } catch {}
    }

    async function handleReject(friendshipId) {
        try {
            await apiRequest(`/api/friends/reject/${friendshipId}`, { method: "POST" })
            setRequests(prev => prev.filter(r => r.id !== friendshipId))
        } catch {}
    }

    async function handleRemove(userId) {
        try {
            await apiRequest(`/api/friends/${userId}`, { method: "DELETE" })
            setFriends(prev => prev.filter(f => f.id !== userId))
        } catch {}
    }

    function formatNumber(n) {
        if (n == null) return "0"
        return Number(n).toLocaleString()
    }

    return (
        <Layout>
            <section className="friendsPage">
                <h1 className="pageHeading">Friends</h1>

                <div className="friendsTabs">
                    <button className={`tab ${tab === "friends" ? "active" : ""}`} onClick={() => setTab("friends")}>
                        Friends ({friends.length})
                    </button>
                    <button className={`tab ${tab === "requests" ? "active" : ""}`} onClick={() => setTab("requests")}>
                        Requests ({requests.length})
                    </button>
                    <button className={`tab ${tab === "sent" ? "active" : ""}`} onClick={() => setTab("sent")}>
                        Sent ({sent.length})
                    </button>
                </div>

                {loading ? (
                    <p>Loading...</p>
                ) : tab === "friends" ? (
                    friends.length === 0 ? (
                        <p className="friendsEmpty">No friends yet. Visit a user's profile to add them!</p>
                    ) : (
                        <div className="friendsList">
                            {friends.map(friend => (
                                <div key={friend.id} className="friendCard">
                                    <Link to={`/u/${friend.username}`} className="friendAvatar">
                                        <Avatar user={friend} size="md" />
                                    </Link>
                                    <div className="friendInfo">
                                        <Link to={`/u/${friend.username}`}>
                                            <Username user={friend} size="md" />
                                        </Link>
                                        <div className="friendStats">
                                            <span>{formatNumber(friend.lifetime_player_kills)} kills</span>
                                            <span>{formatNumber(friend.lifetime_deaths)} deaths</span>
                                            <span>{formatNumber(friend.total_xp)} XP</span>
                                        </div>
                                    </div>
                                    <button className="friendRemoveBtn" onClick={() => handleRemove(friend.id)}>
                                        Remove
                                    </button>
                                </div>
                            ))}
                        </div>
                    )
                ) : tab === "requests" ? (
                    requests.length === 0 ? (
                        <p className="friendsEmpty">No pending requests.</p>
                    ) : (
                        <div className="friendsList">
                            {requests.map(req => (
                                <div key={req.id} className="friendCard">
                                    <Link to={`/u/${req.username}`} className="friendAvatar">
                                        <Avatar user={req} size="md" />
                                    </Link>
                                    <div className="friendInfo">
                                        <Link to={`/u/${req.username}`}>
                                            <Username user={req} size="md" />
                                        </Link>
                                    </div>
                                    <div className="friendActions">
                                        <button className="friendAcceptBtn" onClick={() => handleAccept(req.id)}>
                                            Accept
                                        </button>
                                        <button className="friendRejectBtn" onClick={() => handleReject(req.id)}>
                                            Reject
                                        </button>
                                    </div>
                                </div>
                            ))}
                        </div>
                    )
                ) : (
                    sent.length === 0 ? (
                        <p className="friendsEmpty">No sent requests.</p>
                    ) : (
                        <div className="friendsList">
                            {sent.map(s => (
                                <div key={s.id} className="friendCard">
                                    <Link to={`/u/${s.username}`} className="friendAvatar">
                                        <Avatar user={s} size="md" />
                                    </Link>
                                    <div className="friendInfo">
                                        <Link to={`/u/${s.username}`}>
                                            <Username user={s} size="md" />
                                        </Link>
                                    </div>
                                    <span className="friendPending">Pending</span>
                                </div>
                            ))}
                        </div>
                    )
                )}
            </section>
        </Layout>
    )
}
