import "../App.css"

import Layout from "../components/Layout"
import FeedbackBox from "../components/FeedbackBox"

const API = import.meta.env.VITE_API_ORIGIN || ""

export default function Download() {
  function handleDownload() {
    try {
      fetch(`${API}/api/track/download`, {
        method: "POST",
        credentials: "include",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify({ source: "website", platform: "windows" })
      }).catch(() => {})
    }
    catch {}
  }

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

          <source
            src="/mimita-preview-small.mp4"
            type="video/mp4"
          />

        </video>

        <a
          href="https://github.com/jorj1357/mimita-public"
          target="_blank"
          rel="noopener noreferrer"
          className="downloadButton"
          onClick={handleDownload}
        >
          DOWNLOAD v1
          <br></br>
          (redirects to github)
        </a>

        <p className="downloadInfo">
          windows 64-bit
        </p>

      </div>

      <FeedbackBox pageName="download" />

    </Layout>
  )
}