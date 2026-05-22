import "../App.css"

import Layout from "../components/Layout"

export default function Newsletter() {
  return (
    <Layout>

      <div className="newsletterPage">

        <h1 className="newsletterTitle">
          MiMITA NEWSLETTER
        </h1>

        <p className="newsletterText">
          occasional updates about development,
          experiments,
          gameplay,
          engine progress,
          and the future of MiMITA
        </p>

        <div className="newsletterForm">

          <input
            type="email"
            placeholder="enter email"
            className="newsletterInput"
          />

          <button className="newsletterButton">
            SIGN UP
          </button>

        </div>

        {/* <p className="newsletterSmall">
          placeholder
          <br></br>
          future emails sent from:
          <br></br>
          hello@mimita.fun
        </p> */}

      </div>

    </Layout>
  )
}