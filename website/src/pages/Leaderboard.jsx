// 09 06 2026, 14 43
/* purpose
* Display persisted progression rankings alongside existing competitions.
* Preserve backend ordering and link players to public profiles.
* Reuse the table and API diagnostics for all leaderboard types.
* DOES NOT calculate rewards or reorder server ranks in the browser.
*/
import { useEffect, useState } from "react"
import { Link } from "react-router-dom"
import "../App.css"
import Layout from "../components/Layout"
import PixelBox from "../components/PixelBox"
import Username from "../components/Username"
import { apiRequest } from "../lib/api"
import { formatPersistentStat } from "../lib/persistentStats"

const progressionBoards = [
  ["xp", "Most XP", "total_xp", "XP"],
  ["gold", "Most Gold", "gold", "GOLD"],
  ["playtime", "Most Playtime", "playtime_ticks", "PLAYTIME"],
  ["kills", "Most Kills", "lifetime_player_kills", "KILLS"],
  ["deaths", "Most Deaths", "lifetime_deaths", "DEATHS"],
]

function formatScore(value, decimals) {
  if (!decimals) return formatPersistentStat(value)
  const n = Number(value)
  if (!Number.isFinite(n)) return "—"
  return decimals ? n.toFixed(decimals) : Math.round(n).toLocaleString()
}

function LeaderboardTable({ rows, highlightId, valueKey, valueLabel, decimals, emptyText, loading, error }) {
  if (loading) return <p style={{ color: "rgba(255,255,255,0.6)" }}>Loading leaderboard...</p>
  if (error) return <p style={{ color: "#ff6b6b" }}>Unable to load leaderboard.</p>
  if (!rows || rows.length === 0) return <p style={{ color: "rgba(255,255,255,0.6)" }}>{emptyText}</p>

  return (
    <div style={{ overflowX: "auto" }}>
    <div role="table" aria-label={valueLabel} style={{ display: "flex", flexDirection: "column", gap: "0.35rem", minWidth: "28rem" }}>
      <div role="row" style={{ display: "flex", gap: "0.5rem", color: "#a020ff", fontWeight: 800, borderBottom: "1px solid #333", paddingBottom: "0.35rem" }}>
        <span role="columnheader" style={{ width: "3rem" }}>RANK</span>
        <span role="columnheader" style={{ flex: 1 }}>PLAYER</span>
        <span role="columnheader" style={{ width: "13rem", textAlign: "right" }}>{valueLabel}</span>
      </div>
      {rows.map((entry, i) => (
        <div
          role="row"
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
          <span role="cell" style={{ width: "3rem", color: "rgba(255,255,255,0.65)", fontWeight: 800 }}>#{entry.rank || i + 1}</span>
          <span role="cell" style={{ flex: 1, minWidth: 0, overflowWrap: "anywhere" }}><Link to={`/users/id/${encodeURIComponent(entry.id)}`}><Username user={entry} size="sm" /></Link></span>
          <span role="cell" style={{ width: "13rem", textAlign: "right", color: "#00ffcc", overflowWrap: "anywhere" }}>{valueKey === "playtime_ticks" ? formatPersistentStat(entry[valueKey], true) : formatScore(entry[valueKey], decimals)}</span>
        </div>
      ))}
    </div>
    </div>
  )
}

export default function Leaderboard() {
  const [progression, setProgression] = useState({})
  const [mmrRows, setMmrRows] = useState(null)
  const [mmrLoading, setMmrLoading] = useState(true)
  const [mmrError, setMmrError] = useState(false)

  const [aimRows, setAimRows] = useState(null)
  const [aimLoading, setAimLoading] = useState(true)
  const [aimError, setAimError] = useState(false)

  useEffect(() => {
    let active = true
    apiRequest("/api/leaderboard?type=mmr&limit=50")
      .then(d => { if (active) { setMmrRows(d.leaderboard || []); setMmrLoading(false) } })
      .catch(() => { if (active) { setMmrError(true); setMmrLoading(false) } })

    apiRequest("/api/games/aim-test-v1/leaderboard?limit=50")
      .then(d => { if (active) { setAimRows(d.leaderboard || []); setAimLoading(false) } })
      .catch(() => { if (active) { setAimError(true); setAimLoading(false) } })

    return () => { active = false }
  }, [])

  useEffect(() => {
    const controller = new AbortController()
    let pending = false
    const refresh = async () => {
      if (pending) return
      pending = true
      await Promise.all(progressionBoards.map(async ([type]) => {
        try {
          const data = await apiRequest(`/api/leaderboard?type=${type}&limit=50`, { signal: controller.signal })
          if (!data?.success || !Array.isArray(data.leaderboard)) throw new Error("Invalid leaderboard response")
          if (!controller.signal.aborted) setProgression(previous => ({ ...previous, [type]: { rows: data.leaderboard, error: false } }))
        } catch {
          if (!controller.signal.aborted) setProgression(previous => ({ ...previous, [type]: { rows: previous[type]?.rows, error: true } }))
        }
      }))
      pending = false
    }
    refresh()
    const timer = setInterval(refresh, 60 * 1000)
    return () => { controller.abort(); clearInterval(timer) }
  }, [])

  return (
    <Layout>
      <div className="aboutPage">
        <div className="aboutContent">
          <h1 className="aboutTitle">LEADERBOARDS</h1>

          <p>Saved game totals. Highest first; equal totals are ordered by player ID. Refreshes every minute.</p>
          {progressionBoards.map(([type, title, valueKey, valueLabel]) => {
            const board = progression[type]
            return (
              <PixelBox key={type} style={{ marginBottom: "1.5rem" }}>
                <h3 style={{ color: "#a020ff", marginBottom: "0.5rem" }}>{title}</h3>
                {type === "kills" && <p>Player kills only. NPC kills are counted separately.</p>}
                {board?.error && board.rows && <p role="status">Unable to refresh. Showing the last loaded rankings.</p>}
                <LeaderboardTable rows={board?.rows} valueKey={valueKey} valueLabel={valueLabel}
                  emptyText="No saved totals yet." loading={!board} error={board?.error && !board.rows} />
              </PixelBox>
            )
          })}

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
