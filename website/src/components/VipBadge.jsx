// 08 05 2026, 00 00
/* purpose
* Renders the canonical VIP badge image for website username displays.
* Keeps badge asset paths and sizing rules in one reusable React component.
* Provides safe fallback behavior when a row has no paid VIP tier.
* DOES NOT calculate entitlements or decide which users are VIP.
* DOES NOT create Stripe checkout sessions or mutate account style.
* DOES NOT render game-engine badge textures.
*/

import { useNavigate } from "react-router-dom"

const BADGE_IMAGES = {
    vip: "/assets/images/mimita%20vip%202.png",
    super_vip: "/assets/images/mimita%20super%20vip.png",
    ultra_vip: "/assets/images/mimita%20ultra%20vip.png"
}

const BADGE_LABELS = {
    vip: "VIP",
    super_vip: "SUPER VIP",
    ultra_vip: "ULTRA VIP"
}

function normalizeTier(tier) {
    const value = String(tier || "").trim().toLowerCase()
    return BADGE_IMAGES[value] ? value : ""
}

export default function VipBadge({ tier, src = "", size = "md" }) {
    const navigate = useNavigate()
    const normalized = normalizeTier(tier)
    const badgeSrc = src || BADGE_IMAGES[normalized]
    if (!normalized || !badgeSrc) return null

    const sizeClass = size === "sm" ? "vipBadgeSm" : size === "lg" ? "vipBadgeLg" : "vipBadgeMd"

    function openVip(event) {
        event.preventDefault()
        event.stopPropagation()
        navigate("/vip")
    }

    return (
        <span
            role="button"
            tabIndex={0}
            className="vipBadgeBtn"
            title={`${BADGE_LABELS[normalized]} - view VIP`}
            onClick={openVip}
            onKeyDown={event => {
                if (event.key === "Enter" || event.key === " ") {
                    event.preventDefault()
                    openVip(event)
                }
            }}
        >
            <img
                className={`vipBadgeImg ${sizeClass}`}
                src={badgeSrc}
                alt={BADGE_LABELS[normalized]}
                loading="lazy"
            />
        </span>
    )
}

export { BADGE_IMAGES, BADGE_LABELS }
