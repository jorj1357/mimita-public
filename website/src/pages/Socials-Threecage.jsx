import "../App.css"

import Layout from "../components/Layout"

export default function ThreeCage() {
  return (
    <Layout>

      <div className="socialsPage">

        <h1 className="socialsTitle">
          3cage
        </h1>

        {/* SOCIAL LINKS */}

        <div className="socialsLinks">

          <a
            href="https://www.youtube.com/@3cage"
            target="_blank"
            rel="noopener noreferrer"
            className="socialLink"
          >
            <img
              src="/youtube icon v1-optimized.webp"
              alt="youtube"
              className="socialIcon"
            />

            <span>
              3cage : youtube
            </span>
          </a>

          <a
            href="https://www.tiktok.com/@3cage"
            target="_blank"
            rel="noopener noreferrer"
            className="socialLink"
          >
            <img
              src="/tiktok icon v1-optimized.webp"
              alt="tiktok"
              className="socialIcon"
            />

            <span>
              3cage : tiktok
            </span>
          </a>

          <a
            href="https://www.instagram.com/prod3cage/"
            target="_blank"
            rel="noopener noreferrer"
            className="socialLink"
          >
            <img
              src="/instagram icon v1-optimized.webp"
              alt="instagram"
              className="socialIcon"
            />

            <span>
              3cage : instagram
            </span>
          </a>

        </div>

        {/* ROBLOX GAMES */}

        <div
          style={{
            marginTop: "80px"
          }}
        >

          <h2 className="socialsTitle">
            roblox projects
          </h2>

          <div className="socialsLinks">

            <a
              href="https://www.roblox.com/games/86943475641569"
              target="_blank"
              rel="noopener noreferrer"
              className="socialLink"
            >
              <img
                src="/ids pvp v2-compressed.jpg"
                alt="ids pvp"
                className="socialIcon"
              />

              <span>
                IDS: PvP — movement shooter, on roblox
              </span>
            </a>

            <a
              href="https://www.roblox.com/games/126120811396387"
              target="_blank"
              rel="noopener noreferrer"
              className="socialLink"
            >
              <img
                src="/ids logo v1-optimized.webp"
                alt="ids"
                className="socialIcon"
              />

              <span>
                infinite dungeon slayer
              </span>
            </a>

            <a
              href="https://www.roblox.com/games/131875330396600"
              target="_blank"
              rel="noopener noreferrer"
              className="socialLink"
            >
              <span>
                knife tag!
              </span>
            </a>

            <a
              href="https://mimita.fun/does-not-exist"
              target="_blank"
              rel="noopener noreferrer"
              className="socialLink"
            >
              <img
                src="/get teh burger thumb v1-optimized.webp"
                alt="get the burger"
                className="socialIcon"
              />

              <span>
                get the burger!!!
              </span>
            </a>

          </div>

        </div>

        {/* GROUPS */}

        <div
          style={{
            marginTop: "80px",
            marginBottom: "80px"
          }}
        >

          <h2 className="socialsTitle">
            groups
          </h2>

          <div className="socialsLinks">

            <a
              href="https://www.roblox.com/communities/412637603"
              target="_blank"
              rel="noopener noreferrer"
              className="socialLink"
            >
              "in slop we trust"
            </a>

          </div>

        </div>

        {/* ABOUT */}

        <h1 className="socialsTitle">
          3cage : about
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

          <br /><br />

          that people genuinely care about at a much larger scale than jorj1357,

          <br /><br />

          while improving at game/system design, content creation, and distribution

          <br /><br />

          as part of the larger MiMITA project.

        </p>

      </div>

    </Layout>
  )
}
