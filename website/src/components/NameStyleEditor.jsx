// 08 06 2026, 16 20
/* purpose
* Reusable VIP name-style editor with a live username preview.
* Renders style kind, solid color, rainbow speed/direction, and an interactive per-letter color editor.
* Used by the profile dropdown, the profile page, the account page, and the admin dashboard.
* DOES NOT grant entitlements, contact Stripe, or mutate account state.
* DOES NOT render game-engine nameplates.
*/

import { useId, useState } from "react"
import Username from "./Username"
import { STYLE_LABELS, STAFF_DISPLAY_LABELS, STAFF_COLORS, STAFF_ROLES } from "../lib/vipStyle"

const RESERVED_COLORS = new Set(["#000000", "#ff0000"])

function displayName(user) {
    return user?.display_name || user?.username || ""
}

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
    const [selectedLetters, setSelectedLetters] = useState(() => new Set())
    const [pickColor, setPickColor] = useState(() => style?.colors?.[0] || "#40e0d0")

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
    const maxPerLetter = Number(limits?.maxPerLetterColors || 32)
    const solidHex = String(style.solid_color || "").toLowerCase()
    const name = displayName(user)
    const isStaff = STAFF_ROLES.has(String(user.role || "").toLowerCase())
    const staffColor = STAFF_COLORS[style.staff_display] || ""
    const previewStyle = isStaff && staffColor
        ? { ...style, kind: "solid", solid_color: staffColor }
        : style

    function setColor(index, value) {
        const colors = [...(style.colors || [])]
        colors[index] = value
        onChange({ ...style, colors })
    }

    function letterColorAt(index) {
        const colors = style.colors || []
        if (colors.length === 0) return "#40e0d0"
        return colors[index % colors.length]
    }

    function toggleLetter(index) {
        setSelectedLetters(current => {
            const next = new Set(current)
            if (next.has(index)) next.delete(index)
            else next.add(index)
            return next
        })
    }

    function applyPickToSelected(event) {
        const hex = event.target.value
        setPickColor(hex)
        if (selectedLetters.size === 0) return
        const colors = [...(style.colors || [])]
        const maxIndex = Math.max(...selectedLetters)
        while (colors.length <= maxIndex) colors.push("#40e0d0")
        for (const index of selectedLetters) colors[index] = hex
        onChange({ ...style, colors })
    }

    function selectAllLetters() {
        const count = Math.min(name.length, maxPerLetter)
        setSelectedLetters(new Set(Array.from({ length: count }, (_, index) => index)))
    }

    function resetAllLetters() {
        onChange({ ...style, colors: [style.colors?.[0] || "#40e0d0"] })
    }

    return (
        <div className={`nameStyleEditor ${compact ? "nameStyleEditorCompact" : ""}`}>
            <div className="vipPreview">
                <Username user={previewUser} size="lg" style={previewStyle} />
            </div>

            {isStaff && (
                <label className="nameStyleField">
                    show color
                    <select
                        value={style.staff_display || "vip"}
                        onChange={e => onChange({ ...style, staff_display: e.target.value })}
                    >
                        {Object.entries(STAFF_DISPLAY_LABELS).map(([key, label]) => (
                            <option key={key} value={key}>{label}</option>
                        ))}
                    </select>
                </label>
            )}

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
                <>
                    <label className="nameStyleField">
                        solid color
                        <input
                            type="color"
                            value={style.solid_color || "#40e0d0"}
                            onChange={e => onChange({ ...style, solid_color: e.target.value })}
                        />
                    </label>
                    {RESERVED_COLORS.has(solidHex) && (
                        <p className="vipError">that color is reserved for staff - pick another</p>
                    )}
                </>
            )}

            {["rainbow", "animated_rainbow"].includes(style.kind) && (
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

            {style.kind === "per_letter" && (
                <div className="perLetterEditor">
                    <p className="nameStyleField">click letters to highlight them, then pick a color for the highlighted letters</p>
                    <div className="perLetterRow">
                        {Array.from(name).slice(0, maxPerLetter).map((ch, index) => (
                            <button
                                key={index}
                                type="button"
                                className={`perLetterCell${selectedLetters.has(index) ? " selected" : ""}`}
                                style={{ color: letterColorAt(index) }}
                                onClick={() => toggleLetter(index)}
                                aria-label={`select letter ${ch}`}
                            >
                                {ch}
                            </button>
                        ))}
                    </div>
                    {name.length > maxPerLetter && (
                        <p className="vipNotice">names longer than {maxPerLetter} letters: only the first {maxPerLetter} are editable.</p>
                    )}
                    <label className="nameStyleField">
                        highlighted letters color
                        <input
                            type="color"
                            value={pickColor}
                            onChange={applyPickToSelected}
                        />
                    </label>
                    <div className="vipCheckoutBtns">
                        <button type="button" onClick={selectAllLetters}>select all</button>
                        <button type="button" onClick={() => setSelectedLetters(new Set())}>clear selection</button>
                        <button type="button" onClick={resetAllLetters}>reset all letters</button>
                    </div>
                </div>
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
