import { useState, useEffect, useRef, useCallback } from "react"
import { Link } from "react-router-dom"
import Layout from "../components/Layout"

const TOTAL_TARGETS = 30
const TARGET_RADIUS = 50
const PLAY_AREA_PADDING = 12
const BEST_KEY = "aimtestv1_best"
const RESULTS_KEY = "aimtestv1_results"
const LEADERBOARD_KEY = "aimtestv1_leaderboard"
const LEADERBOARD_CACHE_MS = 60000

function loadLocalBest() {
  try { const raw = localStorage.getItem(BEST_KEY); return raw ? parseFloat(raw) : null }
  catch { return null }
}
function loadLocalResults() {
  try { const raw = localStorage.getItem(RESULTS_KEY); return raw ? JSON.parse(raw) : [] }
  catch { return [] }
}
function saveLocalResult(time) {
  const results = loadLocalResults()
  results.push({ time, date: Date.now() })
  results.sort((a, b) => a.time - b.time)
  if (results.length > 10) results.length = 10
  localStorage.setItem(RESULTS_KEY, JSON.stringify(results))
  const best = results[0].time
  localStorage.setItem(BEST_KEY, String(best))
  return { results, best }
}
function loadCachedLeaderboard() {
  try {
    const raw = localStorage.getItem(LEADERBOARD_KEY)
    if (!raw) return null
    const data = JSON.parse(raw)
    if (Date.now() - data.ts > LEADERBOARD_CACHE_MS) return null
    return data.entries
  } catch { return null }
}
function cacheLeaderboard(entries) {
  try { localStorage.setItem(LEADERBOARD_KEY, JSON.stringify({ ts: Date.now(), entries })) }
  catch {}
}

function randomInRect(playW, playH, radius) {
  const minX = radius + PLAY_AREA_PADDING
  const maxX = playW - radius - PLAY_AREA_PADDING
  const minY = radius + PLAY_AREA_PADDING
  const maxY = playH - radius - PLAY_AREA_PADDING
  if (maxX <= minX || maxY <= minY) return { x: playW / 2, y: playH / 2 }
  return {
    x: minX + Math.random() * (maxX - minX),
    y: minY + Math.random() * (maxY - minY),
  }
}

