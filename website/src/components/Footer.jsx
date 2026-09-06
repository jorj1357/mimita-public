import { useLocation } from "react-router-dom"
import FeedbackBox from "./FeedbackBox"

export default function Footer() {
  const location = useLocation()
  const buildTime = typeof __BUILD_UTC__ !== "undefined" ? __BUILD_UTC__ : ""

  return (
    <footer className="footer">
      <div className="footerFeedback">
        <FeedbackBox pageName={location.pathname} />
      </div>

      <p>
        MiMITA Copyleft v5
        <br></br>
        :D
        <br></br>
        thanks  for visiting i am very grateful 
        {buildTime && (
          <>
            <br></br>
            <span style={{ opacity: 0.5, fontSize: "0.85em" }}>
              built: {buildTime}
            </span>
          </>
        )}
        <br></br>
        hello@mimita.fun contact me/us!!! 
      </p>

    </footer>
  )
}
