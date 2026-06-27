import { useState, useEffect } from "react"
import { useNavigate } from "react-router-dom"
import Layout from "../components/Layout"
import { apiRequest } from "../lib/api"

export default function ClientSignIn() {
    const navigate = useNavigate()
    const [checkingAuth, setCheckingAuth] = useState(true)
    const [signedIn, setSignedIn] = useState(false)
    const [code, setCode] = useState("")
    const [expiresAt, setExpiresAt] = useState(null)
    const [countdown, setCountdown] = useState(300)
    const [message, setMessage] = useState("")
    const [copied, setCopied] = useState(false)

    useEffect(() => {
        apiRequest("/api/auth/me").then((data) => {
            if (data.success && data.user) {
                setSignedIn(true)
                createCode()
            } else {
                setSignedIn(false)
            }
            setCheckingAuth(false)
        }).catch(() => {
            setSignedIn(false)
            setCheckingAuth(false)
        })
    }, [])

    useEffect(() => {
        if (!expiresAt) return
        const update = () => {
            const remaining = Math.max(0, Math.floor((new Date(expiresAt).getTime() - Date.now()) / 1000))
            setCountdown(remaining)
            if (remaining <= 0) setCode("")
        }
        update()
        const timer = setInterval(update, 1000)
        return () => clearInterval(timer)
    }, [expiresAt])

    async function createCode() {
        setMessage("")
        try {
            const data = await apiRequest("/api/client-login/create-code", { method: "POST" })
            if (data.success) {
                setCode(data.code)
                setExpiresAt(data.expires_at)
            } else {
                setMessage(data.message || "failed to generate code")
            }
        } catch (error) {
            setMessage(error.message)
        }
    }

    async function handleCopy() {
        try {
            await navigator.clipboard.writeText(code)
            setCopied(true)
            setTimeout(() => setCopied(false), 2000)
        } catch {
            setMessage("could not copy, select the code manually")
        }
    }

    function formatTime(seconds) {
        const m = Math.floor(seconds / 60)
        const s = seconds % 60
        return `${m}:${String(s).padStart(2, "0")}`
    }

    if (checkingAuth) {
        return (
            <Layout>
                <section className="authPage" style={{ textAlign: "center" }}>
                    <p>Loading...</p>
                </section>
            </Layout>
        )
    }

    if (!signedIn) {
        return (
            <Layout>
                <section className="authPage">
                    <div className="authCard">
                        <h1>LINK WITH MIMITA</h1>
                        <p>Sign in or create an account to get a code for the game.</p>
                        <button onClick={() => navigate("/signin", { state: { redirect: "/clientsignin" } })}>
                            SIGN IN
                        </button>
                        <button onClick={() => navigate("/signup", { state: { redirect: "/clientsignin" } })}>
                            CREATE ACCOUNT
                        </button>
                    </div>
                </section>
            </Layout>
        )
    }

    return (
        <Layout>
            <section className="authPage">
                <div className="authCard" style={{ textAlign: "center" }}>
                    <h1>ENTER THIS CODE IN MIMITA</h1>

                    {code ? (
                        <>
                            <div
                                className="clientCodeDisplay"
                                onClick={handleCopy}
                                style={{
                                    fontSize: "4rem",
                                    letterSpacing: "1rem",
                                    fontWeight: "bold",
                                    fontFamily: "monospace",
                                    padding: "2rem",
                                    margin: "1rem 0",
                                    background: "rgba(255,255,255,0.05)",
                                    border: "2px dashed rgba(255,255,255,0.2)",
                                    borderRadius: "12px",
                                    cursor: "pointer",
                                    userSelect: "all"
                                }}
                            >
                                {code}
                            </div>

                            <p style={{ color: "rgba(255,255,255,0.5)", fontSize: "0.85rem" }}>
                                Click code to copy &middot; Expires in {formatTime(countdown)}
                            </p>

                            <div style={{ display: "flex", gap: "0.75rem", justifyContent: "center", margin: "1rem 0" }}>
                                <button onClick={handleCopy} style={{ flex: 1 }}>
                                    {copied ? "COPIED!" : "COPY CODE"}
                                </button>
                                <button onClick={createCode} style={{ flex: 1, background: "rgba(255,255,255,0.1)", color: "white" }}>
                                    REGENERATE
                                </button>
                            </div>

                            <p style={{ color: "rgba(255,255,255,0.4)", fontSize: "0.8rem", marginTop: "1.5rem" }}>
                                Paste this 4-letter code into the Mimita game client to link your account.
                                <br />Keep this page open until the game confirms the link.
                            </p>
                        </>
                    ) : (
                        <>
                            <p>Your code has expired.</p>
                            <button onClick={createCode}>GET NEW CODE</button>
                        </>
                    )}

                    {message && <p className="authMessage">{message}</p>}
                </div>
            </section>
        </Layout>
    )
}  