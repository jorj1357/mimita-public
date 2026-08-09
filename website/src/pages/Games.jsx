import { useRef } from "react"
import { Link } from "react-router-dom"
import Layout from "../components/Layout"
import StickerLayer from "../components/StickerLayer"
import RainbowText from "../components/RainbowText"
import ChaosBox from "../components/ChaosBox"

function seededRandom(seed) {
  let s = seed
  return function () {
    s = (s * 16807) % 2147483647
    return (s - 1) / 2147483646
  }
}

const BORDERS = [1, 1, 1, 2, 2, 3, 3, 4]
const ACCENTS = [
  "#00ff41",
  "#ff0044",
  "#aa3bff",
  "#00ffff",
  "#ffff00",
  "#ff6600",
  "#ff00ff",
  "#66ff00",
]
const BADGES = ["★", "✦", "✧", "⚡", "🔥", "💀", "🎮", "👾"]

function GameCardDecorator({ children, gameId }) {
  const rand = useRef(seededRandom(gameId ? gameId.charCodeAt(0) * 999 + gameId.length : Date.now()))
  const r = rand.current

  const borderW = BORDERS[Math.floor(r() * BORDERS.length)]
  const accent = ACCENTS[Math.floor(r() * ACCENTS.length)]
  const rot = (r() - 0.5) * 2
  const badge = BADGES[Math.floor(r() * BADGES.length)]
  const badgeX = 5 + r() * 15
  const badgeY = 5 + r() * 10

  return (
    <div
      style={{
        position: "relative",
        display: "block",
        background: "rgba(255,255,255,0.03)",
        border: `${borderW}px solid ${accent}`,
        overflow: "hidden",
        textDecoration: "none",
        color: "inherit",
        transform: `rotate(${rot}deg)`,
        transition: "transform 0.2s ease, border-color 0.2s ease, box-shadow 0.2s ease",
        margin: "4px 0",
      }}
      onMouseEnter={e => { e.currentTarget.style.borderColor = accent; e.currentTarget.style.transform = `rotate(${rot}deg) scale(1.02)` }}
      onMouseLeave={e => { e.currentTarget.style.borderColor = accent; e.currentTarget.style.transform = `rotate(${rot}deg)` }}
    >
      <span
        style={{
          position: "absolute",
          top: `${badgeY}%`,
          right: `${badgeX}%`,
          fontSize: "14px",
          opacity: 0.4,
          pointerEvents: "none",
          userSelect: "none",
        }}
      >
        {badge}
      </span>
      <span className="pixelCorner pixelCornerTL" style={{ color: accent, opacity: 0.4 }} />
      <span className="pixelCorner pixelCornerTR" style={{ color: accent, opacity: 0.4 }} />
      <span className="pixelCorner pixelCornerBL" style={{ color: accent, opacity: 0.4 }} />
      <span className="pixelCorner pixelCornerBR" style={{ color: accent, opacity: 0.4 }} />
      {children}
    </div>
  )
}

const GAMES = [
    {
        id: "aim-test-v1",
        title: "aim-test-v1",
        subtitle: "How good is your aim...../?????? MOBILE AND PC WORKS",
        path: "/games/aim-test-v1",
    },
    {
        id: "dream-toy",
        title: "Dream Toy",
        subtitle: "A colorful dream playground. Draw, explode, throw, orbit, and explore.",
        path: "/games/dream-toy",
    },
    {
        id: "rhythm-test-v1",
        title: "Rhythm Test v1",
        subtitle: "Click in time with the beat. Starts at 120 BPM, gets faster!",
        path: "/games/rhythm-test-v1",
    },
    {
        id: "chili-race",
        title: "Chili Race",
        subtitle: "Try and eat 50 chilis before the CPU does!!!",
        path: "/games/chili-race",
    },
]

export default function Games() {
    return (
        <Layout>
            <div className="gamesPage" style={{ position: "relative" }}>
                <StickerLayer count={6} />

                <div className="gamesInner" style={{ position: "relative", zIndex: 1 }}>
                    <ChaosBox seed="GamesTitle">
                        <RainbowText as="h1" className="gamesTitle">
                            Games
                        </RainbowText>
                    </ChaosBox>
                    <ChaosBox seed="GamesSubtitle">
                        <p className="gamesSubtitle">Freaky slenderman: Collect my pagesssssahhhhh👅👅👅
                            Freaky  mimita: Play my gamesssssahhhhhhh 👅👅👅👅👅</p>
                    </ChaosBox>

                    <div className="gamesGrid">
                        {GAMES.map((game) => (
                            <Link key={game.id} to={game.path} style={{ textDecoration: "none", color: "inherit", display: "block" }}>
                                <GameCardDecorator gameId={game.id}>
                                    <div className="gameCardThumb">
                                        <div className="gameCardThumbPlaceholder">
                                            <span className="gameCardThumbIcon">{game.id === "dream-toy" ? "✦" : game.id === "rhythm-test-v1" ? "🎵" : game.id === "chili-race" ? "🌶️" : "🎯"}</span>
                                        </div>
                                    </div>
                                    <div className="gameCardBody">
                                        <h2 className="gameCardTitle">{game.title}</h2>
                                        <p className="gameCardSubtitle">{game.subtitle}</p>
                                    </div>
                                </GameCardDecorator>
                            </Link>
                        ))}
                    </div>
                </div>
            </div>
        </Layout>
    )
}
