import { useEffect, useState } from "react"
import { Link, useLocation } from "react-router-dom"
import { apiRequest } from "../lib/api.js"
import { formatCountdown, isBannerCollapsed, setBannerCollapsed } from "../lib/bannerUtils.js"
import "../styles/banner.css"

const CACHE_TTL_MS = 30_000
const EMPTY_SLOT_KEY = "placeholder"
let bannerCache = { at: 0, banner: null }

async function fetchActiveBanner() {
    if (Date.now() - bannerCache.at < CACHE_TTL_MS) return bannerCache.banner
    try {
        const res = await fetch("/api/site/banner", { credentials: "include" })
        const data = await res.json()
        bannerCache = { at: Date.now(), banner: data?.banner || null }
    }
    catch {
        // keep the previous cache on transient errors
    }
    return bannerCache.banner
}

function formatUtc(iso) {
    if (!iso) return ""
    return String(iso).replace("T", " ").replace(/\.\d+Z$/, " UTC")
}

export default function SiteBanner() {
    const location = useLocation()
    const [banner, setBanner] = useState(null)
    const [now, setNow] = useState(0)
    const [collapsed, setCollapsed] = useState(false)
    const [reported, setReported] = useState(false)
    const [reportError, setReportError] = useState("")

    useEffect(() => {
        let active = true
        fetchActiveBanner().then(b => {
            if (active) {
                setBanner(b)
                if (b) setCollapsed(isBannerCollapsed(b.id))
                else setCollapsed(isBannerCollapsed(EMPTY_SLOT_KEY))
            }
        })
        return () => { active = false }
    }, [location.pathname])

    useEffect(() => {
        if (!banner) return
        const tick = () => setNow(Date.now())
        tick()
        const timer = setInterval(tick, 1000)
        return () => clearInterval(timer)
    }, [banner])

    const collapseKey = banner ? banner.id : EMPTY_SLOT_KEY

    function toggleCollapse() {
        const next = !collapsed
        setCollapsed(next)
        setBannerCollapsed(collapseKey, next)
    }

    if (!banner) {
        if (collapsed) {
            return (
                <div className="siteBanner siteBannerCollapsed siteBannerPlaceholder">
                    <span className="siteBannerCollapsedText">Banner hidden</span>
                    <button
                        type="button"
                        className="siteBannerCollapseArrow"
                        onClick={toggleCollapse}
                        aria-expanded="false"
                        aria-label="expand banner"
                    >
                        ▸
                    </button>
                </div>
            )
        }
        return (
            <div className="siteBanner siteBannerPlaceholder">
                <div className="siteBannerMain">
                    <Link className="siteBannerMessage" to="/banner/create">
                        your banner here!
                    </Link>
                    <span className="siteBannerInfo">
                        make a free 1-day banner — you just need an account
                    </span>
                </div>
                <div className="siteBannerActions">
                    <button
                        type="button"
                        className="siteBannerCollapseArrow"
                        onClick={toggleCollapse}
                        aria-expanded="true"
                        aria-label="collapse banner"
                    >
                        ▾
                    </button>
                </div>
            </div>
        )
    }

    async function handleReport() {
        setReportError("")
        try {
            await apiRequest("/api/banner/report", {
                method: "POST",
                body: JSON.stringify({ banner_id: banner.id })
            })
            setReported(true)
        }
        catch (e) {
            setReportError(e.status === 401 ? "sign in to report" : "report failed")
        }
    }

    if (collapsed) {
        return (
            <div
                className="siteBanner siteBannerCollapsed"
                style={{ backgroundColor: banner.background_color, color: banner.text_color }}
            >
                <span className="siteBannerCollapsedText">Banner hidden</span>
                <button
                    type="button"
                    className="siteBannerCollapseArrow"
                    onClick={toggleCollapse}
                    aria-expanded="false"
                    aria-label="expand banner"
                >
                    ▸
                </button>
            </div>
        )
    }

    return (
        <div
            className="siteBanner"
            style={{ backgroundColor: banner.background_color, color: banner.text_color }}
        >
            <div className="siteBannerMain">
                {banner.target_url ? (
                    <a
                        className="siteBannerMessage"
                        href={banner.target_url}
                        target="_blank"
                        rel="noopener noreferrer"
                    >
                        {banner.message}
                    </a>
                ) : (
                    <span className="siteBannerMessage">{banner.message}</span>
                )}
                <span className="siteBannerInfo">
                    by{" "}
                    <Link className="siteBannerAuthor" to={`/users/${encodeURIComponent(banner.username)}`}>
                        {banner.username}
                    </Link>
                    {" · "}
                    {formatUtc(banner.created_at)}
                    {" · "}
                    {formatCountdown(banner.expires_at, now)} left
                </span>
            </div>
            <div className="siteBannerActions">
                <Link className="siteBannerBtn" to="/banner/create">make your own banner</Link>
                {reported ? (
                    <span className="siteBannerBtn">reported ✓</span>
                ) : (
                    <button type="button" className="siteBannerBtn" onClick={handleReport}>
                        report this banner
                    </button>
                )}
                <button
                    type="button"
                    className="siteBannerCollapseArrow"
                    onClick={toggleCollapse}
                    aria-expanded="true"
                    aria-label="collapse banner"
                >
                    ▾
                </button>
            </div>
            {reportError && <span className="siteBannerError">{reportError}</span>}
        </div>
    )
}
