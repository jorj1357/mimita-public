import { useState, useEffect, useRef, useCallback } from "react"

import "../App.css"

import Layout from "../components/Layout"
import FeedbackBox from "../components/FeedbackBox"

export default function Download() {
  const [version, setVersion] = useState(null)
  const [error, setError] = useState(null)
  const [downloading, setDownloading] = useState(false)
  const timerRef = useRef(null)
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

    timerRef.current = setTimeout(() => {
      doDownload()
    }, 5000)

    return () => {
      if (timerRef.current) clearTimeout(timerRef.current)
    }
  }, [doDownload])

  const fileSize = version ? (version.file_size_mb || "~200") : "~200"

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
          <source src="/mimita-preview-small.mp4" type="video/mp4" />
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
          windows 64-bit
        </p>

        {version && (
          <div className="downloadVersion">
            <span>Version: {version.version}</span>
            <span>Released: {version.release_date}</span>
            <span>Size: {fileSize} MB</span>
          </div>
        )}
      </div>

      <FeedbackBox pageName="download" />
    </Layout>
  )
}
