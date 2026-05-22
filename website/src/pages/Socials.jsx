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
            href="/"
            className="socialLink"
          >
            MiMITA : YOUTUBE
          </a>

          <a
            href="/"
            className="socialLink"
          >
            MiMITA : TIKTOK
          </a>

          <a
            href="/"
            className="socialLink"
          >
            MiMITA : INSTAGRAM
          </a>

          <a
            href="/"
            className="socialLink"
          >
            MiMITA : BLUESKY
          </a>

          <a
            href="/"
            className="socialLink"
          >
            MiMITA : MASTODON
          </a>

          <a
            href="/"
            className="socialLink"
          >
            MiMITA : MIMITA
          </a>

        </div>

      </div>

    </Layout>
  )
}