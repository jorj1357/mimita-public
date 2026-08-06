import { useEffect, useState } from "react"
import { useLocation } from "react-router-dom"
import "../App.css"
import "../styles/support.css"
import Layout from "../components/Layout"
import PixelBox from "../components/PixelBox"
import { apiRequest } from "../lib/api.js"

const TOPICS = [
    { value: "user_issue", label: "Issue with another user" },
    { value: "game_issue", label: "Issue with the MiMITA game" },
    { value: "payment_finance", label: "Issue with payment or finance" },
    { value: "vip_purchase", label: "Issue with VIP purchase" },
    { value: "security", label: "Issue with security, hacks, or attempted hacks" },
    { value: "other", label: "Other" }
]

function stampText(iso) {
    if (!iso) return ""
    const d = new Date(iso)
    if (!Number.isFinite(d.getTime())) return ""
    const pad = value => String(value).padStart(2, "0")
    return `${pad(d.getMonth() + 1)}-${pad(d.getDate())}-${d.getFullYear()} ${pad(d.getHours())}:${pad(d.getMinutes())}:${pad(d.getSeconds())}`
}

export default function Support() {
    const location = useLocation()
    const [email, setEmail] = useState("")
    const [topic, setTopic] = useState("game_issue")
    const [subject, setSubject] = useState("")
    const [message, setMessage] = useState("")
    const [url, setUrl] = useState("")
    const [bannerOrderId, setBannerOrderId] = useState("")
    const [sent, setSent] = useState(false)
    const [sentEmail, setSentEmail] = useState("")
    const [error, setError] = useState("")

    const refundOrderId = Number(new URLSearchParams(location.search).get("refund_order") || 0)

    useEffect(() => {
        if (!refundOrderId) return
        let alive = true
        Promise.all([
            apiRequest("/api/auth/me"),
            apiRequest("/api/vip/orders")
        ])
            .then(([me, ordersData]) => {
                if (!alive) return
                const order = (ordersData.orders || []).find(o => o.id === refundOrderId)
                if (!order) return
                const user = me.user
                const price = `$${(Number(order.amount_cents) / 100).toFixed(2)} ${order.currency}`
                setTopic("vip_purchase")
                setSubject("VIP Payment Issue")
                setEmail(user.email || "")
                setUrl(`/vip/success?order_id=${order.id}`)
                setMessage([
                    `Hi, I'd like to request a refund for the purchase I made on ${stampText(order.paid_at)}.`,
                    `The purchase id is ${order.id} and the price I saw was ${price}.`,
                    "",
                    "Verification:",
                    `Username: ${user.username}`,
                    `Account ID: ${user.id}`,
                    `Tier: ${order.tier}`,
                    `Payment Intent: ${order.stripe_payment_intent_id || "unknown"}`
                ].join("\n"))
            })
            .catch(() => {})
        return () => { alive = false }
    }, [refundOrderId])

    async function handleSubmit(e) {
        e.preventDefault()
        setError("")
        try {
            const data = await apiRequest("/api/support", {
                method: "POST",
                body: JSON.stringify({
                    email, topic, subject, message, url,
                    banner_order_id: bannerOrderId ? Number(bannerOrderId) : null
                })
            })
            if (data?.success) {
                setSentEmail(email)
                setSent(true)
                setEmail("")
                setSubject("")
                setMessage("")
                setUrl("")
                setBannerOrderId("")
            }
            else {
                setError(data?.message || "failed to send")
            }
        }
        catch (e2) {
            setError(e2.message || "failed to send")
        }
    }

    return (
        <Layout>
            <div className="supportPage">
                <h1 className="supportTitle">SUPPORT</h1>

                <PixelBox>
                    {sent ? (
                        <div className="supportSent">
                            <p>Your message has been sent. We will respond to you at {sentEmail || email}.</p>
                            <button
                                type="button"
                                className="supportWrongEmail"
                                onClick={() => {
                                    setSent(false)
                                    setSentEmail("")
                                }}
                            >
                                Wrong email? Submit another one here
                            </button>
                        </div>
                    ) : (
                        <form className="supportForm" onSubmit={handleSubmit}>
                            <label className="supportLabel">Topic (required)</label>
                            <select className="supportInput" value={topic} onChange={e => setTopic(e.target.value)}>
                                {TOPICS.map(t => <option key={t.value} value={t.value}>{t.label}</option>)}
                            </select>

                            <label className="supportLabel">Your Email</label>
                            <input className="supportInput" type="email" value={email} onChange={e => setEmail(e.target.value)} required placeholder="you@example.com" />

                            <label className="supportLabel">Subject</label>
                            <input className="supportInput" type="text" value={subject} onChange={e => setSubject(e.target.value)} required placeholder="What is this about?" />

                            <label className="supportLabel">Message</label>
                            <textarea className="supportTextarea" value={message} onChange={e => setMessage(e.target.value)} required rows={6} placeholder="Describe your issue in detail..." />

                            <label className="supportLabel">Relevant URL (optional)</label>
                            <input className="supportInput" type="text" value={url} onChange={e => setUrl(e.target.value)} placeholder="https://..." />

                            <label className="supportLabel">Related banner order ID (optional)</label>
                            <input className="supportInput" type="number" min="1" value={bannerOrderId} onChange={e => setBannerOrderId(e.target.value)} placeholder="for payment or finance issues" />

                            {error && <p className="supportError">{error}</p>}

                            <button type="submit" className="supportSubmit">Send</button>
                        </form>
                    )}
                </PixelBox>
            </div>
        </Layout>
    )
}
