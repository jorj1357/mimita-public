import "../App.css"

import Layout from "../components/Layout"

import { Link } from "react-router-dom"

import NewsletterBox from "../components/NewsletterBox"

export default function Home() {
  return (
    <Layout>

      <main className="hero">

        <video
        autoPlay
        loop
        muted
        playsInline
        preload="auto"
        className="logoVideo"
        >

        <source
            src="/mimita-preview-small.mp4"
            type="video/mp4"
        />

        </video>

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

            <Link
                to="/download"
                className="bigLink"
            >
                download
            </Link>

            <Link
                to="/about"
                className="bigLink"
            >
                about
            </Link>

            </div>

      </section>

      <NewsletterBox />

    </Layout>
  )
}