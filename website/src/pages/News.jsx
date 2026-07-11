import { useState, useEffect } from "react"
import { Link } from "react-router-dom"
import "../App.css"
import Layout from "../components/Layout"
import RainbowText from "../components/RainbowText"
import PixelBox from "../components/PixelBox"

export default function News() {
  const [newsItems, setNewsItems] = useState([])
  const [loading, setLoading] = useState(true)

  useEffect(() => {
    fetch("/news.generated.json")
      .then(r => r.json())
      .then(data => {
        setNewsItems(data)
        setLoading(false)
      })
      .catch(() => {
        setNewsItems([])
        setLoading(false)
      })
  }, [])

  return (
    <Layout>
      <div className="aboutPage">
        <div className="aboutContent">
          <h1 className="aboutTitle">
            <RainbowText as="span">MiMITA News!!!</RainbowText>
          </h1>

          <PixelBox style={{ marginBottom: "2rem", textAlign: "center" }}>
            <p style={{ color: "rgba(255,255,255,0.8)", marginBottom: "0.5rem" }}>
              Sign up and get emailed whenever we put new news!!!
            </p>
            <Link to="/signup" className="downloadBtn" style={{ display: "inline-block", marginTop: "0.5rem" }}>
              Create Account
            </Link>
          </PixelBox>

          {loading ? (
            <PixelBox style={{ marginBottom: "1rem" }}>
              <p style={{ color: "rgba(255,255,255,0.6)" }}>Loading news...</p>
            </PixelBox>
          ) : newsItems.length === 0 ? (
            <PixelBox style={{ marginBottom: "1rem" }}>
              <p style={{ color: "rgba(255,255,255,0.6)" }}>No news yet.</p>
            </PixelBox>
          ) : (
            newsItems.map((item, i) => (
              <PixelBox key={i} style={{ marginBottom: "1rem" }}>
                {item.url ? (
                  <Link to={item.url} style={{ textDecoration: "none" }}>
                    <h3 style={{ color: "#a020ff", marginBottom: "0.25rem" }}>{item.title}</h3>
                  </Link>
                ) : (
                  <h3 style={{ color: "#a020ff", marginBottom: "0.25rem" }}>{item.title}</h3>
                )}
                <p style={{ color: "rgba(255,255,255,0.4)", fontSize: "0.8rem", marginBottom: "0.75rem" }}>{item.date}</p>
                <p style={{ color: "rgba(255,255,255,0.8)", lineHeight: 1.6 }}>{item.description}</p>
              </PixelBox>
            ))
          )}
        </div>
      </div>
    </Layout>
  )
}
