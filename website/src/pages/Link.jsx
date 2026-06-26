import { useState, useEffect } from "react"
import { useNavigate } from "react-router-dom"

import Layout from "../components/Layout"
import { apiRequest } from "../lib/api"

export default function Link() {
    const [code, setCode] = useState("")
    const [message, setMessage] = useState("")
    const [success, setSuccess] = useState(false)
    const navigate = useNavigate()

    useEffect(() => {
        apiRequest("/api/auth/me").then((data) => {
            if (!data.user) navigate("/signin", { state: { message: "Sign in first" } })
        }).catch(() => {
            navigate("/signin", { state: { message: "Sign in first" } })
        })
    }, [navigate])

    async function submit(event) {
        event.preventDefault()
        setMessage("")

        try {
            const data = await apiRequest("/api/auth/link-claim", {
                method: "POST",
                body: JSON.stringify({ code: code.trim() })
            })
            if (data.success) {
                setSuccess(true)
                setMessage("Account linked! You can now close this page and return to the game.")
            }
            else {
                setMessage(data.message || "failed to link account")
            }
        }
        catch (error) {
            setMessage(error.message)
        }
    }

    return (
        <Layout>
            <section className="authPage">
                <div className="authCard">
                    <h1>LINK ACCOUNT</h1>
                    <p>Enter the code shown in the game to link your account.</p>

                    {!success && (
                        <form onSubmit={submit}>
                            <label htmlFor="code">6-digit code</label>
                            <input
                                id="code"
                                value={code}
                                onChange={(e) => setCode(e.target.value)}
                                maxLength={6}
                                placeholder="123456"
                                autoFocus
                                required
                            />
                            <button type="submit">LINK</button>
                        </form>
                    )}

                    {message && (
                        <p className={success ? "authSuccess" : "authMessage"}>{message}</p>
                    )}
                </div>
            </section>
        </Layout>
    )
}
