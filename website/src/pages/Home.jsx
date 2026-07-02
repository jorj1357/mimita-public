import "../App.css"

import Layout from "../components/Layout"
import FeedbackBox from "../components/FeedbackBox"
import RainbowText from "../components/RainbowText"
import Sticker from "../components/Sticker"
import PixelBox from "../components/PixelBox"
import RandomRotation from "../components/RandomRotation"
import ProceduralOffset from "../components/ProceduralOffset"
import NoiseBackground from "../components/NoiseBackground"
import BounceBall from "../components/BounceBall"

export default function Home() {
  return (
    <Layout>

      <BounceBall />

      <main className="hero heroNoise">

        <NoiseBackground opacity={0.02} style={{ position: "absolute", inset: 0 }} />

        <div className="stickerLayer">
          <Sticker index={0} />
          <Sticker index={1} />
          <Sticker index={2} />
          <Sticker index={3} />
          <Sticker index={4} />
          <Sticker index={5} />
        </div>

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

          <div className="heroLogoWrap">
            <RainbowText as="h1" className="mainLogo">
              MiMITA
            </RainbowText>
          </div>

          <RandomRotation maxDeg={1}>
            <p className="tagline">
              &ldquo;Movement is More Important Than Aim&rdquo;
            </p>
          </RandomRotation>

          <PixelBox style={{ width: "100%", maxWidth: "720px", margin: "0 auto 28px auto", border: "none" }}>
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
          </PixelBox>

          <ProceduralOffset max={2}>
            <p className="description">
              Mimita is a movement-based PvP game where movement matters more than aim.
            </p>
          </ProceduralOffset>

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
              href="https://discord.gg/sY8QHbfG9D"
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
