import { useEffect, useState } from "react"
import { useNavigate, Link } from "react-router-dom"
import { apiRequest } from "../lib/api.js"
import Layout from "../components/Layout"
import "../styles/banner.css"

const CONTENT_RULES = [
    "Do not submit malicious links.",
    "Do not submit slurs or hateful content.",
    "Do not impersonate another person or organization.",
    "Do not submit scams, malware, phishing, or deceptive content.",
    "Aim to add joy instead of adding negativity.",
    "Admin has final say."
]

export default function BannerCreate() {
    const navigate = useNavigate()
    const [user, setUser] = useState(null)
    const [checking, setChecking] = useState(true)
    const [pricing, setPricing] = useState({
        price_per_day_usd: 1,
        free_days: 1,
        paid_min_days: 2,
        paid_max_days: 7,
        cooldown_minutes: 5
    })
    const [schedule, setSchedule] = useState(null)
    const [mode, setMode] = useState("free")
    const [days, setDays] = useState(2)
    const [message, setMessage] = useState("")
    const [url, setUrl] = useState("")
    const [bg, setBg] = useState("#000000")
    const [textColor, setTextColor] = useState("#ffffff")
    const [submitting, setSubmitting] = useState(false)
    const [error, setError] = useState("")

    useEffect(() => {
        fetch("/api/site/banner/pricing", { credentials: "include" })
            .then(r => r.json())
            .then(d => { if (d?.pricing) setPricing(prev => ({ ...prev, ...d.pricing })) })
            .catch(() => {})
        fetch("/api/site/banner/schedule", { credentials: "include" })
            .then(r => r.json())
            .then(d => { if (d?.schedule) setSchedule(d.schedule) })
            .catch(() => {})
        fetch("/api/auth/me", { credentials: "include" })
            .then(r => r.json())
            .then(d => { setUser(d.success ? d.user : null); setChecking(false) })
            .catch(() => setChecking(false))
    }, [])

    const totalUsd = days * pricing.price_per_day_usd

    function switchMode(next) {
        setMode(next)
        setError("")
        if (next === "free") setDays(pricing.free_days)
        else setDays(pricing.paid_min_days)
    }

    const freeShowsNow = !schedule || !schedule.active || schedule.active.kind === "free"

    async function handleSubmit(e) {
        e.preventDefault()
        setSubmitting(true)
        setError("")
        const body = {
            message,
            target_url: url,
            background_color: bg,
            text_color: textColor
        }
        try {
            if (mode === "free") {
                const data = await apiRequest("/api/banner/free", {
                    method: "POST",
                    body: JSON.stringify(body)
                })
                if (data?.success) navigate("/")
                else setError(data?.message || "failed to place banner")
            }
            else {
                const data = await apiRequest("/api/banner/payment/create-checkout-session", {
                    method: "POST",
                    body: JSON.stringify({ ...body, duration_days: days })
                })
                if (data?.success && data?.url) window.location.href = data.url
                else setError(data?.message || "failed to start payment")
            }
        }
        catch (e2) {
            setError(e2.message || "request failed")
        }
        finally {
            setSubmitting(false)
        }
    }

    if (checking) {
        return (
            <Layout>
                <div className="bannerCreator"><p className="aboutSmall">loading...</p></div>
            </Layout>
        )
    }

    if (!user) {
        return (
            <Layout>
                <div className="bannerCreator">
                    <h1 className="bannerCreatorTitle">make your own banner</h1>
                    <p>you need an account to make a banner. <Link to="/signin" style={{ color: "#40e0d0" }}>sign in</Link></p>
                </div>
            </Layout>
        )
    }

    return (
        <Layout>
            <div className="bannerCreator">
                <h1 className="bannerCreatorTitle">make your own banner</h1>

                <p className="bannerCreatorBlurb">
                    Banners are community-powered. They are replaceable and not meant to be super serious.
                    They are a way for community members to tell people about things and promote their own projects.
                </p>

                <form className="bannerCreatorForm" onSubmit={handleSubmit}>
                    <div className="bannerCreatorField">
                        <label>message (max 280)</label>
                        <textarea
                            value={message}
                            onChange={e => setMessage(e.target.value)}
                            maxLength={280}
                            rows={3}
                            placeholder="check out my new game!"
                            required
                        />
                        <span className="bannerCreatorCounter">{message.length}/280</span>
                    </div>

                    <div className="bannerCreatorField">
                        <label>link (optional, one link, doesn&apos;t count toward characters)</label>
                        <input
                            type="text"
                            value={url}
                            onChange={e => setUrl(e.target.value)}
                            placeholder="https://..."
                        />
                    </div>

                    <div className="bannerCreatorField">
                        <label>how long</label>
                        <div className="bannerCreatorModes">
                            <button type="button" className={"bannerCreatorMode" + (mode === "free" ? " active" : "")} onClick={() => switchMode("free")}>
                                free ({pricing.free_days} day, $0)
                            </button>
                            <button type="button" className={"bannerCreatorMode" + (mode === "paid" ? " active" : "")} onClick={() => switchMode("paid")}>
                                paid (${pricing.price_per_day_usd}/day)
                            </button>
                        </div>

                        <select
                            className="bannerCreatorSelect"
                            value={days}
                            onChange={e => setDays(Number(e.target.value))}
                        >
                            {mode === "free"
                                ? <option value={pricing.free_days}>{pricing.free_days} day (free)</option>
                                : Array.from({ length: pricing.paid_max_days - pricing.paid_min_days + 1 }, (_, i) => pricing.paid_min_days + i).map(d => (
                                    <option key={d} value={d}>{d} days</option>
                                ))}
                        </select>

                        {mode === "paid" && (
                            <p className="bannerCreatorPrice">
                                total: <strong>${totalUsd.toFixed(2)}</strong> ({days} × ${pricing.price_per_day_usd})
                            </p>
                        )}

                        {mode === "free" && (
                            <p className="bannerCreatorNote">
                                {freeShowsNow
                                    ? "your free banner shows immediately for 1 day."
                                    : "a paid banner is showing, so your free banner will wait in line until it ends."}
                            </p>
                        )}

                        {mode === "paid" && days >= pricing.paid_max_days && (
                            <p className="bannerCreatorNote">
                                {days} days is the maximum. Nothing can be longer, so this banner can never be overwritten.
                                It shows its full {days} days.
                            </p>
                        )}

                        {mode === "paid" && (
                            <p className="bannerCreatorNote">
                                You will be redirected to Stripe to complete payment securely.
                            </p>
                        )}
                    </div>

                    <div className="bannerCreatorRow">
                        <div className="bannerCreatorField">
                            <label>background color</label>
                            <input type="color" value={bg} onChange={e => setBg(e.target.value)} />
                            <input type="text" value={bg} onChange={e => setBg(e.target.value)} />
                        </div>
                        <div className="bannerCreatorField">
                            <label>text color</label>
                            <input type="color" value={textColor} onChange={e => setTextColor(e.target.value)} />
                            <input type="text" value={textColor} onChange={e => setTextColor(e.target.value)} />
                        </div>
                    </div>

                    <div className="bannerCreatorField">
                        <label>preview</label>
                        <div className="siteBanner" style={{ backgroundColor: bg, color: textColor }}>
                            <div className="siteBannerMain">
                                <span className="siteBannerMessage">{message || "your message here"}</span>
                            </div>
                        </div>
                    </div>

                    <div className="bannerCreatorField">
                        <label>rules</label>
                        <ul className="bannerCreatorRules">
                            {CONTENT_RULES.map(r => <li key={r}>{r}</li>)}
                        </ul>
                    </div>

                    <p className="bannerCreatorNote">
                        Banner purchases are fully refundable within 30 days of purchase.{" "}
                        <Link to="/support" style={{ color: "#40e0d0" }}>Go to /support</Link> and choose Payment or finance.
                        Lame ahhhhh Admin has final say.
                    </p>

                    {error && <p className="bannerCreatorError">{error}</p>}

                    <button type="submit" className="bannerCreatorSubmit" disabled={submitting}>
                        {submitting
                            ? "working..."
                            : mode === "free"
                                ? "place free banner"
                                : `pay $${totalUsd.toFixed(2)}`}
                    </button>
                </form>
            </div>
        </Layout>
    )
}
