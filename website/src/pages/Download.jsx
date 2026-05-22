import "../App.css"

import Layout from "../components/Layout"

export default function Download() {
  return (
    <Layout>

      <div className="downloadPage">

        <img
          src="https://placehold.co/1000x600/000000/FFFFFF?text=MiMITA+Gameplay+GIF"
          alt="MiMITA Gameplay"
          className="downloadGif"
        />

        <a
          href="https://github.com/jorj1357/mimita-public"
          target="_blank"
          rel="noopener noreferrer"
          className="downloadButton"
        >
          DOWNLOAD v1
        </a>

        <p className="downloadInfo">
          windows 64-bit
        </p>

      </div>

    </Layout>
  )
}