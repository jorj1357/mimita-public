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
            <br></br>
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

          <p>
            Some analytics and third-party services may be used
            to understand if people are using the platform,
            or visiting the site,
            or finding things useful,
            or care at all.
          </p>

          <p>
            This may include:
            <br></br>
            Google Analytics,
            hosting providers,
            payment systems,
            newsletters,
            or other infrastructure services.
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