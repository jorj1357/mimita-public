import { useState } from "react"

import Layout from "../components/Layout"
import { apiRequest } from "../lib/api"

export default function PasswordChange() {
    const [step, setStep] = useState("request")
    const [code, setCode] = useState("")
    const [form, setForm] = useState({
        oldPassword: "",
        newPassword: "",
        confirmNewPassword: ""
    })
    const [message, setMessage] = useState("")

    async function requestCode() {
        try {
            const data = await apiRequest(
                "/api/auth/password-change/request",
                { method: "POST" }
            )
            setMessage(data.message)
            setStep("verify")
        }
        catch (error) {
            setMessage(error.message)
        }
    }

    async function verifyCode(event) {
        event.preventDefault()

        try {
            const data = await apiRequest(
                "/api/auth/password-change/verify",
                {
                    method: "POST",
                    body: JSON.stringify({ code })
                }
            )
            setMessage(data.message)
            setStep("finalize")
        }
        catch (error) {
            setMessage(error.message)
        }
    }

    async function finalize(event) {
        event.preventDefault()

        try {
            const data = await apiRequest(
                "/api/auth/password-change/finalize",
                {
                    method: "POST",
                    body: JSON.stringify(form)
                }
            )
            setMessage(data.message)
            setStep("done")
        }
        catch (error) {
            setMessage(error.message)
        }
    }

    return (
        <Layout>
            <section className="authPage">
                <div className="authCard">
                    <h1>CHANGE PASSWORD</h1>

                    {step === "request" && (
                        <button type="button" onClick={requestCode}>
                            SEND 6-DIGIT CODE
                        </button>
                    )}

                    {step === "verify" && (
                        <form onSubmit={verifyCode}>
                            <label htmlFor="code">verification code</label>
                            <input
                                id="code"
                                inputMode="numeric"
                                pattern="[0-9]{6}"
                                maxLength={6}
                                value={code}
                                onChange={(event) =>
                                    setCode(event.target.value)}
                                required
                            />
                            <button type="submit">VERIFY CODE</button>
                        </form>
                    )}

                    {step === "finalize" && (
                        <form onSubmit={finalize}>
                            <label htmlFor="oldPassword">old password</label>
                            <input
                                id="oldPassword"
                                type="password"
                                value={form.oldPassword}
                                onChange={(event) => setForm({
                                    ...form,
                                    oldPassword: event.target.value
                                })}
                                required
                            />
                            <label htmlFor="newPassword">new password</label>
                            <input
                                id="newPassword"
                                type="password"
                                value={form.newPassword}
                                onChange={(event) => setForm({
                                    ...form,
                                    newPassword: event.target.value
                                })}
                                required
                            />
                            <label htmlFor="confirmNewPassword">
                                confirm new password
                            </label>
                            <input
                                id="confirmNewPassword"
                                type="password"
                                value={form.confirmNewPassword}
                                onChange={(event) => setForm({
                                    ...form,
                                    confirmNewPassword: event.target.value
                                })}
                                required
                            />
                            <button type="submit">CHANGE PASSWORD</button>
                        </form>
                    )}

                    {step === "done" && (
                        <p>Your password has been changed.</p>
                    )}

                    {message && (
                        <p className="authMessage">{message}</p>
                    )}
                </div>
            </section>
        </Layout>
    )
}
