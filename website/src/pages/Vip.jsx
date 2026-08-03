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
import { Link, useNavigate } from "react-router-dom"
import Layout from "../components/Layout"
import Username from "../components/Username"
import { apiRequest } from "../lib/api"

const STYLE_LABELS = {
    vip_turquoise: "VIP turquoise",
    rainbow: "Rainbow",
    solid: "Solid color",
    animated_rainbow: "Animated rainbow",
    per_letter: "Per-letter",
    color_cycle: "Color cycle"
}

function dollars(cents) {
    return `$${(Number(cents || 0) / 100).toFixed(2)}`
}

function dateText(iso) {
    if (!iso) return "none"
    return new Date(iso).toLocaleString()
}

function normalizeStyle(style) {
    return {
        version: 1,
        kind: style?.kind || "vip_turquoise",
        solid_color: style?.solid_color || "#40e0d0",
        colors: Array.isArray(style?.colors) && style.colors.length
            ? style.colors
            : ["#ff0044", "#ffcc00", "#00ff66", "#00ccff", "#9944ff"],
        rainbow_speed: Number(style?.rainbow_speed || 1),
        rainbow_direction: style?.rainbow_direction || "ltr",
        animation: style?.animation || "cycle"
    }
}

export default function Vip() {
    const navigate = useNavigate()
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
            .catch(() => navigate("/signin"))
        return () => { alive = false }
    }, [navigate])

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

    function setColor(index, value) {
        setStyle(current => {
            const colors = [...(current.colors || [])]
            colors[index] = value
            return { ...current, colors }
        })
    }

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

    const allowed = new Set(vip.allowed_styles || [])
    const isUltra = vip.active_tier === "ultra_vip"

    return (
        <Layout>
            <section className="vipPage">
                <h1>MiMITA VIP</h1>
                <p className="vipNotice">
                    VIP belongs to your account. Checkout only starts when a Stripe test Price ID is configured on the server.
                </p>
                {message && <p className={message.includes("failed") || message.includes("not") ? "vipError" : "vipNotice"}>{message}</p>}

                <div className="vipGrid">
                    {config.tiers.map(tier => (
                        <article key={tier.tier} className="vipTierBox">
                            <h2>{tier.tier.replace("_", " ").toUpperCase()}</h2>
                            <img className="vipBadgeImg" src={tier.badge_url} alt={`${tier.tier} badge`} />
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
                                        title={option.configured ? "" : "server Price ID missing"}
                                    >
                                        {option.type === "one_month" && `1 month ${dollars(option.amount_cents)}`}
                                        {option.type === "monthly_subscription" && `subscribe ${dollars(option.amount_cents)}`}
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
                        <button type="button" onClick={manageSubscription} disabled={busy === "manage-subscription"}>
                            manage subscription
                        </button>
                        <p><Link to="/account">back to account</Link></p>
                    </section>

                    <section className="vipPanel">
                        <h2>Name Style</h2>
                        <div className="vipStyleGrid">
                            {Object.entries(STYLE_LABELS).map(([kind, label]) => (
                                <label key={kind} className={!allowed.has(kind) ? "vipLocked" : ""}>
                                    <input
                                        type="radio"
                                        name="vip-style-kind"
                                        value={kind}
                                        checked={style.kind === kind}
                                        disabled={!allowed.has(kind)}
                                        onChange={() => setStyle(current => ({ ...current, kind }))}
                                    />
                                    {label}
                                </label>
                            ))}
                        </div>

                        {style.kind === "solid" && (
                            <label>
                                solid color
                                <input
                                    type="color"
                                    value={style.solid_color || "#40e0d0"}
                                    onChange={e => setStyle(current => ({ ...current, solid_color: e.target.value }))}
                                />
                            </label>
                        )}

                        {["rainbow", "animated_rainbow", "per_letter", "color_cycle"].includes(style.kind) && (
                            <>
                                <label>
                                    speed
                                    <input
                                        type="range"
                                        min="0.15"
                                        max="2"
                                        step="0.05"
                                        value={style.rainbow_speed || 1}
                                        disabled={!isUltra && style.kind !== "rainbow"}
                                        onChange={e => setStyle(current => ({ ...current, rainbow_speed: Number(e.target.value) }))}
                                    />
                                </label>
                                <label>
                                    direction
                                    <select
                                        value={style.rainbow_direction || "ltr"}
                                        disabled={!isUltra}
                                        onChange={e => setStyle(current => ({ ...current, rainbow_direction: e.target.value }))}
                                    >
                                        <option value="ltr">left to right</option>
                                        <option value="rtl">right to left</option>
                                    </select>
                                </label>
                                <div className="vipColorList">
                                    {(style.colors || []).slice(0, 8).map((color, index) => (
                                        <input
                                            key={index}
                                            type="color"
                                            value={color}
                                            disabled={style.kind === "rainbow" && !isUltra}
                                            onChange={e => setColor(index, e.target.value)}
                                        />
                                    ))}
                                </div>
                            </>
                        )}

                        <div className="vipCheckoutBtns">
                            <button type="button" onClick={saveStyle} disabled={!vip.controls_unlocked || busy === "save-style"}>
                                save style
                            </button>
                            <button type="button" onClick={resetStyle} disabled={busy === "reset-style"}>
                                reset default
                            </button>
                        </div>
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
