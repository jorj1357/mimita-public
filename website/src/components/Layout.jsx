import { useEffect, useState } from "react"
import { useLocation } from "react-router-dom"
import Header from "./Header"
import Footer from "./Footer"
import SiteBanner from "./SiteBanner"

export default function Layout({ children }) {
  const location = useLocation()
  const [emailBanner, setEmailBanner] = useState(false)
  const [emailBannerDismissed, setEmailBannerDismissed] = useState(false)

  useEffect(() => {
    try {
      fetch("/api/game/analytics/events", {
        method: "POST",
        credentials: "include",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify({
          event_name: "page_visit",
          event_data: { page_url: location.pathname }
        })
      }).catch(() => {})
    }
    catch {
      // page visit tracking failure is non-critical
    }
  }, [location.pathname])

  useEffect(() => {
    fetch("/api/auth/me", { credentials: "include" })
      .then(r => r.json())
      .then(data => {
        if (data.success && data.user && !data.user.email_verified) {
          setEmailBanner(true)
        }
      })
      .catch(() => {})
  }, [])

  return (
    <div className="app">
      <SiteBanner />
      <Header />
      {emailBanner && !emailBannerDismissed && (
        <div className="emailVerifyBanner">
          <p>
            Confirm your email to prove you&apos;re a real player, not a bot. Bots spam stuff
            that real players could be doing, so confirming helps keep Mimita clean.
            You&apos;ll also get a <strong>Confirmed Email</strong> achievement.
          </p>
          <button
            type="button"
            className="emailVerifyDismiss"
            onClick={() => setEmailBannerDismissed(true)}
          >
            &times;
          </button>
        </div>
      )}
      <main className="pageContent">
        {children}
      </main>
      <Footer />
    </div>
  )
}