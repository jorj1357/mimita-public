import { useEffect, useState } from "react"
import "../App.css"
import Layout from "../components/Layout"
import PixelBox from "../components/PixelBox"
import Username from "../components/Username"

function formatScore(value, decimals) {
  const n = Number(value)
  if (!Number.isFinite(n)) return "—"
  return decimals ? n.toFixed(decimals) : Math.round(n).toLocaleString()
}

function LeaderboardTable({ rows, highlightId, valueKey, valueLabel, decimals, emptyText, loading, error }) {
  if (loading) return <p style={{ color: "rgba(255,255,255,0.6)" }}>Loading leaderboard...</p>
  if (error) return <p style={{ color: "#ff6b6b" }}>Unable to load leaderboard.</p>
  if (!rows || rows.length === 0) return <p style={{ color: "rgba(255,255,255,0.6)" }}>{emptyText}</p>

  return (
    <div style={{ display: "flex", flexDirection: "column", gap: "0.35rem" }}>
      <div style={{ display: "flex", gap: "0.5rem", color: "#a020ff", fontWeight: 800, borderBottom: "1px solid #333", paddingBottom: "0.35rem" }}>
        <span style={{ width: "3rem" }}>RANK</span>
        <span style={{ flex: 1 }}>PLAYER</span>
        <span style={{ width: "6rem", textAlign: "right" }}>{valueLabel}</span>
      </div>
      {rows.map((entry, i) => (
        <div
          key={entry.id ?? i}
          style={{
            display: "flex",
            gap: "0.5rem",
            alignItems: "center",
            background: highlightId && entry.id === highlightId ? "#1a0033" : "#0a0a0a",
            outline: highlightId && entry.id === highlightId ? "1px solid #a020ff" : "1px solid #222",
            padding: "0.35rem 0.5rem",
          }}
        >
          <span style={{ width: "3rem", color: "rgba(255,255,255,0.65)", fontWeight: 800 }}>#{entry.rank || i + 1}</span>
          <span style={{ flex: 1 }}><Username user={entry} size="sm" /></span>
          <span style={{ width: "6rem", textAlign: "right", color: "#00ffcc" }}>{formatScore(entry[valueKey], decimals)}</span>
        </div>
      ))}
    </div>
  )
}

export default function Leaderboard() {
  const [mmrRows, setMmrRows] = useState(null)
  const [mmrLoading, setMmrLoading] = useState(true)
  const [mmrError, setMmrError] = useState(false)

  const [aimRows, setAimRows] = useState(null)
  const [aimLoading, setAimLoading] = useState(true)
  const [aimError, setAimError] = useState(false)

  useEffect(() => {
    let active = true
    fetch("/api/leaderboard?type=mmr&limit=50")
      .then(r => r.json())
      .then(d => { if (active) { setMmrRows(d.leaderboard || []); setMmrLoading(false) } })
      .catch(() => { if (active) { setMmrError(true); setMmrLoading(false) } })

    fetch("/api/games/aim-test-v1/leaderboard?limit=50")
      .then(r => r.json())
      .then(d => { if (active) { setAimRows(d.leaderboard || []); setAimLoading(false) } })
      .catch(() => { if (active) { setAimError(true); setAimLoading(false) } })

    return () => { active = false }
  }, [])

  return (
    <Layout>
      <div className="aboutPage">
        <div className="aboutContent">
          <h1 className="aboutTitle">LEADERBOARDS</h1>

          <PixelBox style={{ marginBottom: "1.5rem" }}>
            <h3 style={{ color: "#a020ff", marginBottom: "0.5rem" }}>Ranked MMR</h3>
            <p style={{ color: "rgba(255,255,255,0.6)", marginBottom: "0.75rem" }}>
              The highest-ranked MiMITA players by Matchmaking Rating (MMR). Compete in duels to climb the ladder.
            </p>
            <LeaderboardTable
              rows={mmrRows}
              valueKey="current_mmr"
              valueLabel="MMR"
              emptyText="No ranked players yet."
              loading={mmrLoading}
              error={mmrError}
            />
          </PixelBox>

          <PixelBox style={{ marginBottom: "1.5rem" }}>
            <h3 style={{ color: "#a020ff", marginBottom: "0.5rem" }}>Aim Trainer Best Times</h3>
            <p style={{ color: "rgba(255,255,255,0.6)", marginBottom: "0.75rem" }}>
              The fastest aim trainer completion times are tracked globally. Play the aim test on the Games page to set your time.
            </p>
            <LeaderboardTable
              rows={aimRows}
              valueKey="score_value"
              valueLabel="TIME"
              decimals={2}
              emptyText="No times yet."
              loading={aimLoading}
              error={aimError}
            />
          </PixelBox>

          <PixelBox>
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
