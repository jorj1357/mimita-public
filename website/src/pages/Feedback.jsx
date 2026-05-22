import "../App.css"

import Layout from "../components/Layout"

export default function Feedback() {
  return (
    <Layout>

      <div className="feedbackPage">

        <h1 className="feedbackTitle">
          MiMITA FEEDBACK
        </h1>

        <p className="feedbackText">
          suggestions,
          ideas,
          comments,
          concerns,
          weird mechanics,
          bugs,
          thoughts,
          experiments,
          anything!
        </p>

        <p className="feedbackText">
          you can write it directly here:
        </p>

        <textarea
          className="feedbackBox"
          placeholder="write feedback here..."
        />

        <button className="feedbackButton">
          SEND PLACEHOLDER
        </button>

        {/* <p className="feedbackSmall">
          placeholder system for now
        </p> */}

      </div>

    </Layout>
  )
}