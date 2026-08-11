import { useState, useEffect, useRef, useCallback } from "react"

import "../App.css"

import Layout from "../components/Layout"
import FeedbackBox from "../components/FeedbackBox"

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

    window.location.href = "/api/download/latest"
    setTimeout(() => {
      if (!document.hidden) {
        setError("Download didn't start automatically. Click the button below.")
      }
      setDownloading(false)
      downloadingRef.current = false
    }, 3000)
  }, [])

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
          download MimitaLauncher.exe (about {fileSize} MB) — it installs the
          game and updates it automatically
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
            MiMITA ("Movement is More Important Than Aim") is a free,
            open-source, movement-based PvP shooter for Windows 64-bit.
            Dash, double jump, and climb walls as atomic actions —
            combining them creates emergent movement tech
            in the spirit of CS:GO surfing/kz/bhop,
            TF2 rocket jumping,
            and Super Smash Bros. Melee-style jank.
            It is also a hangout platform:
            chat, casual party games, synchronized music,
            and weird worlds to explore.
          </p>
          <p>
            The game and launcher are built entirely from source in the public
            GitHub repository and released under the MIT License.
          </p>
        </div>

        <div className="downloadLegal">
          <p>
            Free code signing provided by{" "}
            <a href="https://about.signpath.io" target="_blank" rel="noopener noreferrer">SignPath.io</a>,
            certificate by{" "}
            <a href="https://signpath.org" target="_blank" rel="noopener noreferrer">SignPath Foundation</a>.
          </p>
          <p>
            <a href="/privacy">Privacy Policy</a> ·{" "}
            <a href="/uninstall">Uninstall</a> ·{" "}
            <a href="https://github.com/jorj1357/mimita-public/blob/main/docs/code-signing-policy.md" target="_blank" rel="noopener noreferrer">Code signing policy</a> ·{" "}
            <a href="https://github.com/jorj1357/mimita-public/blob/main/LICENSE" target="_blank" rel="noopener noreferrer">License</a>
          </p>
        </div>
      </div>

      <FeedbackBox pageName="download" />
    </Layout>
  )
}
