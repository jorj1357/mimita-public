import "../App.css"

import Layout from "../components/Layout"
import NewsletterBox from "../components/NewsletterBox"
import FeedbackBox from "../components/FeedbackBox"
import RainbowText from "../components/RainbowText"
import StickerLayer from "../components/StickerLayer"
import PixelBox from "../components/PixelBox"
import RandomRotation from "../components/RandomRotation"
import ProceduralOffset from "../components/ProceduralOffset"
import NoiseBackground from "../components/NoiseBackground"
import ChaosBox from "../components/ChaosBox"
import ChaosSway from "../components/ChaosSway"
import RainbowTrail from "../components/RainbowTrail"
import BounceBall from "../components/BounceBall"
import DelayedRender from "../components/DelayedRender"
import { GAME_ZIP_DOWNLOAD_URL } from "../lib/downloadUrls"

export default function Home() {
  return (
    <Layout>

      <DelayedRender delayMs={500}>
        <BounceBall />
      </DelayedRender>

      <main className="hero heroNoise">

        <DelayedRender delayMs={300}>
          <NoiseBackground opacity={0.02} style={{ position: "absolute", inset: 0 }} />
        </DelayedRender>

        <DelayedRender delayMs={600}>
          <StickerLayer count={8} />
        </DelayedRender>

        <video
          autoPlay
          loop
          muted
          playsInline
          preload="none"
          className="bgVideo"
        >
          <source
            src="/untitled-loop-small.mp4"
            type="video/mp4"
          />
        </video>

        <div className="heroOverlay" />

        <div className="heroContent">

          <DelayedRender delayMs={400}>
            <RainbowTrail count={30}>
              <ChaosSway as="div">
                <div className="heroLogoWrap">
                  <RainbowText as="h1" className="mainLogo">
                    MiMITA
                  </RainbowText>
                </div>
              </ChaosSway>
            </RainbowTrail>
          </DelayedRender>

          <DelayedRender delayMs={500}>
            <ChaosBox seed="HomeTagline">
              <RandomRotation maxDeg={1}>
                <p className="tagline">
                  &ldquo;Movement is More Important Than Aim&rdquo;
                </p>
              </RandomRotation>
            </ChaosBox>
          </DelayedRender>

          <DelayedRender delayMs={700}>
            <ChaosBox seed="HomePreview">
              <PixelBox style={{ width: "100%", maxWidth: "720px", margin: "0 auto 28px auto", border: "none" }}>
                <div className="previewArea">
                  <video
                    autoPlay
                    loop
                    muted
                    playsInline
                    preload="none"
                    className="previewVideo"
                  >
                    <source
                      src="/mimita-preview-small-compressed.mp4"
                      type="video/mp4"
                    />
                  </video>
                </div>
              </PixelBox>
            </ChaosBox>
          </DelayedRender>

          <DelayedRender delayMs={600}>
            <ChaosBox seed="HomeDescription">
              <ProceduralOffset max={2}>
                <p className="description">
                  Mimita is a movement-based PvP game where movement matters more than aim.
                </p>
              </ProceduralOffset>
            </ChaosBox>
          </DelayedRender>

          <div className="ctaButtons">
            <a
              href={GAME_ZIP_DOWNLOAD_URL}
              className="downloadButton"
              onClick={() => {
                sessionStorage.setItem("mimita_dl_auto", "1")
                setTimeout(() => {
                  window.location.href = "/download"
                }, 1500)
              }}
            >
              DOWNLOAD MIMITA
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

          <div className="signPathLine">
            Free code signing provided by{" "}
            <a href="https://about.signpath.io" target="_blank" rel="noopener noreferrer">SignPath.io</a>,
            certificate by{" "}
            <a href="https://signpath.org" target="_blank" rel="noopener noreferrer">SignPath Foundation</a>.
            {" "}·{" "}
            <a href="/privacy">Privacy</a> ·{" "}
            <a href="/uninstall">Uninstall</a> ·{" "}
            <a
              href="https://github.com/jorj1357/mimita-public/blob/main/docs/code-signing-policy.md"
              target="_blank"
              rel="noopener noreferrer"
            >
              Code signing policy
            </a>
          </div>

        </div>

      </main>

      <div style={{ borderTop: "2px solid rgba(255,255,255,0.1)", margin: "0 auto", maxWidth: "500px", paddingTop: "2rem" }} />
      <NewsletterBox />
      <FeedbackBox pageName="home" />

    </Layout>
  )
}
