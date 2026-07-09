import "../App.css"

import Layout from "../components/Layout"
import FeedbackBox from "../components/FeedbackBox"

export default function Feedback() {
  return (
    <Layout>
      <div className="feedbackPage">
        <h1 className="feedbackTitle">FEEDBACK</h1>
        <p className="feedbackText">
          7 9 2026 I ACTUALLY READ THESE LIKE EVRY DAY <br></br>
          like whenever i open  the admin page these are at the bottom<br></br>
          i scroll and read every single one bro
        </p>
        <FeedbackBox pageName="feedback" />
      </div>
    </Layout>
  )
}
