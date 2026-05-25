import "../App.css"

import Layout from "../components/Layout"

import { Link } from "react-router-dom"

export default function NotFound() {
  return (
    <Layout>

      <div
        className="socialsPage"
        style={{
          minHeight: "70vh",
          display: "flex",
          flexDirection: "column",
          justifyContent: "center",
          alignItems: "center",
          textAlign: "center",
          gap: "20px"
        }}
      >

        <h1 className="socialsTitle">
          sorry, page not found
        </h1>

        <p
          className="aboutSmall"
          style={{
            maxWidth: "500px"
          }}
        >
          this page probably does not exist yet,
          moved somewhere else,
          or exploded.

          <br></br>
          <br></br>

          if ur here from "GET THE BURGER",
          <br></br>
          as of 5 25 2026 
          <br></br>
          it's not done yet 
          <br></br>
          <br></br>
          stay tuned Vro 💝
        </p>

        <Link
          to="/"
          className="bigLink"
        >
          go home
        </Link>

      </div>

    </Layout>
  )
}