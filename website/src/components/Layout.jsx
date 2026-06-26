import { useEffect } from "react"
import { useLocation } from "react-router-dom"
import Header from "./Header"
import Footer from "./Footer"

export default function Layout({ children }) {
  const location = useLocation()

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

  return (
    <div className="app">
      <Header />
      <main className="pageContent">
        {children}
      </main>
      <Footer />
    </div>
  )
}