import "../App.css"

import Layout from "../components/Layout"

export default function ThreeCage() {
  return (
    <Layout>

      <div className="socialsPage">

        <h1 className="socialsTitle">
          3cage
        </h1>

      <p
        className="aboutSmall"
        style={{
          maxWidth: "700px",
          margin: "0 auto 40px auto",
          textAlign: "center",
          lineHeight: "1.8"
        }}
      >

        3cage is mainly an experiment in learning how to make interactive stuff,
        <br></br>
        <br></br>
        that people genuinely care about at a much larger scale than jorj1357,
        <br></br>
        <br></br>
        while improving at game/system design, content creation, and distribution 
        <br></br>
        <br></br>
        as part of the larger MiMITA project.

      </p>

        {/* SOCIAL LINKS */}

          <h1 className="socialsTitle">
          3cage socials
        </h1>

        <div className="socialsLinks">

          <a
            href="https://www.youtube.com/@3cage"
            target="_blank"
            rel="noopener noreferrer"
            className="socialLink"
          >
            3cage : youtube
          </a>

          <a
            href="https://www.tiktok.com/@3cage"
            target="_blank"
            rel="noopener noreferrer"
            className="socialLink"
          >
            3cage : tiktok
          </a>

          <a
            href="https://www.instagram.com/3cage/"
            target="_blank"
            rel="noopener noreferrer"
            className="socialLink"
          >
            3cage : instagram
          </a>
{/* 
          <a
            href="https://x.com/3cage"
            target="_blank"
            rel="noopener noreferrer"
            className="socialLink"
          >
            3CAGE : X / TWITTER
          </a> */}

        </div>

        {/* ROBLOX GAMES */}

        <div
          style={{
            marginTop: "80px"
          }}
        >

          <h2 className="socialsTitle">
            ROBLOX Projects
          </h2>

          <div className="socialsLinks">

            <a
              href="https://www.roblox.com/games/131875330396600"
              target="_blank"
              rel="noopener noreferrer"
              className="socialLink"
            >
              Knife Tag!
            </a>

            <a
            // put it as doesnt exist so it goes to the idk what it is page 
            // not found apge 5 25 2026 
              href="https://mimita.fun/does-not-exist"              
              target="_blank"
              rel="noopener noreferrer"
              className="socialLink"
            >
              Get The Burger!!!
            </a>

            <a
              href="https://www.roblox.com/games/126120811396387"
              target="_blank"
              rel="noopener noreferrer"
              className="socialLink"
            >
              INFINITE DUNGEON SLAYER!!!!!!
            </a>

          </div>

        </div>

        {/* ROBLOX GROUPS */}

        <div
          style={{
            marginTop: "80px",
            marginBottom: "80px"
          }}
        >

          <h2 className="socialsTitle">
            Groups
          </h2>

          <div className="socialsLinks">

            {/* <a
              href="https://www.roblox.com/communities/YOUR_GROUP_ID"
              target="_blank"
              rel="noopener noreferrer"
              className="socialLink"
            >
              3CAGE GROUP
            </a> */}

            <a
              href="https://www.roblox.com/communities/412637603"
              target="_blank"
              rel="noopener noreferrer"
              className="socialLink"
            >
              "In Slop We Trust"
            </a>

          </div>

        </div>

      </div>

    </Layout>
  )
}