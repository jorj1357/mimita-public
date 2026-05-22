import "./App.css"
import { Link } from "react-router-dom"

export default function App() {
  return (
    <div className="app">

      <header className="header">

        <Link to="/">
          HOME
        </Link>

        <Link to="/download">
          MIMITA DOWNLOAD
        </Link>

        <Link to="/socials">
          SOCIAL LINKS
        </Link>

        <Link to="/about">
          ABOUT
        </Link>

        <Link to="/feedback">
          FEEDBACK
        </Link>

        <Link to="/donate">
          DONATE
        </Link>

      </header>

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

          <Link to="/download" className="bigLink">
            DOWNLOAD MiMITA
          </Link>

          <Link to="/socials" className="bigLink">
            SOCIAL LINKS
          </Link>

          <Link to="/donate" className="bigLink">
            SUPPORT DEVELOPMENT + GET COOL PERKS
          </Link>

          <Link to="/newsletter" className="bigLink">
            EMAIL NEWSLETTER
          </Link>

        </div>

      </section>

    </div>
  )
}