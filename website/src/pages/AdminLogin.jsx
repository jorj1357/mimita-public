import { useState } from "react"
import { useNavigate } from "react-router-dom"
import { apiRequest } from "../lib/api.js"

export default function AdminLogin() {
    const navigate = useNavigate()
    const [username, setUsername] = useState("")
    const [password, setPassword] = useState("")
    const [message, setMessage] = useState("")
    const [loading, setLoading] = useState(false)

    async function handleSubmit(e) {
        e.preventDefault()
        setLoading(true)
        setMessage("")
        try {
            const data = await apiRequest("/api/admin/login", {
                method: "POST",
                body: JSON.stringify({ username, password })
            })
            navigate("/admin/dashboard")
        }
        catch (err) {
            setMessage(err.message || "login failed")
        }
        finally {
            setLoading(false)
        }
    }

    return (
        <div className="adminLoginPage">
            <form className="adminLoginCard" onSubmit={handleSubmit}>
                <h1 className="adminLoginTitle">admin</h1>
                <input
                    className="adminLoginInput"
                    type="text"
                    placeholder="username"
                    value={username}
                    onChange={e => setUsername(e.target.value)}
                    autoComplete="username"
                    required
                />
                <input
                    className="adminLoginInput"
                    type="password"
                    placeholder="password"
                    value={password}
                    onChange={e => setPassword(e.target.value)}
                    autoComplete="current-password"
                    required
                />
                <button className="adminLoginButton" type="submit" disabled={loading}>
                    {loading ? "signing in..." : "sign in"}
                </button>
                {message && <p className="adminLoginMessage">{message}</p>}
            </form>
        </div>
    )
}
