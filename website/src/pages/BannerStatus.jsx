import { useEffect, useState } from "react"
import { Link } from "react-router-dom"
import { apiRequestRaw } from "../lib/api.js"
import Layout from "../components/Layout"
import "../styles/banner.css"

function fmtDate(iso) {
    if (!iso) return ""
    return new Date(iso).toLocaleString("en-US", {
        month: "short", day: "numeric", year: "numeric", hour: "numeric", minute: "2-digit"
    })
}

export default function BannerStatus() {
    const [data, setData] = useState(null)
    const [error, setError] = useState("")
    const [loading, setLoading] = useState(true)

    useEffect(() => {
        fetchMine()
    }, [])

    async function fetchMine() {
        setLoading(true)
        try {
            const { response, data: body } = await apiRequestRaw("/api/banner/mine")
            if (response.status === 401) { setError("sign in to see your banner status"); return }
            if (body?.success) setData(body)
            else setError(body?.message || "unable to load")
        }
        catch {
            setError("unable to load your banners")
        }
        finally {
            setLoading(false)
        }
    }

    function positionFor(bannerId) {
        if (!data?.schedule) return null
        if (data.schedule.active && data.schedule.active.id === bannerId) return 0
        const match = data.schedule.queue.find(q => q.banner.id === bannerId)
        return match ? match.position : null
    }

    if (loading) {
        return (
            <Layout>
                <div className="bannerCreator"><p className="aboutSmall">loading...</p></div>
            </Layout>
        )
    }

    if (error) {
        return (
            <Layout>
                <div className="bannerCreator">
                    <h1 className="bannerCreatorTitle">your banner</h1>
                    <p>{error} <Link to="/signin" style={{ color: "#40e0d0" }}>sign in</Link></p>
                </div>
            </Layout>
        )
    }

    const banners = data?.banners || []

    return (
        <Layout>
            <div className="bannerCreator">
                <h1 className="bannerCreatorTitle">your banner</h1>

                {banners.length === 0 ? (
                    <p>
                        You have not made a banner yet.{" "}
                        <Link to="/banner/create" style={{ color: "#40e0d0" }}>make your own banner</Link>
                    </p>
                ) : (
                    banners.map(b => {
                        const position = positionFor(b.id)
                        const queueEntry = position > 0 ? data.schedule.queue[position - 1] : null
                        return (
                            <div className="bannerStatusItem" key={b.id}>
                                <div className="siteBanner" style={{ backgroundColor: b.background_color, color: b.text_color }}>
                                    <div className="siteBannerMain">
                                        <span className="siteBannerMessage">{b.message}</span>
                                    </div>
                                </div>

                                {b.status === "active" && (
                                    <p><strong>Your banner is active now.</strong></p>
                                )}
                                {b.status === "queued" && position > 0 && (
                                    <p>
                                        <strong>Your banner is number {position} in line.</strong>{" "}
                                        Estimated start: {fmtDate(queueEntry?.estimated_start)}.
                                        Estimated end: {fmtDate(queueEntry?.estimated_end)}.
                                    </p>
                                )}
                                {b.status === "pending_payment" && (
                                    <p>Your banner is waiting for payment to be confirmed.</p>
                                )}
                                {b.status === "draft" && (
                                    <p>Your banner draft is not submitted yet.</p>
                                )}
                                {b.status === "disabled" && (
                                    <p>Your banner was disabled by an admin.</p>
                                )}
                                {b.status === "expired" && (
                                    <p>Your banner finished showing.</p>
                                )}
                                {b.status === "deleted" && (
                                    <p>Your banner was removed by an admin.</p>
                                )}

                                {b.status === "queued" && queueEntry && (
                                    <p className="bannerCreatorNote">
                                        Why it is queued: {queueEntry.reason}. Your purchased time has not been lost.
                                        Your full duration begins when your banner becomes active.
                                    </p>
                                )}

                                <p className="bannerCreatorNote">
                                    Purchased duration: {b.days} day{b.days === 1 ? "" : "s"}.
                                    {b.remaining_days != null ? ` Remaining: ${b.remaining_days.toFixed(2)} days.` : ""}
                                    {b.amount_cents != null ? ` Amount: $${(b.amount_cents / 100).toFixed(2)}.` : ""}
                                </p>
                            </div>
                        )
                    })
                )}

                <p className="bannerCreatorNote">
                    Banner purchases are fully refundable within 30 days of purchase.{" "}
                    <Link to="/support" style={{ color: "#40e0d0" }}>Go to /support</Link> and choose Payment or finance.
                    Lame ahhhhh admin has final say.
                </p>

                <button type="button" className="bannerCreatorSubmit" onClick={fetchMine}>refresh</button>
            </div>
        </Layout>
    )
}
