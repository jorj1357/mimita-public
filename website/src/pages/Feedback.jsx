import "../App.css"

import Layout from "../components/Layout"
import FeedbackBox from "../components/FeedbackBox"

export default function Feedback() {
  return (
    <Layout>
      <div className="feedbackPage">
        <h1 className="feedbackTitle">FEEDBACK</h1>
        <p className="feedbackText">
          Suggestions, ideas, comments, concerns, bugs, thoughts — anything.
        </p>
        <FeedbackBox pageName="feedback" />
      </div>
    </Layout>
  )
}
