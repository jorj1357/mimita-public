import { useState, useEffect } from "react"
import { Link, useSearchParams } from "react-router-dom"
import { apiRequest } from "../lib/api.js"

export default function BannerPayTest() {
    const [params] = useSearchParams()
    const [user, setUser] = useState(null)
    const [checking, setChecking] = useState(true)
    const [days, setDays] = useState(1)
    const [submitting, setSubmitting] = useState(false)
    const [error, setError] = useState("")

    useEffect(() => {
        fetch("/api/auth/me", { credentials: "include" })
            .then(r => r.json())
            .then(data => {
                if (data.success) setUser(data.user)
                else setUser(null)
            })
            .catch(() => setUser(null))
            .finally(() => setChecking(false))
    }, [])

    async function handlePay() {
        setSubmitting(true)
        setError("")
        try {
            const data = await apiRequest("/api/banner/payment/create-checkout-session", {
                method: "POST",
                body: JSON.stringify({ duration_days: days })
            })
            if (data?.success && data?.url) {
                window.location.href = data.url
                return
            }
            setError(data?.message || "failed to create checkout")
        }
        catch (e) {
            setError(e.message || "request failed")
        }
        finally {
            setSubmitting(false)
        }
    }

    const status = params.get("status")

    return (
        <div style={{ maxWidth: "520px", margin: "3rem auto", padding: "0 1.25rem", fontFamily: "sans-serif", color: "#ddd" }}>
            <h1>banner payment test</h1>

            {status === "success" && (
                <p style={{ color: "#66ff88", border: "1px solid #66ff8833", padding: "0.75rem" }}>
                    payment complete (pending webhook confirmation)
                </p>
            )}
            {status === "cancelled" && (
                <p style={{ color: "#ffcc66", border: "1px solid #ffcc6633", padding: "0.75rem" }}>
                    payment cancelled
                </p>
            )}

            {checking ? (
                <p>checking sign-in...</p>
            ) : !user ? (
                <p>
                    you must be signed in to buy banner time.{" "}
                    <Link to="/signin" style={{ color: "#40e0d0" }}>sign in</Link>
                </p>
            ) : (
                <>
                    <p style={{ opacity: 0.7 }}>signed in as <strong>{user.username}</strong></p>
                    <label style={{ display: "block", margin: "1rem 0 0.5rem" }}>days (1 day = $1, max 7)</label>
                    <div style={{ display: "flex", gap: "0.5rem", flexWrap: "wrap" }}>
                        {[1, 2, 3, 4, 5, 6, 7].map(d => (
                            <button
                                key={d}
                                type="button"
                                onClick={() => setDays(d)}
                                style={{
                                    padding: "0.5rem 0.9rem",
                                    background: days === d ? "#40e0d0" : "#111",
                                    color: days === d ? "#000" : "#ddd",
                                    border: "1px solid #40e0d0",
                                    cursor: "pointer"
                                }}
                            >
                                {d}
                            </button>
                        ))}
                    </div>
                    <p style={{ marginTop: "1rem", fontSize: "1.1rem" }}>
                        total: <strong>${days.toFixed(2)}</strong>
                    </p>
                    {error && <p style={{ color: "#f87171" }}>{error}</p>}
                    <button
                        type="button"
                        onClick={handlePay}
                        disabled={submitting}
                        style={{
                            marginTop: "1rem",
                            padding: "0.75rem 1.5rem",
                            background: submitting ? "#333" : "#40e0d0",
                            color: "#000",
                            border: "none",
                            cursor: submitting ? "wait" : "pointer",
                            fontWeight: 700
                        }}
                    >
                        {submitting ? "creating..." : `pay $${days.toFixed(2)}`}
                    </button>
                </>
            )}
        </div>
    )
}
