import "../App.css"

import Layout from "../components/Layout"

import NewsletterBox from "../components/NewsletterBox"

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

        <NewsletterBox />

      </div>

    </Layout>
  )
}