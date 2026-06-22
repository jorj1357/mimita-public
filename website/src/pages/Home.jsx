import "../App.css"

import Layout from "../components/Layout"
import FeedbackBox from "../components/FeedbackBox"

export default function Home() {
  return (
    <Layout>

      <main className="hero">

        <video
          autoPlay
          loop
          muted
          playsInline
          preload="auto"
          className="bgVideo"
        >
          <source
            src="/untitled-loop-small.mp4"
            type="video/mp4"
          />
        </video>

        <div className="heroOverlay" />

        <div className="heroContent">

          <h1 className="mainLogo">
            MiMITA
          </h1>

          <p className="tagline">
            "Movement is More Important Than Aim"
          </p>

          <div className="previewArea">
            <video
              autoPlay
              loop
              muted
              playsInline
              preload="auto"
              className="previewVideo"
            >
              <source
                src="/mimita-preview-small.mp4"
                type="video/mp4"
              />
            </video>
          </div>

          <p className="description">
            Mimita is a movement-based PvP game where movement matters more than aim.
          </p>

          <div className="ctaButtons">
            <a
              href="https://github.com/jorj1357/mimita-public"
              target="_blank"
              rel="noopener noreferrer"
              className="downloadButton"
            >
              DOWNLOAD v1
            </a>

            <span className="platformLabel">
              windows 64-bit
            </span>
          </div>

          <div className="secondaryButtons">
            <a
              href="https://discord.gg/4MRjvpDu7e"
              target="_blank"
              rel="noopener noreferrer"
              className="secondaryButton"
            >
              Discord
            </a>

            <a
              href="https://github.com/jorj1357/mimita-public"
              target="_blank"
              rel="noopener noreferrer"
              className="secondaryButton"
            >
              GitHub
            </a>

            <a
              href="https://github.com/jorj1357/mimita-public/releases"
              target="_blank"
              rel="noopener noreferrer"
              className="secondaryButton"
            >
              Changelog
            </a>

            <a
              href="/about"
              className="secondaryButton"
            >
              Documentation
            </a>
          </div>

        </div>

      </main>

      <FeedbackBox pageName="home" />

    </Layout>
  )
}
