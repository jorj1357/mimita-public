import { useEffect, useRef, useState } from "react"
import { Link, useSearchParams } from "react-router-dom"
import Layout from "../components/Layout"
import "../styles/banner.css"

function fmtDate(iso) {
    if (!iso) return ""
    return new Date(iso).toLocaleString("en-US", {
        month: "short", day: "numeric", year: "numeric", hour: "numeric", minute: "2-digit"
    })
}

export default function BannerSuccess() {
    const [params] = useSearchParams()
    const orderId = params.get("order_id")
    const [state, setState] = useState(orderId ? "loading" : "error")
    const [data, setData] = useState(null)
    const timerRef = useRef(null)

    useEffect(() => {
        if (!orderId) return
        let cancelled = false
        let attempts = 0
        const maxAttempts = 10

        async function tick() {
            if (cancelled) return
            try {
                const res = await fetch(`/api/banner/orders/${orderId}`, { credentials: "include" })
                if (cancelled) return
                if (res.status === 401) { setState("signin"); return }
                const body = await res.json()
                if (body?.success) {
                    setData(body)
                    if (body.order?.status === "paid") { setState("confirmed"); return }
                }
                attempts += 1
                if (attempts >= maxAttempts) { setState("delayed"); return }
                timerRef.current = setTimeout(tick, 2000)
            }
            catch {
                if (cancelled) return
                attempts += 1
                if (attempts >= maxAttempts) { setState("delayed"); return }
                timerRef.current = setTimeout(tick, 2000)
            }
        }
        tick()
        return () => { cancelled = true; clearTimeout(timerRef.current) }
    }, [orderId])

    function refresh() {
        window.location.reload()
    }

    const order = data?.order
    const banner = data?.banner
    const position = data?.position

    return (
        <Layout>
            <div className="bannerCreator bannerSuccess">
                <h1 className="bannerCreatorTitle">
                    {state === "confirmed" ? "You just got a banner!" : "Banner purchase"}
                </h1>

                {state === "loading" && (
                    <>
                        <div className="bannerSuccessSpinner" aria-hidden="true" />
                        <p>Confirming your banner purchase...</p>
                        <p>Stripe has completed your payment. We are verifying it and placing your banner now.</p>
                    </>
                )}

                {state === "signin" && (
                    <p>
                        Sign in to see your banner status.{" "}
                        <Link to="/signin" style={{ color: "#40e0d0" }}>sign in</Link>
                    </p>
                )}

                {state === "delayed" && (
                    <>
                        <p>
                            Stripe completed the checkout, but the confirmation is taking a little longer than expected.
                            Your purchase is safe. Refresh this page in a moment or contact support if it does not update.
                        </p>
                        <div className="bannerSuccessActions">
                            <button type="button" className="bannerCreatorSubmit" onClick={refresh}>refresh</button>
                            <Link className="bannerCreatorSubmit bannerSuccessLink" to="/support">contact support</Link>
                        </div>
                    </>
                )}

                {state === "error" && (
                    <p>
                        No order to confirm.{" "}
                        <Link to="/banner/create" style={{ color: "#40e0d0" }}>make a banner</Link>
                    </p>
                )}

                {state === "confirmed" && order && (
                    <>
                        {banner && (
                            <div className="siteBanner" style={{ backgroundColor: banner.background_color, color: banner.text_color }}>
                                <div className="siteBannerMain">
                                    <span className="siteBannerMessage">{banner.message}</span>
                                </div>
                            </div>
                        )}

                        {position === 0 ? (
                            <p>
                                Your banner starts showing now.{" "}
                                {banner?.expires_at && `It will show until ${fmtDate(banner.expires_at)}.`}
                            </p>
                        ) : position ? (
                            <p>
                                Your banner is currently number {position} in line.{" "}
                                It is estimated to start on {fmtDate(data?.schedule?.queue?.[position - 1]?.estimated_start)}
                                {" "}and show until {fmtDate(data?.schedule?.queue?.[position - 1]?.estimated_end)}.
                            </p>
                        ) : (
                            <p>Your banner is on the way. It will appear when the current slot frees up.</p>
                        )}

                        <p className="bannerCreatorNote">
                            {banner?.days != null && `Duration: ${banner.days} day${banner.days === 1 ? "" : "s"}.`}{" "}
                            {order?.amount_cents != null && `Amount paid: $${(order.amount_cents / 100).toFixed(2)}.`}
                        </p>

                        <p className="bannerCreatorNote">
                            A paid banner with a higher amount can replace a lower paid banner. A free banner can be
                            replaced by another free banner. Your purchased time is not lost when this happens.
                            Your full duration begins when your banner becomes active.
                        </p>

                        <div className="bannerSuccessActions">
                            <Link className="bannerCreatorSubmit bannerSuccessLink" to="/">View the home page</Link>
                            <Link className="bannerCreatorSubmit bannerSuccessLink" to="/banner/status">Your banner status</Link>
                            <Link className="bannerCreatorSubmit bannerSuccessLink" to="/support">Support</Link>
                        </div>
                    </>
                )}
            </div>
        </Layout>
    )
}
