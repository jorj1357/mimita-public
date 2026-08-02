import { Link, useNavigate } from "react-router-dom"
import { useState } from "react"

import Layout from "../components/Layout"
import { apiRequest } from "../lib/api"

export default function ForgotPassword() {
    const navigate = useNavigate()
    const [step, setStep] = useState(1)
    const [identifier, setIdentifier] = useState("")
    const [code, setCode] = useState("")
    const [newPassword, setNewPassword] = useState("")
    const [confirmPassword, setConfirmPassword] = useState("")
    const [message, setMessage] = useState("")
    const [error, setError] = useState("")
    const [loading, setLoading] = useState(false)

    async function requestCode(event) {
        event.preventDefault()
        setLoading(true)
        setError("")
        setMessage("")

        try {
            await apiRequest("/api/auth/forgot-password/request", {
                method: "POST",
                body: JSON.stringify({ identifier })
            })
            setStep(2)
            setMessage("if an account exists, a reset code was sent")
        }
        catch (requestError) {
            setError(requestError.message)
        }
        finally {
            setLoading(false)
        }
    }

    async function resetPassword(event) {
        event.preventDefault()
        setLoading(true)
        setError("")

        if (newPassword !== confirmPassword) {
            setError("new passwords do not match")
            setLoading(false)
            return
        }

        try {
            await apiRequest("/api/auth/forgot-password/reset", {
                method: "POST",
                body: JSON.stringify({
                    identifier,
                    code,
                    newPassword,
                    confirmNewPassword: confirmPassword
                })
            })
            navigate("/signin", { state: { message: "password reset. sign in with your new password." } })
        }
        catch (requestError) {
            setError(requestError.message)
        }
        finally {
            setLoading(false)
        }
    }

    return (
        <Layout>
            <section className="authPage">
                <form
                    className="authCard"
                    onSubmit={step === 1 ? requestCode : resetPassword}
                >
                    <h1>FORGOT PASSWORD</h1>

                    {step === 1 && (
                        <>
                            <label htmlFor="identifier">
                                username or email
                            </label>
                            <input
                                id="identifier"
                                value={identifier}
                                onChange={(event) =>
                                    setIdentifier(event.target.value)}
                                autoComplete="username"
                                required
                            />
                            <button type="submit" disabled={loading}>
                                {loading ? "WORKING..." : "SEND RESET CODE"}
                            </button>
                        </>
                    )}

                    {step === 2 && (
                        <>
                            <label htmlFor="code">reset code</label>
                            <input
                                id="code"
                                value={code}
                                onChange={(event) =>
                                    setCode(event.target.value)}
                                placeholder="6-digit code"
                                autoComplete="one-time-code"
                                required
                            />

                            <label htmlFor="newPassword">new password</label>
                            <input
                                id="newPassword"
                                type="password"
                                value={newPassword}
                                onChange={(event) =>
                                    setNewPassword(event.target.value)}
                                autoComplete="new-password"
                                required
                            />

                            <label htmlFor="confirmPassword">
                                confirm new password
                            </label>
                            <input
                                id="confirmPassword"
                                type="password"
                                value={confirmPassword}
                                onChange={(event) =>
                                    setConfirmPassword(event.target.value)}
                                autoComplete="new-password"
                                required
                            />

                            <p className="authHint">
                                8+ characters, 1 uppercase, 1 symbol.
                            </p>

                            <button type="submit" disabled={loading}>
                                {loading ? "WORKING..." : "RESET PASSWORD"}
                            </button>
                        </>
                    )}

                    {message && (
                        <p className="authMessage">{message}</p>
                    )}
                    {error && (
                        <p className="authMessage">{error}</p>
                    )}

                    <p className="authSwitch">
                        Remembered it?{" "}
                        <Link to="/signin">Sign in</Link>
                    </p>
                </form>
            </section>
        </Layout>
    )
}
