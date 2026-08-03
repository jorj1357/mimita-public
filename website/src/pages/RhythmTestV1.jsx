import { useState, useEffect, useRef, useCallback } from "react"
import { Link } from "react-router-dom"
import Layout from "../components/Layout"
import Username from "../components/Username"

const STARTING_BPM = 120
const BPM_INCREASE = 10
const BPM_INTERVAL_MS = 15000
const MAX_LIVES = 10
const TOLERANCE_MS = 300
const PERFECT_SCORE = 100
const MIN_SCORE = 1
const CIRCLE_RADIUS = 50
const PLAY_AREA_PADDING = 12
const BEST_KEY = "rhythmv1_best"
const RESULTS_KEY = "rhythmv1_results"

function loadLocalBest() {
  try { const raw = localStorage.getItem(BEST_KEY); return raw ? parseInt(raw) : null }
  catch { return null }
}
function loadLocalResults() {
  try { const raw = localStorage.getItem(RESULTS_KEY); return raw ? JSON.parse(raw) : [] }
  catch { return [] }
}
function saveLocalResult(score) {
  const results = loadLocalResults()
  results.push({ score, date: Date.now() })
  results.sort((a, b) => b.score - a.score)
  if (results.length > 10) results.length = 10
  localStorage.setItem(RESULTS_KEY, JSON.stringify(results))
  const best = results[0].score
  localStorage.setItem(BEST_KEY, String(best))
  return { results, best }
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

function calcPoints(offsetMs) {
  const abs = Math.abs(offsetMs)
  if (abs >= TOLERANCE_MS) return 0
  return Math.round(PERFECT_SCORE - (abs / TOLERANCE_MS) * (PERFECT_SCORE - MIN_SCORE))
}

export default function RhythmTestV1() {
  const [phase, setPhase] = useState("start")
  const [score, setScore] = useState(0)
  const [lives, setLives] = useState(MAX_LIVES)
  const [bpm, setBpm] = useState(STARTING_BPM)
  const [targetPos, setTargetPos] = useState({ x: 0, y: 0 })
  const [timingTexts, setTimingTexts] = useState([])
  const [beatFlash, setBeatFlash] = useState(false)
  const [localBest, setLocalBest] = useState(() => loadLocalBest())
  const [localResults, setLocalResults] = useState(() => loadLocalResults())
  const [submissionStatus, setSubmissionStatus] = useState(null)
  const [user, setUser] = useState(null)
  const [leaderboard, setLeaderboard] = useState([])
  const [leaderboardLoading, setLeaderboardLoading] = useState(false)
  const [leaderboardError, setLeaderboardError] = useState(false)

  const playRef = useRef(null)
  const scoreRef = useRef(0)
  const livesRef = useRef(MAX_LIVES)
  const bpmRef = useRef(STARTING_BPM)
  const beatTimerRef = useRef(null)
  const bpmTimerRef = useRef(null)
  const lastBeatTimeRef = useRef(0)
  const clickedThisBeatRef = useRef(false)
  const timingIdRef = useRef(0)
  const gameOverRef = useRef(false)

  function spawnCircle() {
    const el = playRef.current
    if (!el) return
    const rect = el.getBoundingClientRect()
    const pos = randomInRect(rect.width, rect.height, CIRCLE_RADIUS)
    setTargetPos(pos)
  }

  function onBeat() {
    if (gameOverRef.current) return
    const now = performance.now()

    if (!clickedThisBeatRef.current) {
      livesRef.current -= 1
      setLives(livesRef.current)
      if (livesRef.current <= 0) {
        gameOverRef.current = true
        clearTimeout(beatTimerRef.current)
        clearInterval(bpmTimerRef.current)
        finishGame()
        return
      }
    }

    clickedThisBeatRef.current = false
    lastBeatTimeRef.current = now
    spawnCircle()
    setBeatFlash(true)
    setTimeout(() => setBeatFlash(false), 100)

    const intervalMs = 60000 / bpmRef.current
    beatTimerRef.current = setTimeout(onBeat, intervalMs)
  }

  function handlePlayAreaClick(e) {
    if (phase !== "playing" || gameOverRef.current) return

    const el = playRef.current
    if (!el) return

    const rect = el.getBoundingClientRect()
    const clickX = e.clientX - rect.left
    const clickY = e.clientY - rect.top
    const dx = clickX - targetPos.x
    const dy = clickY - targetPos.y
    const dist = Math.sqrt(dx * dx + dy * dy)

    if (dist > CIRCLE_RADIUS) return

    if (clickedThisBeatRef.current) return
    clickedThisBeatRef.current = true

    const now = performance.now()
    const offset = now - lastBeatTimeRef.current
    const absOffset = Math.abs(offset)

    if (absOffset > TOLERANCE_MS) {
      return
    }

    const points = calcPoints(offset)
    scoreRef.current += points
    setScore(scoreRef.current)

    const sign = offset >= 0 ? "+" : ""
    const id = ++timingIdRef.current
    setTimingTexts(prev => [...prev, { id, text: `${sign}${Math.round(offset)}ms`, x: clickX, y: clickY }])
    setTimeout(() => {
      setTimingTexts(prev => prev.filter(t => t.id !== id))
    }, 1000)
  }

  function finishGame() {
    const finalScore = scoreRef.current
    saveLocalResult(finalScore)
    setLocalBest(loadLocalBest())
    setLocalResults(loadLocalResults())
    setPhase("results")
    setSubmissionStatus("submitting")

    if (user) {
      fetch("/api/games/rhythm-test-v1/scores", {
        method: "POST",
        credentials: "include",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify({ time: finalScore }),
      })
        .then(r => r.json())
        .then(d => {
          setSubmissionStatus(d.success ? "success" : "error")
        })
        .catch(() => setSubmissionStatus("error"))
    } else {
      setSubmissionStatus("success")
    }
  }

  function startGame() {
    gameOverRef.current = false
    scoreRef.current = 0
    livesRef.current = MAX_LIVES
    bpmRef.current = STARTING_BPM
    clickedThisBeatRef.current = false

    setPhase("playing")
    setScore(0)
    setLives(MAX_LIVES)
    setBpm(STARTING_BPM)
    setTimingTexts([])

    lastBeatTimeRef.current = performance.now()
    spawnCircle()
    setBeatFlash(true)
    setTimeout(() => setBeatFlash(false), 100)

    const intervalMs = 60000 / STARTING_BPM
    beatTimerRef.current = setTimeout(onBeat, intervalMs)

    bpmTimerRef.current = setInterval(() => {
      bpmRef.current += BPM_INCREASE
      setBpm(bpmRef.current)
    }, BPM_INTERVAL_MS)
  }

  useEffect(() => {
    fetch("/api/auth/me", { credentials: "include" })
      .then(r => r.json())
      .then(data => { if (data.success) setUser(data.user) })
      .catch(() => {})

    setLocalBest(loadLocalBest())
    setLocalResults(loadLocalResults())
    fetchLeaderboard()
  }, [])

  function fetchLeaderboard() {
    setLeaderboardLoading(true)
    fetch("/api/games/rhythm-test-v1/leaderboard")
      .then(r => r.json())
      .then(d => {
        if (d.success && d.leaderboard) setLeaderboard(d.leaderboard)
        setLeaderboardLoading(false)
      })
      .catch(() => { setLeaderboardLoading(false); setLeaderboardError(true) })
  }

  useEffect(() => {
    return () => {
      clearTimeout(beatTimerRef.current)
      clearInterval(bpmTimerRef.current)
    }
  }, [])

  function retry() {
    clearTimeout(beatTimerRef.current)
    clearInterval(bpmTimerRef.current)
    setPhase("start")
    setScore(0)
  }

  if (phase === "start") {
    return (
      <Layout>
        <div className="gamePage">
          <div className="gameInner">
            <div className="gameStartScreen">
              <h1 className="gameTitle">Rhythm Test v1</h1>
              <p className="gameSubtitle">Click in time with the beat. {STARTING_BPM} BPM + {BPM_INCREASE} every {BPM_INTERVAL_MS / 1000}s.</p>
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
            <div className="rhythmHud">
              <div className="rhythmHudRow">
                <div className="rhythmScore">{score.toLocaleString()}</div>
                <div className="rhythmBpm">{bpm} BPM</div>
              </div>
              <div className="rhythmHudRow">
                <div className="rhythmLives">
                  {Array.from({ length: MAX_LIVES }, (_, i) => (
                    <span key={i} className={"rhythmHeart" + (i < lives ? "" : " rhythmHeartLost")}>&#9829;</span>
                  ))}
                </div>
              </div>
            </div>
            <div
              className={"rhythmPlayArea" + (beatFlash ? " rhythmBeat" : "")}
              ref={playRef}
              onClick={handlePlayAreaClick}
            >
              <div
                className="rhythmTarget"
                style={{ left: targetPos.x, top: targetPos.y }}
              />
              {timingTexts.map(t => (
                <div key={t.id} className="rhythmTimingText" style={{ left: t.x, top: t.y }}>{t.text}</div>
              ))}
            </div>
          </div>
        </div>
      </Layout>
    )
  }

  const displayBest = localBest
  const displayResults = localResults

  return (
    <Layout>
      <div className="gamePage">
        <div className="gameInner">
          <div className="gameResultsScreen">
            <h1 className="gameTitle">Final Score</h1>
            <p className="gameFinalScore">{score.toLocaleString()}</p>

            <div className="gameLeaderboard">
              <div className="gameLeaderboardCol">
                <h3>Your Best</h3>
                <div className="gameLeaderboardList">
                  {displayBest != null ? (
                    <div className="gameLeaderboardRow gameLeaderboardRowBest">
                      <span className="gameLbRank">#1</span>
                      <span className="gameLbTime">{displayBest.toLocaleString()}</span>
                    </div>
                  ) : <p className="gameLbEmpty">No scores yet.</p>}
                </div>
                <h3 style={{marginTop:'0.75rem'}}>Recent</h3>
                <div className="gameLeaderboardList">
                  {displayResults.length > 0 ? displayResults.slice(0, 5).map((r, i) => (
                    <div key={i} className="gameLeaderboardRow">
                      <span className="gameLbRank">#{i + 1}</span>
                      <span className="gameLbTime">{(typeof r === 'object' ? r.score : r).toLocaleString()}</span>
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
                      <span className="gameLbName"><Username user={entry} size="sm" /></span>
                      <span className="gameLbTime">{Number(entry.score_value).toLocaleString()}</span>
                    </div>
                  )) : (
                    <p className="gameLbEmpty">
                      {leaderboardLoading ? "Loading leaderboard..." : leaderboardError ? "Unable to load leaderboard." : "No global scores yet."}
                    </p>
                  )}
                </div>
                {submissionStatus === "submitting" && !user && (
                  <p className="gameLbEmpty" style={{marginTop:'0.5rem'}}>Not logged in — score saved locally only.</p>
                )}
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
