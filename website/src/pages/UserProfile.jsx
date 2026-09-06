// 09 06 2026, 14 43
/* purpose
* Resolve public accounts by ID or username and display their profile.
* Reuse shared persisted statistics and account presentation components.
* DOES NOT save progression or expose private account settings.
*/
import { useEffect, useState } from "react"
import { useParams } from "react-router-dom"

import Layout from "../components/Layout"
import Username from "../components/Username"
import Avatar from "../components/Avatar"
import ProfileStats from "../components/ProfileStats"
import { apiRequest } from "../lib/api"

export default function UserProfile() {
    const { username, id } = useParams()
    const [result, setResult] = useState(null)
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

    function formatDate(dateStr) {
        if (!dateStr) return "Unknown"
        const d = new Date(dateStr)
        const mm = String(d.getMonth() + 1).padStart(2, "0")
        const dd = String(d.getDate()).padStart(2, "0")
        const yyyy = d.getFullYear()
        return `${mm}-${dd}-${yyyy}`
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
