import "../App.css"

import Layout from "../components/Layout"

export default function Donate() {
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
          SUPPORT MiMITA
          <br></br>
          <br></br>
        </h1>

        <p className="donateText">
          Supporting MiMITA directly 
          <br></br>
          helps move toward a world 
          <br></br>where
          creation is done 
          <br></br>
          more for the love of creating, experimenting,
          and building meaningful things.
        <br></br>
          <br></br>

        </p>

        {/* <p className="donateText">
          No popups.
          No pressure.
          No manipulation.
          Just optional support if you believe in the vision.
        </p> */}

        <a
        href="https://www.paypal.com/donate/?hosted_button_id=ANQ9KHSMCYMFC"
        target="_blank"
        rel="noopener noreferrer"
        className="donateButton"
        >
        SUPPORT MiMITA
        </a>

      </div>

    </Layout>
  )
}