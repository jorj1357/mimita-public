import { useEffect, useState } from "react"
import { useParams } from "react-router-dom"

import Layout from "../components/Layout"
import Username from "../components/Username"
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
                        <div className="profileAvatarWrap">
                            {user.avatar_url ? (
                                <img
                                    src={user.avatar_url}
                                    alt={`${user.username}'s avatar`}
                                    className="profileAvatar"
                                />
                            ) : (
                                <div className="profileAvatarPlaceholder">
                                    {user.username[0]?.toUpperCase()}
                                </div>
                            )}
                        </div>

                        <h1 className="profileUsername">
                            <Username user={user} size="lg" />
                        </h1>

                        {user.bio && <p className="profileBio">{user.bio}</p>}

                        <p className="profileJoined">
                            Joined {formatDate(user.created_at)}
                        </p>
                    </div>
                ) : (
                    <p>{message}</p>
                )}
            </section>
        </Layout>
    )
}