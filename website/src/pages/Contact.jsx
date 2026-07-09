import "../App.css"

import Layout from "../components/Layout"

export default function Contact() {
  return (
    <Layout>
      <div className="socialsPage">
        <h1 className="socialsTitle">CONTACT</h1>

        <div className="socialsLinks">
          <p style={{ textAlign: "center", marginBottom: "1.5rem", color: "rgba(255,255,255,0.7)" }}>
            Email: <a href="mailto:hello@mimita.fun" style={{ color: "#a020ff" }}>hello@mimita.fun</a>
          </p>

          <a href="https://www.youtube.com/@mimita_move" target="_blank" rel="noopener noreferrer" className="socialLink">
            MiMITA : YOUTUBE
          </a>
          <a href="https://www.tiktok.com/@mimita.move" target="_blank" rel="noopener noreferrer" className="socialLink">
            MiMITA : TIKTOK
          </a>
          <a href="https://www.instagram.com/mimita.move/" target="_blank" rel="noopener noreferrer" className="socialLink">
            MiMITA : INSTAGRAM
          </a>
          <a href="https://bsky.app/profile/mimita-move.bsky.social" target="_blank" rel="noopener noreferrer" className="socialLink">
            MiMITA : BLUESKY
          </a>
          <a href="https://mastodon.social/@MiMITA" target="_blank" rel="noopener noreferrer" className="socialLink">
            MiMITA : MASTODON
          </a>
        </div>
      </div>
    </Layout>
  )
}
