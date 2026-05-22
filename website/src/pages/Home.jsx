import "../App.css"

import Layout from "../components/Layout"

import { Link } from "react-router-dom"

export default function Home() {
  return (
    <Layout>

      <main className="hero">

        <img
          src="/mimita icon v1.png"
          alt="MiMITA"
          className="logo"
        />

        <h1 className="title">
          MiMITA: v1
        </h1>

      </main>

      <section className="aboutSection">

        <h2 className="aboutBig">
          movement is more important than aim
        </h2>

        <p className="aboutSmall">
          copyleft-enabled experimental game engine
        </p>

        <div className="linkGroup">

          <a
            href="https://github.com/jorj1357/mimita-public"
            target="_blank"
            rel="noopener noreferrer"
            className="bigLink"
          >
            DOWNLOAD MiMITA
          </a>

          <Link
            to="/socials"
            className="bigLink"
          >
            SOCIAL LINKS
          </Link>

          <Link
            to="/donate"
            className="bigLink"
          >
            SUPPORT DEVELOPMENT + GET COOL PERKS
          </Link>

          <Link
            to="/newsletter"
            className="bigLink"
          >
            EMAIL NEWSLETTER
          </Link>

        </div>

      </section>

    </Layout>
  )
}