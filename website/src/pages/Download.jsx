import "../App.css"

import Layout from "../components/Layout"

export default function Download() {
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
        >
          DOWNLOAD v1
          <br></br>
          (redirects to github)
        </a>

        <p className="downloadInfo">
          windows 64-bit
        </p>

      </div>

    </Layout>
  )
}