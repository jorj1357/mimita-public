import { useState, useEffect, useRef, useCallback } from "react"

import "../App.css"

import Layout from "../components/Layout"
import FeedbackBox from "../components/FeedbackBox"
import { PORTABLE_ZIP_DOWNLOAD_URL } from "../lib/downloadUrls"

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

        <div className="portableZipSection">
          <a
            href={PORTABLE_ZIP_DOWNLOAD_URL}
            className="portableZipButton"
          >
            Download Portable ZIP
          </a>
          <p className="portableZipInfo">
            No installer. Right-click the ZIP, choose Extract All, then open the
            folder and run mimita.exe.
          </p>
          <ol className="portableZipSteps">
            <li>Download the ZIP</li>
            <li>Right-click &rarr; Extract All</li>
            <li>Open the extracted folder</li>
            <li>Run <code>mimita.exe</code></li>
          </ol>
        </div>

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
            The game and launcher are built entirely from source in the public
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
            <a href="/uninstall">Uninstall</a> ·{" "}
            {/* <a href="https://github.com/jorj1357/mimita-public/blob/main/docs/code-signing-policy.md" target="_blank" rel="noopener noreferrer">Code signing policy</a> ·{" "}
            <a href="https://github.com/jorj1357/mimita-public/blob/main/LICENSE" target="_blank" rel="noopener noreferrer">License</a> */}
          </p>
        </div>
      </div>

      <FeedbackBox pageName="download" />
    </Layout>
  )
}
