import { Link } from "react-router-dom"
import "../App.css"
import Layout from "../components/Layout"
import RainbowText from "../components/RainbowText"
import PixelBox from "../components/PixelBox"

const newsItems = [
  {
    title: "Welcome to MiMITA News!",
    date: "2026-07-08",
    content: "MiMITA is in active development. Check back here for patch notes, game updates, and announcements. This feed will soon be powered by admin-authored markdown posts."
  }
]

export default function News() {
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

          {newsItems.map((item, i) => (
            <PixelBox key={i} style={{ marginBottom: "1rem" }}>
              <h3 style={{ color: "#a020ff", marginBottom: "0.25rem" }}>{item.title}</h3>
              <p style={{ color: "rgba(255,255,255,0.4)", fontSize: "0.8rem", marginBottom: "0.75rem" }}>{item.date}</p>
              <p style={{ color: "rgba(255,255,255,0.8)", lineHeight: 1.6 }}>{item.content}</p>
            </PixelBox>
          ))}
        </div>
      </div>
    </Layout>
  )
}
