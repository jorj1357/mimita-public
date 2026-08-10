// 08 03 2026, 17 20
/* purpose
* Provides the MiMITA VIP purchase and current-status page.
* Calls the real VIP APIs for checkout and the subscription portal.
* Keeps the supporter explanation and tier cards, then a centered current-status panel.
* Name-style editing lives on the profile page, the account page, and the profile menu.
* DOES NOT fake checkout success or grant VIP from redirect parameters.
* DOES NOT store Stripe Price IDs or secrets on the client.
* DOES NOT implement game-engine name rendering.
*/

import { useEffect, useState } from "react"
import { Link, useLocation, useNavigate } from "react-router-dom"
import Layout from "../components/Layout"
import Username from "../components/Username"
import VipBadge from "../components/VipBadge"
import { apiRequest } from "../lib/api"
import { buildSigninPath } from "../lib/returnTo"
import { logAuthEvent } from "../lib/api-log"

function dollars(cents) {
    return `$${(Number(cents || 0) / 100).toFixed(2)}`
}

function dateText(iso) {
    if (!iso) return "none"
    return new Date(iso).toLocaleString()
}

const ACTIVE_SUBSCRIPTION_STATUSES = new Set(["active", "trialing", "past_due"])

const TIER_RANK = { vip: 1, super_vip: 2, ultra_vip: 3 }

const TIER_LABELS = {
    vip: "VIP",
    super_vip: "Super VIP",
    ultra_vip: "Ultra VIP"
}

