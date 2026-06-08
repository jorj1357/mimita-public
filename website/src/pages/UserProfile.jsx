import { useEffect, useState } from "react"
import { useParams } from "react-router-dom"

import Layout from "../components/Layout"
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

    return (
        <Layout>
            <section className="profilePage">
                {user ? (
                    <>
                        <h1>@{user.username}</h1>
                        <p>{user.bio || "No bio yet."}</p>
                    </>
                ) : (
                    <p>{message}</p>
                )}
            </section>
        </Layout>
    )
}
