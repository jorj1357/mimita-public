import "../App.css"

import Layout from "../components/Layout"

import { Link } from "react-router-dom"

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
          className="bgVideo"
        >
          <source
            src="/untitled-loop-small.mp4"
            type="video/mp4"
          />
        </video>

        <div className="heroOverlay" />

        <div className="heroContent">

          <h1 className="mainLogo">
            MiMITA
          </h1>

          <p className="tagline">
            = "Movement is More Important Than Aim"
          </p>

          <div className="homeCards">

            <Link
              to="https://www.roblox.com/games/86943475641569"
              className="homeCard"
            >
              <img
                src="/ids pvp v2-compressed.jpg"
                alt="ids pvp"
                className="cardLogo"
              />

              <div className="cardText">
                IDS: PvP
              </div>

              <div className="cardSubtext">
                movement shooter, on roblox, 
                
                <br></br>
                <br></br> 
                good example of MiMITA PvP i want to make
              </div>
            </Link>

            <Link
              to="https://www.roblox.com/games/126120811396387"
              className="homeCard"
            >
              <img
                src="/ids logo v1-optimized.webp"
                alt="ids"
                className="cardLogo"
              />

              <div className="cardText">
                infinite dungeon slayer
              </div>

              <div className="cardSubtext">
                slay lots of monsters • roblox game
              </div>
            </Link>

            <Link
              to="/3cage"
              className="homeCard"
            >
              <img
                src="/3cage pfp v2-optimized.webp"
                alt="3cage"
                className="cardLogo"
              />

              <div className="cardText">
                3cage
              </div>

              <div className="cardSubtext">
                project for the roblox games i make
              </div>
            </Link>

            <Link
              to="/jorj"
              className="homeCard"
            >
              <img
                src="/fake raptv post jorj rav maybe.png JORJ LOGO v8-optimized.webp"
                alt="jorj1357"
                className="cardLogo"
              />

              <div className="cardText">
                jorj1357
              </div>

              <div className="cardSubtext">
                music project / original internet thing
              </div>
            </Link>

          </div>

        </div>

      </main>

    </Layout>
  )
}
