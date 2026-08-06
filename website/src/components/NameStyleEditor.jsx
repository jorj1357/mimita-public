// 08 06 2026, 16 20
/* purpose
* Reusable VIP name-style editor with a live username preview.
* Renders style kind, solid color, rainbow speed, direction, and color controls plus save/reset actions.
* Used by the VIP page, the profile dropdown, the profile page, and the account page.
* DOES NOT grant entitlements, contact Stripe, or mutate account state.
* DOES NOT render game-engine nameplates.
*/

import { useId } from "react"
import Username from "./Username"
import { STYLE_LABELS } from "../lib/vipStyle"

export default function NameStyleEditor({
    user,
    vip,
    style,
    onChange,
    onSave,
    onReset,
    busy = "",
    compact = false,
    limits = null,
    admin = false
}) {
    const kindName = `vip-style-kind-${useId()}`
    if (!user || !vip || !style) return null

    const allowed = admin
        ? new Set(Object.keys(STYLE_LABELS))
        : new Set(vip.allowed_styles || [])
    const isUltra = admin || vip.active_tier === "ultra_vip"
    const saveEnabled = admin || vip.controls_unlocked
    const previewUser = {
        ...user,
        supporter_tier: vip.active_tier,
        vip: {
            ...vip,
            name_style: style
        }
    }

    const minSpeed = Number(limits?.minRainbowSpeed || 0.25)
    const maxSpeed = Number(limits?.maxRainbowSpeed || 4)

    function setColor(index, value) {
        const colors = [...(style.colors || [])]
        colors[index] = value
        onChange({ ...style, colors })
    }

    return (
        <div className={`nameStyleEditor ${compact ? "nameStyleEditorCompact" : ""}`}>
            <div className="vipPreview">
                <Username user={previewUser} size="lg" style={style} />
            </div>

            <div className="vipStyleGrid">
                {Object.entries(STYLE_LABELS).map(([kind, label]) => (
                    <label key={kind} className={!allowed.has(kind) ? "vipLocked" : ""}>
                        <input
                            type="radio"
                            name={kindName}
                            value={kind}
                            checked={style.kind === kind}
                            disabled={!allowed.has(kind)}
                            onChange={() => onChange({ ...style, kind })}
                        />
                        {label}
                    </label>
                ))}
            </div>

            {style.kind === "solid" && (
                <label className="nameStyleField">
                    solid color
                    <input
                        type="color"
                        value={style.solid_color || "#40e0d0"}
                        onChange={e => onChange({ ...style, solid_color: e.target.value })}
                    />
                </label>
            )}

            {["rainbow", "animated_rainbow", "per_letter", "color_cycle"].includes(style.kind) && (
                <>
                    <label className="nameStyleField">
                        speed
                        <input
                            type="range"
                            min={minSpeed}
                            max={maxSpeed}
                            step="0.05"
                            value={style.rainbow_speed || 1}
                            disabled={!isUltra && style.kind !== "rainbow"}
                            onChange={e => onChange({ ...style, rainbow_speed: Number(e.target.value) })}
                        />
                    </label>
                    <label className="nameStyleField">
                        direction
                        <select
                            value={style.rainbow_direction || "ltr"}
                            disabled={!isUltra}
                            onChange={e => onChange({ ...style, rainbow_direction: e.target.value })}
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
                <button type="button" onClick={onSave} disabled={!saveEnabled || busy === "save-style"}>
                    save style
                </button>
                <button type="button" onClick={onReset} disabled={busy === "reset-style"}>
                    reset default
                </button>
            </div>
        </div>
    )
}
