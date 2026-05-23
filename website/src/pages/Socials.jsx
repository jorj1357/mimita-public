import "../App.css"

import Layout from "../components/Layout"

export default function Socials() {
  return (
    <Layout>

      <div className="socialsPage">

        <h1 className="socialsTitle">
          MiMITA SOCIALS
        </h1>

        <div className="socialsLinks">

          <a
            href="https://www.youtube.com/@mimita_move"
            target="_blank"
            rel="noopener noreferrer"
            className="socialLink"
          >
            MiMITA : YOUTUBE
          </a>

          <a
            href="https://www.tiktok.com/@mimita.move"
            target="_blank"
            rel="noopener noreferrer"
            className="socialLink"
          >
            MiMITA : TIKTOK
          </a>

          <a
            href="https://www.instagram.com/mimita.move/"
            target="_blank"
            rel="noopener noreferrer"
            className="socialLink"
          >
            MiMITA : INSTAGRAM
          </a>

          <a
            href="https://bsky.app/profile/mimita-move.bsky.social"
            target="_blank"
            rel="noopener noreferrer"
            className="socialLink"
          >
            MiMITA : BLUESKY
          </a>

          <a
            href="https://mastodon.social/@MiMITA"
            target="_blank"
            rel="noopener noreferrer"
            className="socialLink"
          >
            MiMITA : MASTODON
          </a>

        </div>

      </div>

    </Layout>
  )
}