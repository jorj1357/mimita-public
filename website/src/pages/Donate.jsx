import "../App.css"

import Layout from "../components/Layout"

export default function Donate() {
  return (
    <Layout>

      <div className="donatePage">

        <h1 className="donateTitle">
          SUPPORT MiMITA
        </h1>

        <p className="donateText">
          Supporting MiMITA directly helps move toward a world where
          creation is done more for the love of creating,
          experimenting,
          and building meaningful things.
        </p>

        <p className="donateText">
          No popups.
          No pressure.
          No manipulation.
          Just optional support if you believe in the vision.
        </p>

        <a
          href="/"
          className="donateButton"
        >
          DONATE PLACEHOLDER
        </a>

        {/* <p className="donateSmall">
          eventually:
          <br></br>
          ko-fi / github sponsors / stripe / crypto / mimita-native support
        </p> */}

      </div>

    </Layout>
  )
}