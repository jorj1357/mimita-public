// 08 05 2026, 00 00
/* purpose
* Shows the post-Stripe VIP checkout confirmation screen.
* Polls the authenticated VIP API until the webhook-granted tier appears.
* Links back to VIP customization once the account state is visible.
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

export default function VipSuccess() {
    const navigate = useNavigate()
    const location = useLocation()
    const [vip, setVip] = useState(null)
    const [user, setUser] = useState(null)
    const [message, setMessage] = useState("waiting for Stripe webhook...")

    useEffect(() => {
        let alive = true
        let tries = 0

        async function poll() {
            tries++
            try {
                const [me, status] = await Promise.all([
                    apiRequest("/api/auth/me"),
                    apiRequest("/api/vip/me")
                ])
                if (!alive) return
                setUser(me.user)
                setVip(status.vip)
                if (status.vip?.active_tier && status.vip.active_tier !== "free") {
                    setMessage("VIP is active.")
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

    return (
        <Layout>
            <section className="vipPage">
                <h1>MiMITA VIP</h1>
                <p className="vipNotice">{message}</p>
                {previewUser && (
                    <div className="vipPreview">
                        <Username user={previewUser} size="lg" />
                    </div>
                )}
                <p><Link to="/vip">open VIP settings</Link></p>
            </section>
        </Layout>
    )
}
