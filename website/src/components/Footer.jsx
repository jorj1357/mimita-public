import { useLocation } from "react-router-dom"
import FeedbackBox from "./FeedbackBox"

export default function Footer() {
  const location = useLocation()

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
        <br></br>
        last big update: 7 9 2026
        i added a red bouncy ball + aim trainer game + more stuff to mimita.exe hehehe
        <br></br>
        hello@mimita.fun contact me/us!!! 
      </p>

    </footer>
  )
}
