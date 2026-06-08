import { Link, useNavigate } from "react-router-dom"
import { useEffect, useState } from "react"

import Layout from "../components/Layout"
import { apiRequest } from "../lib/api"

export default function Account() {
    const navigate = useNavigate()
    const [user, setUser] = useState(null)
    const [bio, setBio] = useState("")
    const [deletePassword, setDeletePassword] = useState("")
    const [message, setMessage] = useState("loading account...")

    useEffect(() => {
        apiRequest("/api/auth/me")
            .then((data) => {
                setUser(data.user)
                setBio(data.user.bio || "")
                setMessage("")
            })
            .catch(() => navigate("/signin"))
    }, [navigate])

    async function saveProfile(event) {
        event.preventDefault()

        try {
            await apiRequest("/api/account/profile", {
                method: "PATCH",
                body: JSON.stringify({ bio })
            })
            setMessage("profile saved")
        }
        catch (error) {
            setMessage(error.message)
        }
    }

    async function toggleNotifications(event) {
        const enabled = event.target.checked

        try {
            await apiRequest("/api/account/notification-preferences", {
                method: "PATCH",
                body: JSON.stringify({
                    emailNotifications: enabled
                })
            })
            setUser((current) => ({
                ...current,
                email_notifications_enabled: enabled
            }))
            setMessage("notification preferences saved")
        }
        catch (error) {
            setMessage(error.message)
        }
    }

    async function signOut() {
        try {
            const data = await apiRequest("/api/auth/signout", {
                method: "POST"
            })
            navigate("/signin", {
                state: { message: data.message }
            })
        }
        catch (error) {
            setMessage(error.message)
        }
    }

    async function deleteAccount(event) {
        event.preventDefault()

        if (!window.confirm("Permanently delete this account?")) {
            return
        }

        try {
            await apiRequest("/api/account", {
                method: "DELETE",
                body: JSON.stringify({
                    password: deletePassword
                })
            })
            navigate("/signup")
        }
        catch (error) {
            setMessage(error.message)
        }
    }

    if (!user) {
        return (
            <Layout>
                <section className="authPage">
                    <p>{message}</p>
                </section>
            </Layout>
        )
    }

    return (
        <Layout>
            <section className="authPage">
                <div className="authCard accountCard">
                    <h1>ACCOUNT</h1>
                    <p>
                        <Link to={`/users/${user.username}`}>
                            @{user.username}
                        </Link>
                    </p>
                    <p>{user.email}</p>

                    <form onSubmit={saveProfile}>
                        <label htmlFor="bio">bio</label>
                        <textarea
                            id="bio"
                            value={bio}
                            maxLength={500}
                            onChange={(event) => setBio(event.target.value)}
                        />
                        <button type="submit">SAVE PROFILE</button>
                    </form>

                    <label className="authToggle">
                        <input
                            type="checkbox"
                            checked={user.email_notifications_enabled}
                            onChange={toggleNotifications}
                        />
                        email notifications
                    </label>

                    <Link className="authAction" to="/change-password">
                        change password
                    </Link>
                    <button type="button" onClick={signOut}>
                        SIGN OUT
                    </button>

                    <form
                        className="dangerZone"
                        onSubmit={deleteAccount}
                    >
                        <h2>DELETE ACCOUNT</h2>
                        <label htmlFor="deletePassword">
                            confirm password
                        </label>
                        <input
                            id="deletePassword"
                            type="password"
                            value={deletePassword}
                            onChange={(event) =>
                                setDeletePassword(event.target.value)}
                            required
                        />
                        <button type="submit">DELETE ACCOUNT</button>
                    </form>

                    {message && (
                        <p className="authMessage">{message}</p>
                    )}
                </div>
            </section>
        </Layout>
    )
}
