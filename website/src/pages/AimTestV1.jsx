import { useState, useEffect, useRef, useCallback } from "react"
import { Link } from "react-router-dom"
import Layout from "../components/Layout"
import Username from "../components/Username"

const TOTAL_TARGETS = 30
const TARGET_RADIUS = 50
const PLAY_AREA_PADDING = 12
const BEST_KEY = "aimtestv1_best"
const RESULTS_KEY = "aimtestv1_results"
const LEADERBOARD_KEY = "aimtestv1_leaderboard"
const LEADERBOARD_CACHE_MS = 60000

const ASSET_PATHS = {
  crosshairImage: "/assets/images/aimcursor.png?v=2",
  targetImage: "/assets/images/target-placeholder.png?v=2",
  hitSound: "/assets/audio/target-hit.wav?v=2",
  missSound: "/assets/audio/miss-click.wav?v=2",
}

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

function safeJson(resp) {
  return resp.text().then(text => {
    try {
      return JSON.parse(text)
    } catch (e) {
      console.log("[AimTrainer][Submit] JSON parse failed", JSON.stringify({
        rawTextFirst500: text.slice(0, 500),
        errorMessage: e.message,
      }))
      throw new Error("JSON parse failed: " + e.message)
    }
  })
}

function isDebugMode() {
  return typeof window !== "undefined" && window.location.search.includes("debugAim=1")
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
  const debugMode = isDebugMode()

  function getHitAudio() {
    if (!hitAudioRef.current) hitAudioRef.current = new Audio(ASSET_PATHS.hitSound)
    return hitAudioRef.current
  }
  function getMissAudio() {
    if (!missAudioRef.current) missAudioRef.current = new Audio(ASSET_PATHS.missSound)
    return missAudioRef.current
  }

  function verifyAsset(path, type) {
    console.log("[AimTrainer][Assets]", JSON.stringify({ [type]: path }))
    if (type === "targetImage" || type === "crosshairImage") {
      const img = new Image()
      const label = type === "targetImage" ? "target image" : "crosshair image"
      img.onload = () => console.log("[AimTrainer][Assets] " + label + " loaded", JSON.stringify({ src: path, width: img.width, height: img.height }))
      img.onerror = () => console.log("[AimTrainer][Assets] " + label + " failed", JSON.stringify({ src: path }))
      img.src = path
    } else {
      const audio = new Audio(path)
      audio.oncanplaythrough = () => console.log("[AimTrainer][Assets] " + type + " loaded", JSON.stringify({ src: path }))
      audio.onerror = (e) => console.log("[AimTrainer][Assets] " + type + " failed", JSON.stringify({ src: path, error: e.message || "unknown", networkState: audio.networkState, readyState: audio.readyState }))
    }
  }

  useEffect(() => {
    console.log("[AimTrainer][Assets]", JSON.stringify(ASSET_PATHS))
    verifyAsset(ASSET_PATHS.crosshairImage, "crosshairImage")
    verifyAsset(ASSET_PATHS.targetImage, "targetImage")
    verifyAsset(ASSET_PATHS.hitSound, "hitSound")
    verifyAsset(ASSET_PATHS.missSound, "missSound")

    setLocalBest(loadLocalBest())
    setLocalResults(loadLocalResults())

    fetch("/api/auth/me", { credentials: "include" })
      .then(safeJson)
      .then(data => {
        if (data.success) {
          setUser(data.user)
          fetch("/api/games/aim-test-v1/scores", { credentials: "include" })
            .then(safeJson)
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
      .then(safeJson)
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

    const durationMs = elapsed * 1000
    const payload = { time: elapsed }

    console.log("[AimTrainer][Result]", JSON.stringify({
      durationMs,
      hits: targetsHitRef.current,
      misses: missIdRef.current,
      averageReactionMs: null,
      bestReactionMs: null,
      worstReactionMs: null,
      accuracy: null,
      finalScore: elapsed,
      submittedPayload: payload,
    }))

    // Validate payload
    for (const key of Object.keys(payload)) {
      const val = payload[key]
      if (val === null || val === undefined || val === "" || (typeof val === "number" && (isNaN(val) || val < 0))) {
        console.log("[AimTrainer][Result] invalid local score", JSON.stringify({ field: key, value: val }))
        setSubmissionStatus("error")
        return
      }
    }

    console.log("[AimTrainer][Submit] start", JSON.stringify({
      url: "/api/games/aim-test-v1/scores",
      method: "POST",
      payload,
      pageOrigin: window.location.origin,
      pathname: window.location.pathname,
      userAgent: navigator.userAgent.slice(0, 80),
      timestamp: new Date().toISOString(),
    }))

    const doSubmit = user
      ? fetch("/api/games/aim-test-v1/scores", {
          method: "POST", credentials: "include",
          headers: { "Content-Type": "application/json" },
          body: JSON.stringify(payload),
        }).then(resp => {
          console.log("[AimTrainer][Submit] response", JSON.stringify({
            status: resp.status,
            statusText: resp.statusText,
            ok: resp.ok,
            contentType: resp.headers.get("content-type"),
          }))
          if (!resp.ok) throw new Error("HTTP " + resp.status)
          return safeJson(resp)
        }).then(d => {
          if (!d.success) throw new Error("rejected: " + (d.error || d.message || "unknown"))
          console.log("[AimTrainer] Score submitted.", JSON.stringify(d))
          setSubmissionStatus("success")
        }).catch(err => {
          console.log("[AimTrainer] Score submission failed: " + err.message)
          console.log("[AimTrainer][Submit] error", JSON.stringify({ message: err.message, stack: (err.stack || "").slice(0, 200) }))
          setSubmissionStatus("error")
        })
      : Promise.resolve().then(() => {
          console.log("[AimTrainer] Score submission skipped (not logged in)")
          setSubmissionStatus("success")
        })

    doSubmit.then(() => {
      console.log("[AimTrainer] Fetching leaderboard...")
      setLeaderboardLoading(true)
      setLeaderboardError(false)
      return fetch("/api/games/aim-test-v1/leaderboard")
    })
    .then(safeJson)
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

  const debugPanel = debugMode ? (
    <div className="gameDebugPanel">
      <h4>Aim Trainer Debug</h4>
      <table><tbody>
        <tr><td>Crosshair image</td><td>{ASSET_PATHS.crosshairImage}</td></tr>
        <tr><td>Target image</td><td>{ASSET_PATHS.targetImage}</td></tr>
        <tr><td>Hit sound</td><td>{ASSET_PATHS.hitSound}</td></tr>
        <tr><td>Miss sound</td><td>{ASSET_PATHS.missSound}</td></tr>
        <tr><td>Auth status</td><td>{user ? "Logged in as " + user.username : "Not logged in"}</td></tr>
        <tr><td>Phase</td><td>{phase}</td></tr>
      </tbody></table>
    </div>
  ) : null

  if (phase === "start") {
    return (
      <Layout>
        <div className="gamePage">
          {debugPanel}
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
          {debugPanel}
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
        {debugPanel}
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
                      <span className="gameLbName"><Username user={entry} size="sm" /></span>
                      <span className="gameLbTime">{Number(entry.score_value).toFixed(2)}s</span>
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
