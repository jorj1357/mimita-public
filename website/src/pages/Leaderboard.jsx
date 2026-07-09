import "../App.css"
import Layout from "../components/Layout"
import PixelBox from "../components/PixelBox"

export default function Leaderboard() {
  return (
    <Layout>
      <div className="aboutPage">
        <div className="aboutContent">
          <h1 className="aboutTitle">LEADERBOARDS</h1>

          <PixelBox style={{ marginBottom: "1.5rem" }}>
            <h3 style={{ color: "#a020ff", marginBottom: "0.5rem" }}>Aim Trainer Best Times</h3>
            <p style={{ color: "rgba(255,255,255,0.6)" }}>
              The fastest aim trainer completion times are tracked globally. Play the aim test on the Games page to set your time.
            </p>
          </PixelBox>

          <PixelBox style={{ marginBottom: "1.5rem" }}>
            <h3 style={{ color: "#a020ff", marginBottom: "0.5rem" }}>Ranked MMR</h3>
            <p style={{ color: "rgba(255,255,255,0.6)" }}>
              The highest-ranked MiMITA players by Matchmaking Rating (MMR). Compete in duels to climb the ladder.
            </p>
          </PixelBox>

          <PixelBox style={{ marginBottom: "1.5rem" }}>
            <h3 style={{ color: "#a020ff", marginBottom: "0.5rem" }}>Mission Best Times</h3>
            <p style={{ color: "rgba(255,255,255,0.6)" }}>
              Best completion times on specific missions — coming soon. Check back after the next update.
            </p>
          </PixelBox>
        </div>
      </div>
    </Layout>
  )
}
