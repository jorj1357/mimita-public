// 08 06 2026, 16 30
/* purpose
* Shows the post-Stripe VIP checkout confirmation screen with a live name preview.
* Polls the authenticated VIP API until the webhook-granted tier appears, then shows
* tier/expiry details, an edit-color link, and a 30-day self-serve refund action.
* DOES NOT trust redirect query parameters as proof of payment.
* DOES NOT create checkout sessions or process Stripe webhooks.
* DOES NOT render game-engine nameplates or badges.
*/

import { useEffect, useState } from "react"
import { Link, useLocation, useNavigate } from "react-router-dom"
import Layout from "../components/Layout"
import Username from "../components/Username"
import { apiRequest } from "../lib/api"
import { buildSigninPath } from "../lib/returnTo"

const TIER_LABELS = {
    vip: "VIP",
    super_vip: "Super VIP",
    ultra_vip: "Ultra VIP"
}

function tierLabel(tier) {
    return TIER_LABELS[tier] || String(tier || "").replace("_", " ").toUpperCase() || "VIP"
}

function pad(value) {
    return String(value).padStart(2, "0")
}

function stampText(iso) {
    if (!iso) return ""
    const d = new Date(iso)
    if (!Number.isFinite(d.getTime())) return ""
    return `${pad(d.getMonth() + 1)}-${pad(d.getDate())}-${d.getFullYear()} ${pad(d.getHours())}:${pad(d.getMinutes())}:${pad(d.getSeconds())}`
}

export default function VipSuccess() {
    const navigate = useNavigate()
    const location = useLocation()
    const [vip, setVip] = useState(null)
    const [user, setUser] = useState(null)
    const [orders, setOrders] = useState([])
    const [message, setMessage] = useState("waiting for Stripe webhook...")

    const orderId = Number(new URLSearchParams(location.search).get("order_id") || 0)

    useEffect(() => {
        let alive = true
        let tries = 0

        async function poll() {
            tries++
            try {
                const [me, status, ordersData] = await Promise.all([
                    apiRequest("/api/auth/me"),
                    apiRequest("/api/vip/me"),
                    apiRequest("/api/vip/orders")
                ])
                if (!alive) return
                setUser(me.user)
                setVip(status.vip)
                setOrders(ordersData.orders || [])
                if (status.vip?.active_tier && status.vip.active_tier !== "free") {
                    setMessage("")
                    return
                }
                setMessage(tries >= 12
                    ? "Stripe has not confirmed the payment yet."
                    : "waiting for Stripe webhook...")
            }
            catch (error) {
                if (error?.status === 401) {
                    if (alive) navigate(buildSigninPath(location.pathname))
                }
                return
            }
            if (alive && tries < 12) {
                setTimeout(poll, 1500)
            }
        }

        poll()
        return () => { alive = false }
    }, [navigate, location.pathname])

    const previewUser = user && vip
        ? { ...user, supporter_tier: vip.active_tier, vip }
        : user

    const paidOrder = orders.find(o => o.id === orderId)
        || orders.find(o => o.status === "paid")
        || null

    const refundOrder = paidOrder && paidOrder.refundable ? paidOrder : null
    const refundedOrder = paidOrder && paidOrder.status === "refunded" ? paidOrder : null

    return (
        <Layout>
            <section className="vipPage">
                <h1>MiMITA VIP</h1>

                {message && <p className="vipNotice">{message}</p>}

                {vip?.active_tier && vip.active_tier !== "free" && (
                    <>
                        <p className="vipSuccessTitle">Success!!!</p>
                        <p className="vipNotice">
                            <strong>{user?.username || "you"}</strong> now has{" "}
                            <strong>{tierLabel(vip.active_tier)}</strong> until{" "}
                            <strong>{stampText(vip.expires_at)}</strong>
                        </p>

                        <div className="vipPreview">
                            <Username user={previewUser} size="lg" />
                        </div>
                        <p className="vipNotice">This is a live preview of your name style in real time.</p>

                        <p className="vipNotice">
                            Edit your color here:{" "}
                            <Link to="/profile">open your profile editor</Link>
                        </p>

                        {refundOrder && (
                            <div className="vipRefundBox">
                                <p>
                                    Want a refund? They're valid for 30 days, meaning you have until{" "}
                                    <strong>{stampText(refundOrder.refund_until)}</strong> to get a{" "}
                                    <strong>100% refund</strong>!!!
                                </p>
                                <p className="vipNotice">
                                    <Link to={`/support?refund_order=${refundOrder.id}`}>Click here to request a refund</Link>
                                </p>
                            </div>
                        )}

                        {refundedOrder && (
                            <div className="vipRefundBox">
                                <p>This order was refunded. Your name style may change back to free styling.</p>
                            </div>
                        )}

                        <p><Link to="/vip">open VIP settings</Link></p>
                    </>
                )}

                {!message && !(vip?.active_tier && vip.active_tier !== "free") && (
                    <p><Link to="/vip">open VIP settings</Link></p>
                )}
            </section>
        </Layout>
    )
}
