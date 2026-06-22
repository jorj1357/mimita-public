import "../App.css"

import Layout from "../components/Layout"
import FeedbackBox from "../components/FeedbackBox"

export default function Contribute() {

  return (

    <Layout>

      <div className="donatePage">

        <video
          autoPlay
          loop
          muted
          playsInline
          preload="auto"
          className="donateVideo"
        >

          <source
            src="/thank-you-small.mp4"
            type="video/mp4"
          />

        </video>

        <h1 className="donateTitle">
          CONTRIBUTE TO MiMITA
        </h1>

        <p className="donateText">

          MiMITA is an experimental creative project
          built around fun, movement, games,
          art, music, systems, and creation.

          <br />
          <br />

          People can contribute through support,
          development, art, testing,
          ideas, community, and experimentation.

        </p>

        {/* =========================
            SUPPORT
        ========================= */}

        <div className="contributeSection">

          <a
            href="https://www.paypal.com/donate/?hosted_button_id=ANQ9KHSMCYMFC"
            target="_blank"
            rel="noopener noreferrer"
            className="donateButton"
          >
            SUPPORT MiMITA
          </a>

          <p className="donateText">
            Financial support for development,
            infrastructure, experiments, and creation.
          </p>

        </div>

        {/* =========================
            DEVELOP
        ========================= */}

        <div className="contributeSection">

          <a
            href="https://github.com/jorj1357/mimita-public"
            target="_blank"
            rel="noopener noreferrer"
            className="donateButton"
          >
            DEVELOP MiMITA
          </a>

          <p className="donateText">
            Programming, tools, gameplay systems,
            engine work, and experiments.
          </p>

        </div>

        {/* =========================
            COMMUNITY
        ========================= */}

        <div className="contributeSection">

          <a
            href="https://discord.gg/XvzV9hSz6j"
            target="_blank"
            rel="noopener noreferrer"
            className="donateButton"
          >
            JOIN COMMUNITY
          </a>

          <p className="donateText">
            Art, music, feedback, testing,
            discussion, and community.
          </p>

        </div>

      </div>

      <FeedbackBox pageName="contribute" />

    </Layout>

  )

}