export default function Vip() {
    const navigate = useNavigate()
    const location = useLocation()
    const [user, setUser] = useState(null)
    const [config, setConfig] = useState(null)
    const [vip, setVip] = useState(null)
    const [message, setMessage] = useState("loading VIP...")
    const [busy, setBusy] = useState("")

    useEffect(() => {
        let alive = true
        Promise.allSettled([
            apiRequest("/api/auth/me"),
            apiRequest("/api/vip/config"),
            apiRequest("/api/vip/me")
        ])
            .then(([me, cfg, vipMe]) => {
                if (!alive) return
                setUser(me.status === "fulfilled" ? me.value.user : null)
                setConfig(cfg.status === "fulfilled" ? cfg.value.config : null)
                setVip(vipMe.status === "fulfilled" ? vipMe.value.vip : null)
                if (me.status !== "fulfilled") {
                    logAuthEvent("anonymous view", { returnTo: location.pathname })
                }
                setMessage(cfg.status === "fulfilled" ? "" : "failed to load VIP")
            })
            .catch(() => {
                setMessage("failed to load VIP")
            })
        return () => { alive = false }
    }, [navigate, location.pathname])

    async function checkout(tier, purchaseType) {
        if (!user) {
            navigate(buildSigninPath(location.pathname))
            return
        }
        setBusy(`${tier}:${purchaseType}`)
        setMessage("")
        try {
            const data = await apiRequest("/api/vip/payment/checkout", {
                method: "POST",
                body: JSON.stringify({ tier, purchase_type: purchaseType })
            })
            if (data.checkout_url) window.location.assign(data.checkout_url)
            else setMessage("checkout did not return a url")
        }
        catch (error) {
            setMessage(error.message)
        }
        finally {
            setBusy("")
        }
    }

    async function manageSubscription() {
        if (!user) {
            navigate(buildSigninPath(location.pathname))
            return
        }
        setBusy("manage-subscription")
        try {
            const data = await apiRequest("/api/vip/manage-subscription", { method: "POST" })
            if (data.url) window.location.assign(data.url)
        }
        catch (error) {
            setMessage(error.message)
        }
        finally {
            setBusy("")
        }
    }

    if (!config) {
        return (
            <Layout>
                <section className="vipPage">
                    <p>{message || "loading VIP..."}</p>
                </section>
            </Layout>
        )
    }

    const vipState = vip || { active_tier: "free", active: false, expires_at: null, subscription: null }
    const currentRank = TIER_RANK[vipState.active_tier] || 0
    const previewUser = user && vip ? {
        ...user,
        supporter_tier: vip.active_tier,
        vip
    } : null

    return (
        <Layout>
            <section className="vipPage">
                <h1>MiMITA VIP</h1>
                {message && <p className={message.includes("failed") || message.includes("not") ? "vipError" : "vipNotice"}>{message}</p>}

                <div className="vipWhatIsThis">
                    <h2>what is this why u asking for money loser</h2>
                    <p>VIP is a voluntary supporter thing that gives you a cool colorful name in game and on the site. It's NOT!!!!!!!! a paywall. MiMITA is 100% free, and VIP will never block access to anything. EVER!!!!!</p>
                </div>

                <div className="vipGrid">
                    {config.tiers.map(tier => {
                        const rank = TIER_RANK[tier.tier] || 0
                        const isLower = vipState.active_tier !== "free" && rank < currentRank
                        const isUpgrade = vipState.active_tier !== "free" && rank > currentRank
                        return (
                            <article key={tier.tier} className="vipTierBox">
                                <div className="vipTierIcon">
                                    <VipBadge tier={tier.tier} src={tier.badge_url} size="lg" />
                                </div>
                                <h2>{tier.tier.replace("_", " ").toUpperCase()}</h2>
                                <p className="vipTierPrice">
                                    {dollars(tier.purchases.find(p => p.type === "monthly_subscription")?.amount_cents)} / month
                                </p>
                                <ul>
                                    <li>{tier.tier === "vip" ? "turquoise name" : "includes lower tiers"}</li>
                                    {tier.tier !== "vip" && <li>custom color and rainbow styles</li>}
                                    {tier.tier === "ultra_vip" && <li>per-letter colors, speed, direction, presets</li>}
                                </ul>
                                {isLower && (
                                    <p className="vipNotice">You already have {TIER_LABELS[vipState.active_tier]}. Buying this lower tier does nothing.</p>
                                )}
                                {isUpgrade && (
                                    <p className="vipNotice">Upgrade - a rollover discount applies from your current {TIER_LABELS[vipState.active_tier]} time.</p>
                                )}
                                <div className="vipCheckoutBtns">
                                    {tier.purchases.map(option => (
                                        <button
                                            key={option.type}
                                            type="button"
                                            disabled={isLower || !option.configured || busy === `${tier.tier}:${option.type}`}
                                            onClick={() => checkout(tier.tier, option.type)}
                                            title={option.configured ? "" : "Stripe is not configured"}
                                        >
                                            {option.type === "one_month" && `1 month ${dollars(option.amount_cents)}`}
                                            {option.type === "monthly_subscription" && `subscribe monthly ${dollars(option.amount_cents)}`}
                                            {option.type === "twelve_month" && `12 months ${dollars(option.amount_cents)}`}
                                        </button>
                                    ))}
                                </div>
                            </article>
                        )
                    })}
                </div>

                <section className="vipCurrentStatus">
                    <h2>Current Status</h2>
                    {user && vip ? (
                        <>
                            <div className="vipPreview">
                                <Username user={previewUser} size="lg" />
                            </div>
                            <div className="vipStatusRows">
                                <p>tier: <span>{vip.active_tier}</span></p>
                                <p>expires: <span>{dateText(vip.expires_at)}</span></p>
                                <p>subscription: <span>{vip.subscription?.status || "none"}</span></p>
                                <p>auto renew: <span>{vip.subscription && !vip.subscription.cancel_at_period_end ? "yes" : "no"}</span></p>
                            </div>
                            {ACTIVE_SUBSCRIPTION_STATUSES.has(vip.subscription?.status) ? (
                                <button type="button" onClick={manageSubscription} disabled={busy === "manage-subscription"}>
                                    manage subscription
                                </button>
                            ) : vip.active ? (
                                <p className="vipNotice">
                                    You have {TIER_LABELS[vip.active_tier]} VIP (prepaid) until {dateText(vip.expires_at)}. No active subscription.
                                </p>
                            ) : (
                                <p className="vipNotice">No active subscription detected!</p>
                            )}
                        </>
                    ) : (
                        <>
                            <p className="vipNotice">You're not signed in.</p>
                            <p>
                                <Link to={buildSigninPath(location.pathname)}>Sign in</Link> to see your VIP status and buy.
                            </p>
                        </>
                    )}
                    <p>Want to change your name style? Edit it on your <Link to="/profile">profile</Link> or from the profile menu.</p>
                    <p><Link to="/account">back to account</Link></p>
                </section>

                {/*
                <p className="vipDonationNotice">
                    VIP is a supporter thing NOT a paywall. NOT required for anything. MiMITA is free 100%.
                    You get a cool colorful name here on the website and in the game.
                    More supporter features will be added over time, 
                    but VIP will never!!!!!!!!!!!!!1 be the main focus 
                    and will never!!!!!!!!!!!!!!!!! block access to anything. 
                    If you don't have VIP you can still play and do everything. 
                    If u dont spend a single dollar in any currency u still will have 
                    1000% access to all fun things in MiMITA. 
                    It's strictly:
                    "I appreciate this and I want to support it."
                    None of this enshittification none of this
                    "Um actually it costs $29.99 to do the egg hunt and an additional $4.99 to get a sprint function"
                    like that is so lame i NEVER am ever gonna do that. EVER!!!!!!!
                    and if i ever DO something that even Remotly seems like that CALL ME OUT ON IT + SHOW ME THIS PAGE !!!!!!!!!!!
                </p>
                */}
            </section>
        </Layout>
    )
}