export default function AimTestV1() {
  const [phase, setPhase] = useState("start")
  const [time, setTime] = useState(0)
  const [targetsHit, setTargetsHit] = useState(0)
  const [targetPos, setTargetPos] = useState({ x: 0, y: 0 })
  const [localBest, setLocalBest] = useState(() => loadLocalBest())
  const [localResults, setLocalResults] = useState(() => loadLocalResults())
  const [serverBest, setServerBest] = useState(null)
  const [serverResults, setServerResults] = useState([])
  const [leaderboard, setLeaderboard] = useState(() => loadCachedLeaderboard())
  const [leaderboardLoading, setLeaderboardLoading] = useState(false)
  const [leaderboardError, setLeaderboardError] = useState(false)
  const [submissionStatus, setSubmissionStatus] = useState(null)
  const [user, setUser] = useState(null)
  const [missTexts, setMissTexts] = useState([])
  const playRef = useRef(null)
  const timerRef = useRef(null)
  const startTimeRef = useRef(0)
  const targetsHitRef = useRef(0)
  const lastSpawnRef = useRef(0)
  const missIdRef = useRef(0)
  const hitAudioRef = useRef(null)
  const missAudioRef = useRef(null)

  function getHitAudio() {
    if (!hitAudioRef.current) hitAudioRef.current = new Audio("/assets/audio/target-hit.wav")
    return hitAudioRef.current
  }
  function getMissAudio() {
    if (!missAudioRef.current) missAudioRef.current = new Audio("/assets/audio/miss-click.wav")
    return missAudioRef.current
  }

  useEffect(() => {
    setLocalBest(loadLocalBest())
    setLocalResults(loadLocalResults())

    fetch("/api/auth/me", { credentials: "include" })
      .then(r => r.json())
      .then(data => {
        if (data.success) {
          setUser(data.user)
          fetch("/api/games/aim-test-v1/scores", { credentials: "include" })
            .then(r => r.json())
            .then(d => {
              if (d.success) {
                if (d.best != null) setServerBest(d.best)
                if (d.recent) setServerResults(d.recent)
              }
            })
            .catch(() => {})
        }
      })
      .catch(() => {})

    const cached = loadCachedLeaderboard()
    if (cached) setLeaderboard(cached)
    fetchLeaderboard()
  }, [])

  function fetchLeaderboard() {
    const cached = loadCachedLeaderboard()
    if (cached) return
    setLeaderboardLoading(true)
    setLeaderboardError(false)
    fetch("/api/games/aim-test-v1/leaderboard")
      .then(r => r.json())
      .then(d => {
        if (d.success && d.leaderboard) {
          setLeaderboard(d.leaderboard)
          cacheLeaderboard(d.leaderboard)
          console.log("[AimTrainer] Leaderboard loaded. Entries: " + d.leaderboard.length)
        }
        setLeaderboardLoading(false)
      })
      .catch(() => {
        console.log("[AimTrainer] Leaderboard request failed.")
        setLeaderboardLoading(false)
        setLeaderboardError(true)
      })
  }

  const spawnTarget = useCallback(() => {
    const el = playRef.current
    if (!el) return
    const rect = el.getBoundingClientRect()
    const pos = randomInRect(rect.width, rect.height, TARGET_RADIUS)
    setTargetPos(pos)
    lastSpawnRef.current = performance.now()
  }, [])

  function startGame() {
    console.log("[AimTrainer] Game started")
    setPhase("playing")
    setTargetsHit(0)
    setTime(0)
    setMissTexts([])
    targetsHitRef.current = 0
    startTimeRef.current = performance.now()
    lastSpawnRef.current = performance.now()
    const tick = () => {
      setTime((performance.now() - startTimeRef.current) / 1000)
      timerRef.current = requestAnimationFrame(tick)
    }
    timerRef.current = requestAnimationFrame(tick)
    spawnTarget()
  }

  function handlePlayAreaClick(e) {
    if (phase !== "playing") return
    const el = playRef.current
    if (!el) return
    const rect = el.getBoundingClientRect()
    const clickX = e.clientX - rect.left
    const clickY = e.clientY - rect.top
    const dx = clickX - targetPos.x
    const dy = clickY - targetPos.y
    const dist = Math.sqrt(dx * dx + dy * dy)

    if (dist <= TARGET_RADIUS) {
      const reaction = performance.now() - lastSpawnRef.current
      console.log("[AimTrainer] Hit  Reaction: " + Math.round(reaction) + "ms")
      const hitAudio = getHitAudio()
      hitAudio.currentTime = 0
      hitAudio.play().catch(() => {})
      const hit = targetsHitRef.current + 1
      targetsHitRef.current = hit
      setTargetsHit(hit)
      if (hit >= TOTAL_TARGETS) {
        const elapsed = (performance.now() - startTimeRef.current) / 1000
        setTime(elapsed)
        cancelAnimationFrame(timerRef.current)
        finishGame(elapsed)
      } else {
        spawnTarget()
      }
    } else {
      console.log("[AimTrainer] Miss click")
      const missAudio = getMissAudio()
      missAudio.currentTime = 0
      missAudio.play().catch(() => {})
      const id = ++missIdRef.current
      setMissTexts(prev => [...prev, { id, x: clickX, y: clickY }])
      setTimeout(() => {
        setMissTexts(prev => prev.filter(m => m.id !== id))
      }, 1000)
    }
  }

  function finishGame(elapsed) {
    const { results, best } = saveLocalResult(elapsed)
    setLocalResults(results)
    setLocalBest(best)
    setPhase("results")
    setSubmissionStatus("submitting")
    console.log("[AimTrainer] Submitting score...")

    const doSubmit = user
      ? fetch("/api/games/aim-test-v1/scores", {
          method: "POST", credentials: "include",
          headers: { "Content-Type": "application/json" },
          body: JSON.stringify({ time: elapsed }),
        }).then(r => {
          if (!r.ok) throw new Error("HTTP " + r.status)
          return r.json()
        }).then(d => {
          if (!d.success) throw new Error("rejected")
          console.log("[AimTrainer] Score submitted.")
          setSubmissionStatus("success")
        }).catch(err => {
          console.log("[AimTrainer] Score submission failed: " + err.message)
          setSubmissionStatus("error")
        })
      : Promise.resolve().then(() => {
          setSubmissionStatus("success")
        })

    doSubmit.then(() => {
      console.log("[AimTrainer] Fetching leaderboard...")
      setLeaderboardLoading(true)
      setLeaderboardError(false)
      return fetch("/api/games/aim-test-v1/leaderboard")
    })
    .then(r => r.json())
    .then(d => {
      if (d.success && d.leaderboard) {
        setLeaderboard(d.leaderboard)
        cacheLeaderboard(d.leaderboard)
        console.log("[AimTrainer] Leaderboard loaded. Entries: " + d.leaderboard.length)
      }
      setLeaderboardLoading(false)
    })
    .catch(() => {
      console.log("[AimTrainer] Leaderboard request failed.")
      setLeaderboardLoading(false)
      setLeaderboardError(true)
    })
  }

  function retry() {
    cancelAnimationFrame(timerRef.current)
    setPhase("start")
    setTime(0)
    setTargetsHit(0)
  }

  useEffect(() => {
    return () => cancelAnimationFrame(timerRef.current)
  }, [])

  if (phase === "start") {
    return (
      <Layout>
        <div className="gamePage">
          <div className="gameInner">
            <div className="gameStartScreen">
              <h1 className="gameTitle">Aim Test v1</h1>
              <p className="gameSubtitle">How good is your aim?</p>
              <button className="gameStartBtn" onClick={startGame}>START</button>
            </div>
          </div>
        </div>
      </Layout>
    )
  }

  if (phase === "playing") {
    return (
      <Layout>
        <div className="gamePage">
          <div className="gameInner">
            <div className="gameHud">
              <div className="gameTimer">{time.toFixed(2)}</div>
              <div className="gameTargetCount">{targetsHit}/{TOTAL_TARGETS}</div>
            </div>
            <div className="gamePlayArea" ref={playRef} onClick={handlePlayAreaClick}>
              {targetsHit < TOTAL_TARGETS && (
                <div
                  className="gameTarget"
                  style={{
                    left: targetPos.x,
                    top: targetPos.y,
                  }}
                />
              )}
              {missTexts.map(m => (
                <div key={m.id} className="gameMissText" style={{ left: m.x, top: m.y }}>-1</div>
              ))}
            </div>
          </div>
        </div>
      </Layout>
    )
  }

  const displayResults = serverResults.length > 0 ? serverResults : localResults
  const displayBest = serverBest != null ? serverBest : localBest

  return (
    <Layout>
      <div className="gamePage">
        <div className="gameInner">
          <div className="gameResultsScreen">
            <h1 className="gameTitle">Final Time</h1>
            <p className="gameFinalTime">{time.toFixed(2)}s</p>

            <div className="gameLeaderboard">
              <div className="gameLeaderboardCol">
                <h3>Your Best</h3>
                <div className="gameLeaderboardList">
                  {displayBest != null ? (
                    <div className="gameLeaderboardRow gameLeaderboardRowBest">
                      <span className="gameLbRank">#1</span>
                      <span className="gameLbTime">{displayBest.toFixed(2)}s</span>
                    </div>
                  ) : <p className="gameLbEmpty">No scores yet.</p>}
                </div>
                <h3 style={{marginTop:'0.75rem'}}>Recent</h3>
                <div className="gameLeaderboardList">
                  {displayResults.length > 0 ? displayResults.slice(0, 5).map((r, i) => (
                    <div key={i} className="gameLeaderboardRow">
                      <span className="gameLbRank">#{i + 1}</span>
                      <span className="gameLbTime">{(typeof r === 'object' ? r.time : r).toFixed(2)}s</span>
                    </div>
                  )) : <p className="gameLbEmpty">No scores yet.</p>}
                </div>
              </div>

              <div className="gameLeaderboardCol">
                <h3>Global Top</h3>
                <div className="gameLeaderboardList">
                  {leaderboard && leaderboard.length > 0 ? leaderboard.slice(0, 10).map((entry, i) => (
                    <div key={i} className={"gameLeaderboardRow" + (user && entry.id === user.id ? " gameLeaderboardRowHighlight" : "")}>
                      <span className="gameLbRank">#{entry.rank || (i + 1)}</span>
                      <span className="gameLbName">{entry.username || "?"}</span>
                      <span className="gameLbTime">{Number(entry.score_value).toFixed(2)}s</span>
                    </div>
                  )) : (
                    <p className="gameLbEmpty">
                      {leaderboardLoading ? "Loading leaderboard..." : leaderboardError ? "Unable to load leaderboard." : "No global scores yet."}
                    </p>
                  )}
                </div>
                {submissionStatus === "error" && (
                  <p className="gameSubmissionError">Unable to submit score.</p>
                )}
              </div>
            </div>

            <div className="gameResultsButtons">
              <button className="gameStartBtn" onClick={retry}>Retry</button>
              <Link to="/games" className="gameStartBtn gameExitBtn">Exit</Link>
            </div>
          </div>
        </div>
      </div>
    </Layout>
  )
}
