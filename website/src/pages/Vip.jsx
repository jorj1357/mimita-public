// 08 03 2026, 17 20
/* purpose
* Provides the functional MiMITA VIP purchase, status, and name-style customization page.
* Calls the real VIP APIs for checkout, subscription portal, style saving, reset, and presets.
* Shows locked controls as previews when the current account tier cannot use them.
* DOES NOT fake checkout success or grant VIP from redirect parameters.
* DOES NOT store Stripe Price IDs or secrets on the client.
* DOES NOT implement game-engine name rendering.
*/

import { useEffect, useMemo, useState } from "react"
import { Link, useLocation, useNavigate } from "react-router-dom"
import Layout from "../components/Layout"
import Username from "../components/Username"
import VipBadge from "../components/VipBadge"
import NameStyleEditor from "../components/NameStyleEditor"
import { normalizeStyle } from "../lib/vipStyle"
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

export default function Vip() {
    const navigate = useNavigate()
    const location = useLocation()
    const [user, setUser] = useState(null)
    const [config, setConfig] = useState(null)
    const [vip, setVip] = useState(null)
    const [style, setStyle] = useState(normalizeStyle(null))
    const [presets, setPresets] = useState([])
    const [presetName, setPresetName] = useState("")
    const [message, setMessage] = useState("loading VIP...")
    const [busy, setBusy] = useState("")

    useEffect(() => {
        let alive = true
        Promise.all([
            apiRequest("/api/auth/me"),
            apiRequest("/api/vip/config"),
            apiRequest("/api/vip/me")
        ])
            .then(([me, cfg, vipMe]) => {
                if (!alive) return
                setUser(me.user)
                setConfig(cfg.config)
                setVip(vipMe.vip)
                setStyle(normalizeStyle(vipMe.vip?.name_style))
                setMessage("")
                if (vipMe.vip?.active_tier === "ultra_vip") {
                    apiRequest("/api/vip/presets")
                        .then(data => {
                            if (alive) setPresets(data.presets || [])
                        })
                        .catch(() => {})
                }
            })
            .catch((error) => {
                if (error?.status === 401) {
                    logAuthEvent("redirect to signin", { returnTo: location.pathname })
                    navigate(buildSigninPath(location.pathname))
                }
                else {
                    setMessage(error?.message || "failed to load VIP")
                }
            })
        return () => { alive = false }
    }, [navigate, location.pathname])

    const previewUser = useMemo(() => {
        if (!user || !vip) return user
        return {
            ...user,
            supporter_tier: vip.active_tier,
            vip: {
                ...vip,
                name_style: style
            }
        }
    }, [user, vip, style])

    async function checkout(tier, purchaseType) {
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

    async function saveStyle() {
        setBusy("save-style")
        setMessage("")
        try {
            const data = await apiRequest("/api/vip/style", {
                method: "PATCH",
                body: JSON.stringify({ style })
            })
            setVip(data.vip)
            setStyle(normalizeStyle(data.vip?.name_style))
            setMessage("style saved")
        }
        catch (error) {
            setMessage(error.message)
        }
        finally {
            setBusy("")
        }
    }

    async function resetStyle() {
        setBusy("reset-style")
        try {
            const data = await apiRequest("/api/vip/style/reset", { method: "POST" })
            setVip(data.vip)
            setStyle(normalizeStyle(data.vip?.name_style))
            setMessage("style reset")
        }
        catch (error) {
            setMessage(error.message)
        }
        finally {
            setBusy("")
        }
    }

    async function manageSubscription() {
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

    async function savePreset() {
        setBusy("preset")
        try {
            const data = await apiRequest("/api/vip/presets", {
                method: "POST",
                body: JSON.stringify({ name: presetName, style })
            })
            setPresets(current => [data.preset, ...current])
            setPresetName("")
            setMessage("preset saved")
        }
        catch (error) {
            setMessage(error.message)
        }
        finally {
            setBusy("")
        }
    }

    async function deletePreset(id) {
        setBusy(`delete:${id}`)
        try {
            await apiRequest(`/api/vip/presets/${id}`, { method: "DELETE" })
            setPresets(current => current.filter(p => p.id !== id))
        }
        catch (error) {
            setMessage(error.message)
        }
        finally {
            setBusy("")
        }
    }

    if (!user || !config || !vip) {
        return (
            <Layout>
                <section className="vipPage">
                    <p>{message}</p>
                </section>
            </Layout>
        )
    }

    const isUltra = vip.active_tier === "ultra_vip"

    return (
        <Layout>
            <section className="vipPage">
                <h1>MiMITA VIP</h1>
                <p className="vipDonationNotice">
                    VIP is a supporter thing, not a paywall. You get a cool colorful name here on the website and in the game.
                    More supporter features will be added over time, but VIP will never be the main focus and will never block
                    access to anything. If you don't have VIP you can still play and do everything. It's strictly:
                    "I appreciate this and I want to support it."
                </p>
                <p className="vipNotice">
                    VIP belongs to your account. Checkout prices are selected by the server and confirmed by Stripe webhooks.
                </p>
                {message && <p className={message.includes("failed") || message.includes("not") ? "vipError" : "vipNotice"}>{message}</p>}

                <div className="vipGrid">
                    {config.tiers.map(tier => (
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
                            <div className="vipCheckoutBtns">
                                {tier.purchases.map(option => (
                                    <button
                                        key={option.type}
                                        type="button"
                                        disabled={!option.configured || busy === `${tier.tier}:${option.type}`}
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
                    ))}
                </div>

                <div className="vipGrid">
                    <section className="vipPanel">
                        <h2>Current Status</h2>
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
                        ) : (
                            <p className="vipNotice">You have no active subscription to manage.</p>
                        )}
                        <p><Link to="/account">back to account</Link></p>
                    </section>

                    <section className="vipPanel">
                        <h2>Name Style</h2>
                        <NameStyleEditor
                            user={user}
                            vip={vip}
                            style={style}
                            onChange={setStyle}
                            onSave={saveStyle}
                            onReset={resetStyle}
                            busy={busy}
                            limits={config.style_limits}
                        />
                    </section>

                    <section className={`vipPanel ${!isUltra ? "vipLocked" : ""}`}>
                        <h2>Ultra Presets</h2>
                        <input
                            type="text"
                            maxLength="40"
                            placeholder="preset name"
                            value={presetName}
                            disabled={!isUltra}
                            onChange={e => setPresetName(e.target.value)}
                        />
                        <button type="button" disabled={!isUltra || !presetName.trim() || busy === "preset"} onClick={savePreset}>
                            save preset
                        </button>
                        <div className="vipPresetList">
                            {presets.map(preset => (
                                <div key={preset.id} className="vipPresetItem">
                                    <button type="button" onClick={() => setStyle(normalizeStyle(preset.style_json))}>
                                        {preset.name}
                                    </button>
                                    <button type="button" disabled={busy === `delete:${preset.id}`} onClick={() => deletePreset(preset.id)}>
                                        delete
                                    </button>
                                </div>
                            ))}
                        </div>
                    </section>
                </div>
            </section>
        </Layout>
    )
}
