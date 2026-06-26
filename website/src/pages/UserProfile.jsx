import { useEffect, useState } from "react"
import { useParams } from "react-router-dom"

import Layout from "../components/Layout"
import Username from "../components/Username"
import Avatar from "../components/Avatar"
import { apiRequest } from "../lib/api"

export default function UserProfile() {
    const { username } = useParams()
    const [user, setUser] = useState(null)
    const [message, setMessage] = useState("loading profile...")

    useEffect(() => {
        apiRequest(`/api/users/${encodeURIComponent(username)}`)
            .then((data) => {
                setUser(data.user)
                setMessage("")
            })
            .catch((error) => setMessage(error.message))
    }, [username])

    function formatDate(dateStr) {
        if (!dateStr) return ""
        return new Date(dateStr).toLocaleDateString("en-US", {
            year: "numeric",
            month: "long",
            day: "numeric"
        })
    }

    return (
        <Layout>
            <section className="profilePage">
                {user ? (
                    <div className="profileCard">
                        <Avatar user={user} size="lg" />

                        <h1 className="profileUsername">
                            <Username user={user} size="lg" />
                        </h1>

                        {user.bio && <p className="profileBio">{user.bio}</p>}

                        <p className="profileJoined">
                            Joined {formatDate(user.created_at)}
                        </p>

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