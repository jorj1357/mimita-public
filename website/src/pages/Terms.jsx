import "../App.css"

import Layout from "../components/Layout"

export default function Terms() {
  return (
    <Layout>

      <div className="termsPage">

        <h1 className="termsTitle">
          MiMITA TERMS / PRIVACY / LEGAL
        </h1>

        <div className="termsContent">

          <h2 style={{ color: "#a020ff", marginTop: "1.5rem" }}>TERMS OF USE</h2>

          <p>
            MiMITA is experimental software.
          </p>

          <p>
            Things may break,
            change,
            disappear,
            reset,
            or behave unexpectedly.
          </p>

          <p>
            Please do not:
          </p>

          <ul className="termsList">
            <li>harm other people</li>
            <li>upload illegal content</li>
            <li>abuse infrastructure or services</li>
            <li>intentionally disrupt the platform</li>
          </ul>

          <p>
            MiMITA is heavily inspired by:
            <br />
            modding culture,
            experimentation,
            remixing,
            open systems,
            and copyleft-style principles.
          </p>

          <p>
            The long-term goal is to encourage creation,
            learning,
            experimentation,
            and things made just for fun.
          </p>

          <h2 style={{ color: "#a020ff", marginTop: "2rem" }}>PRIVACY POLICY</h2>

          <p>
            Mimita may collect anonymous gameplay analytics
            after you answer the first-launch popup.
            This helps us understand session length,
            onboarding,
            maps,
            weapons,
            movement,
            feature use,
            crashes,
            disconnects,
            and retention.
          </p>

          <p>
            We do not sell analytics data.
            We do not collect passwords,
            chat contents,
            auth tokens,
            or sensitive free-form information
            through gameplay analytics.
          </p>

          <p>
            Analytics can be disabled permanently
            from the first-launch popup,
            the in-game settings menu,
            or the analytics terminal command.
            Data deletion can be requested from the game settings menu.
          </p>

          <p>
            The website may use infrastructure services
            such as hosting,
            email,
            newsletters,
            payments,
            Google Analytics,
            or Metricool.
          </p>

          <p>
            By using MiMITA,
            you understand that the project is experimental
            and continuously evolving.
          </p>

        </div>

      </div>

    </Layout>
  )
}
