import { useState, useEffect, useRef, useCallback } from "react"

import "../App.css"

import Layout from "../components/Layout"
import FeedbackBox from "../components/FeedbackBox"
import { GAME_ZIP_DOWNLOAD_URL } from "../lib/downloadUrls"

export default function Download() {
  const [version, setVersion] = useState(null)
  const [error, setError] = useState(null)
  const [downloading, setDownloading] = useState(false)
  const downloadingRef = useRef(false)

  const doDownload = useCallback(() => {
    if (downloadingRef.current) return
    downloadingRef.current = true
    setDownloading(true)
    setError(null)
    fetch("/api/track/download", {
      method: "POST",
      credentials: "include",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify({ source: "website", platform: "windows" })
    }).catch(() => {})

    window.location.href = GAME_ZIP_DOWNLOAD_URL
    setTimeout(() => {
      if (!document.hidden) {
        setError("Download didn't start automatically. Click the button below.")
      }
      setDownloading(false)
      downloadingRef.current = false
    }, 3000)
  }, [])

  useEffect(() => {
    // Arrived here from the home-page button: the download may or may not have
    // started, so auto-trigger it again as a reliable fallback.
    if (sessionStorage.getItem("mimita_dl_auto") === "1") {
      sessionStorage.removeItem("mimita_dl_auto")
      doDownload()
    }
  }, [doDownload])

  useEffect(() => {
    fetch("/api/game/version")
      .then((res) => res.json())
      .then((data) => setVersion(data))
      .catch(() => {})
  }, [])

  const fileSize = version ? (version.file_size_mb || "~1") : "~1"

  return (
    <Layout>
      <div className="downloadPage">
        <video
          autoPlay
          loop
          muted
          playsInline
          preload="auto"
          className="downloadGif"
        >
          <source src="/mimita-preview-small-compressed.mp4" type="video/mp4" />
        </video>

        <button className="downloadButton" onClick={doDownload}
                disabled={downloading}>
          {downloading ? "DOWNLOADING..." : "DOWNLOAD MIMITA"}
        </button>

        {error && (
          <p className="downloadError">
            {error}
          </p>
        )}

        <p className="downloadInfo">
          download mimita-game-v2.0.5.zip (about {fileSize} MB) — no installer.
          Right-click the ZIP, choose Extract All, then run mimita.exe.
        </p>

        {version && (
          <div className="downloadVersion">
            <span>Version: {version.version}</span>
            <span>Released: {version.release_date}</span>
            <span>Size: {fileSize} MB</span>
          </div>
        )}

        <div className="downloadWhat">
          <h2 className="downloadWhatTitle">WHAT IS MIMITA?</h2>
          <p>
            MiMITA ("Movement is More Important Than Aim") is a FREE, awesome,
            open-source, movement-based PvP shooter for Windows 64-bit.
            <br/>
            u can dash, double jump, climb walls (by holding space and walking into it) 
            combining simple movement actions creates emergent movement tech. <br/>
            This is in the spirit of CS:GO surfing/kz/bhop,
            TF2 rocket jumping,
            rocket league freestyling,
            osu! constant movement,
            and Super Smash Bros. Melee-style jank.
            <br/>
            Over time, MiMITA should also become a hangout/creative platform:
            chat, create your own worlds/software/art, play casual/less competitive/more party games, synchronized worldwide music events,
            and weird worlds to explore.
          </p>
          <p>
            The game is built entirely from source in the public
            GitHub repository and released under the MIT License.
          </p>
        </div>

        <div className="downloadLegal">
          <p>
            Free code signing (hopefully ) provided by{" "}
            <a href="https://about.signpath.io" target="_blank" rel="noopener noreferrer">SignPath.io</a>,
            certificate by{" "}
            <a href="https://signpath.org" target="_blank" rel="noopener noreferrer">SignPath Foundation</a>.
          </p>
          <p>
            <a href="/privacy">Privacy Policy</a> ·{" "}
            <a href="/uninstall">Uninstall</a>
          </p>
        </div>
      </div>

      <FeedbackBox pageName="download" />
    </Layout>
  )
}
