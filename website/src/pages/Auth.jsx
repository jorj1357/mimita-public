import { Link, useLocation, useNavigate } from "react-router-dom"
import { useState } from "react"

import Layout from "../components/Layout"
import { apiRequest } from "../lib/api"

export default function Auth({ mode }) {
    const signup = mode === "signup"
    const navigate = useNavigate()
    const location = useLocation()
    const [form, setForm] = useState({
        username: "",
        email: "",
        identifier: "",
        password: ""
    })
    const [message, setMessage] = useState(location.state?.message || "")
    const [loading, setLoading] = useState(false)

    function update(field, value) {
        setForm((current) => ({
            ...current,
            [field]: value
        }))
    }

    async function submit(event) {
        event.preventDefault()
        setLoading(true)
        setMessage("")

        try {
            const data = await apiRequest(
                signup ? "/api/auth/signup" : "/api/auth/signin",
                {
                    method: "POST",
                    body: JSON.stringify(signup
                        ? {
                            username: form.username,
                            email: form.email,
                            password: form.password
                        }
                        : {
                            identifier: form.identifier,
                            password: form.password
                        })
                }
            )

            navigate(`/users/${encodeURIComponent(data.user.username)}`)
        }
        catch (error) {
            setMessage(error.message)
        }
        finally {
            setLoading(false)
        }
    }

    return (
        <Layout>
            <section className="authPage">
                <form className="authCard" onSubmit={submit}>
                    <h1>{signup ? "SIGN UP" : "SIGN IN"}</h1>

                    {signup && (
                        <>
                            <label htmlFor="username">username</label>
                            <input
                                id="username"
                                value={form.username}
                                onChange={(event) =>
                                    update("username", event.target.value)}
                                autoComplete="username"
                                required
                            />

                            <label htmlFor="email">email</label>
                            <input
                                id="email"
                                type="email"
                                value={form.email}
                                onChange={(event) =>
                                    update("email", event.target.value)}
                                autoComplete="email"
                                required
                            />
                        </>
                    )}

                    {!signup && (
                        <>
                            <label htmlFor="identifier">
                                username or email
                            </label>
                            <input
                                id="identifier"
                                value={form.identifier}
                                onChange={(event) =>
                                    update("identifier", event.target.value)}
                                autoComplete="username"
                                required
                            />
                        </>
                    )}

                    <label htmlFor="password">password</label>
                    <input
                        id="password"
                        type="password"
                        value={form.password}
                        onChange={(event) =>
                            update("password", event.target.value)}
                        autoComplete={
                            signup ? "new-password" : "current-password"
                        }
                        required
                    />

                    {signup && (
                        <p className="authHint">
                            8+ characters, 1 uppercase, 1 symbol.{" "}
                            <Link to="/password-principles">
                                Password principles
                            </Link>
                        </p>
                    )}

                    <button type="submit" disabled={loading}>
                        {loading
                            ? "WORKING..."
                            : signup ? "CREATE ACCOUNT" : "SIGN IN"}
                    </button>

                    {message && (
                        <p className="authMessage">{message}</p>
                    )}

                    <p className="authSwitch">
                        {signup ? "Already registered? " : "Need an account? "}
                        <Link to={signup ? "/signin" : "/signup"}>
                            {signup ? "Sign in" : "Sign up"}
                        </Link>
                    </p>
                </form>
            </section>
        </Layout>
    )
}
