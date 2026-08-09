import { useState, useEffect, useRef } from "react"
import { Link } from "react-router-dom"
import Layout from "../components/Layout"
import Username from "../components/Username"

const GOAL = 50
const WIN_MIN = 36
const WIN_MAX = 48
const SPRINT_LEAD = 8
const CATCH_SPEED = 8
const BASE_RATE_MIN = 6
const BASE_RATE_MAX = 8
const BEST_KEY = "chilirace_best"
const RESULTS_KEY = "chilirace_results"
const LEADERBOARD_KEY = "chilirace_leaderboard"
const LEADERBOARD_CACHE_MS = 60000

const ASSET_PATHS = {
  bg: "/assets/images/chili-race-bg.png",
  playerSprite: "/assets/images/plrsprite.png",
  npcSprite: "/assets/images/npcsprite.png",
  chiliSprite: "/assets/images/chilisprite.png",
  music: "/assets/audio/mimita-entertainer-chili-race-small.ogg",
  clickSound: "/assets/audio/chili-click.mp3",
  loseSound: "/assets/audio/wii-aww.mp3",
  countSound: "/assets/audio/target-hit.wav",
  goSound: "/assets/audio/gosound.mp3",
}

// The CPU always eats at a steady clip (6-8 chilis/sec, rolled per round), and
// on top of that it trails YOUR progress so it's just behind while you're
// winning, then sprints past right as you hit a secret finish point.
function npcTarget(p, winPoint) {
  const sprintStart = winPoint - SPRINT_LEAD
  if (p < sprintStart) return p * 0.9
  if (p < winPoint) {
    return sprintStart * 0.9 + (p - sprintStart) * (GOAL - sprintStart * 0.9) / (winPoint - sprintStart)
  }
  return GOAL
}

function rollWinPoint() {
  return WIN_MIN + Math.floor(Math.random() * (WIN_MAX - WIN_MIN + 1))
}

function rollBaseRate() {
  return BASE_RATE_MIN + Math.random() * (BASE_RATE_MAX - BASE_RATE_MIN)
}

function cpsMessage(cps) {
  if (cps < 4) return "Bro are you even trying bro..."
  if (cps < 7) return "Wait you might win..."
  if (cps < 10) return "faster faster faster!!!!!!!!!!"
  if (cps < 15) return "SUPER SPEED!!!!!!!!!!!!!!"
  return "hackiog!!!?!!???!?!?"
}

function loadLocalBest() {
  try { const raw = localStorage.getItem(BEST_KEY); return raw ? parseInt(raw) : null }
  catch { return null }
}
function loadLocalResults() {
  try { const raw = localStorage.getItem(RESULTS_KEY); return raw ? JSON.parse(raw) : [] }
  catch { return [] }
}
function saveLocalResult(clicks) {
  const results = loadLocalResults()
  results.push({ clicks, date: Date.now() })
  results.sort((a, b) => b.clicks - a.clicks)
  if (results.length > 10) results.length = 10
  localStorage.setItem(RESULTS_KEY, JSON.stringify(results))
  const best = results[0].clicks
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
  catch { /* ignore */ }
}
function safeJson(resp) {
  return resp.text().then(text => {
    try { return JSON.parse(text) }
    catch { throw new Error("JSON parse failed") }
  })
}

export default function ChiliRace() {
  const [phase, setPhase] = useState("menu")
  const [countdown, setCountdown] = useState(3)
  const [showGo, setShowGo] = useState(false)
  const [playerClicks, setPlayerClicks] = useState(0)
  const [npcProgress, setNpcProgress] = useState(0)
  const [cps, setCps] = useState(0)
  const [pressed, setPressed] = useState(false)
  const [localBest, setLocalBest] = useState(() => loadLocalBest())
  const [localResults, setLocalResults] = useState(() => loadLocalResults())
  const [user, setUser] = useState(null)
  const [leaderboard, setLeaderboard] = useState(() => loadCachedLeaderboard())
  const [leaderboardLoading, setLeaderboardLoading] = useState(false)
  const [leaderboardError, setLeaderboardError] = useState(false)
  const [submissionStatus, setSubmissionStatus] = useState(null)

  const playerClicksRef = useRef(0)
  const npcRef = useRef(0)
  const clicksTsRef = useRef([])
  const raceOverRef = useRef(false)
  const winPointRef = useRef(WIN_MAX)
  const baseRateRef = useRef(BASE_RATE_MAX)
  const rafRef = useRef(null)
  const lastFrameRef = useRef(0)
  const countdownTimerRef = useRef(null)
  const finishTimerRef = useRef(null)
  const musicRef = useRef(null)
  const clickAudioRef = useRef(null)
  const loseAudioRef = useRef(null)
  const countAudioRef = useRef(null)
  const goAudioRef = useRef(null)

  function getMusic() {
    if (!musicRef.current) {
      musicRef.current = new Audio(ASSET_PATHS.music)
      musicRef.current.loop = true
      musicRef.current.volume = 0.4
    }
    return musicRef.current
  }
  function getClickAudio() {
    if (!clickAudioRef.current) clickAudioRef.current = new Audio(ASSET_PATHS.clickSound)
    return clickAudioRef.current
  }
  function getLoseAudio() {
    if (!loseAudioRef.current) loseAudioRef.current = new Audio(ASSET_PATHS.loseSound)
    return loseAudioRef.current
  }
  function getCountAudio() {
    if (!countAudioRef.current) countAudioRef.current = new Audio(ASSET_PATHS.countSound)
    return countAudioRef.current
  }
  function getGoAudio() {
    if (!goAudioRef.current) goAudioRef.current = new Audio(ASSET_PATHS.goSound)
    return goAudioRef.current
  }
  function playCountdownTick() {
    const tick = getCountAudio()
    tick.currentTime = 0
    tick.play().catch(() => {})
  }
  function playGo() {
    const go = getGoAudio()
    go.currentTime = 0
    go.play().catch(() => {})
  }

  function fetchLeaderboard() {
    const cached = loadCachedLeaderboard()
    if (cached) return
    setLeaderboardLoading(true)
    setLeaderboardError(false)
    fetch("/api/games/chili-race/leaderboard")
      .then(safeJson)
      .then(d => {
        if (d.success && d.leaderboard) {
          setLeaderboard(d.leaderboard)
          cacheLeaderboard(d.leaderboard)
        }
        setLeaderboardLoading(false)
      })
      .catch(() => {
        setLeaderboardLoading(false)
        setLeaderboardError(true)
      })
  }

  useEffect(() => {
    function startMusicOnce() {
      const music = getMusic()
      if (music.paused) music.play().catch(() => {})
      document.removeEventListener("pointerdown", startMusicOnce)
      document.removeEventListener("keydown", startMusicOnce)
    }
    document.addEventListener("pointerdown", startMusicOnce)
    document.addEventListener("keydown", startMusicOnce)

    fetch("/api/auth/me", { credentials: "include" })
      .then(safeJson)
      .then(data => { if (data.success) setUser(data.user) })
      .catch(() => {})
    Promise.resolve().then(fetchLeaderboard)
    return () => {
      document.removeEventListener("pointerdown", startMusicOnce)
      document.removeEventListener("keydown", startMusicOnce)
      cancelAnimationFrame(rafRef.current)
      clearInterval(countdownTimerRef.current)
      clearTimeout(finishTimerRef.current)
      if (musicRef.current) musicRef.current.pause()
    }
  }, [])

  function lose() {
    if (raceOverRef.current) return
    raceOverRef.current = true
    cancelAnimationFrame(rafRef.current)
    npcRef.current = GOAL
    setNpcProgress(GOAL)
    setPlayerClicks(playerClicksRef.current)
    const aww = getLoseAudio()
    aww.currentTime = 0
    aww.play().catch(() => {})
    setPhase("finish")
    finishTimerRef.current = setTimeout(() => finishGame(), 1200)
  }

  function tick(now) {
    const dt = lastFrameRef.current ? Math.min(0.1, (now - lastFrameRef.current) / 1000) : 0
    lastFrameRef.current = now

    const p = playerClicksRef.current
    const winPoint = winPointRef.current
    if (p >= winPoint) { lose(); return }

    const target = npcTarget(p, winPoint)
    const gain = Math.max(0, target - npcRef.current) * CATCH_SPEED * dt + baseRateRef.current * dt
    let npc = Math.min(GOAL, npcRef.current + gain)
    npcRef.current = npc
    setNpcProgress(npc)

    const ts = clicksTsRef.current
    while (ts.length && now - ts[0] > 1000) ts.shift()
    setCps(ts.length)

    if (npc >= GOAL) { lose(); return }
    rafRef.current = requestAnimationFrame(tick)
  }

  function startRound() {
    setPhase("playing")
    lastFrameRef.current = 0
    rafRef.current = requestAnimationFrame(tick)
  }

  function startGame() {
    setLocalBest(loadLocalBest())
    playerClicksRef.current = 0
    npcRef.current = 0
    clicksTsRef.current = []
    raceOverRef.current = false
    winPointRef.current = rollWinPoint()
    baseRateRef.current = rollBaseRate()
    setPlayerClicks(0)
    setNpcProgress(0)
    setCps(0)

    const music = getMusic()
    if (music.paused) music.play().catch(() => {})

    setPhase("countdown")
    setCountdown(3)
    setShowGo(false)
    playCountdownTick()
    let n = 3
    countdownTimerRef.current = setInterval(() => {
      n -= 1
      if (n <= 0) {
        clearInterval(countdownTimerRef.current)
        setShowGo(true)
        playGo()
        finishTimerRef.current = setTimeout(() => startRound(), 700)
      } else {
        setCountdown(n)
        playCountdownTick()
      }
    }, 1000)
  }

  function handleSmash(ts) {
    if (phase !== "playing" || raceOverRef.current) return
    playerClicksRef.current += 1
    setPlayerClicks(playerClicksRef.current)
    clicksTsRef.current.push(ts)
    const click = getClickAudio()
    click.currentTime = 0
    click.play().catch(() => {})
    if (playerClicksRef.current >= winPointRef.current) lose()
  }

  function finishGame() {
    const clicks = playerClicksRef.current
    const { results, best } = saveLocalResult(clicks)
    setLocalResults(results)
    setLocalBest(best)
    setPhase("results")
    setSubmissionStatus("submitting")

    if (user && clicks > 0) {
      fetch("/api/games/chili-race/scores", {
        method: "POST",
        credentials: "include",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify({ time: clicks }),
      })
        .then(safeJson)
        .then(d => setSubmissionStatus(d.success ? "success" : "error"))
        .catch(() => setSubmissionStatus("error"))
        .then(() => {
          setLeaderboardLoading(true)
          setLeaderboardError(false)
          return fetch("/api/games/chili-race/leaderboard")
        })
        .then(safeJson)
        .then(d => {
          if (d.success && d.leaderboard) {
            setLeaderboard(d.leaderboard)
            cacheLeaderboard(d.leaderboard)
          }
          setLeaderboardLoading(false)
        })
        .catch(() => setLeaderboardLoading(false))
    } else {
      setSubmissionStatus("success")
    }
  }

  function retry() {
    cancelAnimationFrame(rafRef.current)
    clearInterval(countdownTimerRef.current)
    clearTimeout(finishTimerRef.current)
    setPhase("menu")
    setShowGo(false)
    setPlayerClicks(0)
    setNpcProgress(0)
    setCps(0)
    setPressed(false)
  }

  useEffect(() => {
    function onKey(e) {
      if ((e.key === " " || e.key === "Enter") && phase === "playing") {
        e.preventDefault()
        if (!e.repeat) handleSmash(e.timeStamp)
      }
    }
    window.addEventListener("keydown", onKey)
    return () => window.removeEventListener("keydown", onKey)
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, [phase])

  const chiliSlots = Array.from({ length: 10 }, (_, i) => (i + 1) * 5)
  function chiliFilled(slot, progress) { return progress >= slot }

  if (phase === "menu") {
    return (
      <Layout>
        <div className="gamePage chiliRacePage">
          <div className="chiliRaceBg">
            <h1 className="chiliRaceTitle">CHILI RACE!!!!!111!!</h1>
            <p className="chiliRaceSubtitle">The Great Chili Eating Showdown.......<br />Out-eat the Champ to take the crown. Or get out-eaten!!!! <br /> Spacebar + mouse + mobile works!!!</p>
            <button className="chiliRacePlayBtn" onClick={startGame}>PLAY</button>
            {localBest != null && (
              <p className="chiliRaceBest">Best: {localBest}/50</p>
            )}
          </div>
        </div>
      </Layout>
    )
  }

  if (phase === "countdown") {
    return (
      <Layout>
        <div className="gamePage chiliRacePage">
          <div className="chiliRaceBg">
            {showGo ? (
              <div className="chiliRaceGo" key="go">GO!!!</div>
            ) : (
              <div className="chiliRaceCountdown" key={countdown}>{countdown}</div>
            )}
          </div>
        </div>
      </Layout>
    )
  }

  if (phase === "playing" || phase === "finish") {
    const over = phase === "finish"
    return (
      <Layout>
        <div className="gamePage chiliRacePage">
          <div className="chiliRaceBg">
            <div className="chiliRaceHud">
              <div className="chiliRaceCps">{cps} <span className="chiliRaceCpsUnit">cps</span></div>
              <div className="chiliRaceMsg">{cpsMessage(cps)}</div>
            </div>

            <div className="chiliRaceBoard">
              <div className="chiliRacePanel chiliRacePanelYou">
                <img className="chiliRaceSprite" src={ASSET_PATHS.playerSprite} alt="You" draggable={false} />
                <div className="chiliRaceName chiliRaceNameYou">YOU</div>
                <div className="chiliRaceCount">{playerClicks}/50</div>
                <div className="chiliRaceChiliRow">
                  {chiliSlots.map(s => (
                    <span key={s} className={"chiliRaceChiliSlot" + (chiliFilled(s, playerClicks) ? " chiliRaceChiliFilled" : " chiliRaceChiliEmpty")}>
                      <img src={ASSET_PATHS.chiliSprite} alt="" className="chiliRaceChiliImg" draggable={false} />
                    </span>
                  ))}
                </div>
              </div>

              <div className="chiliRacePanel chiliRacePanelCpu">
                <img className="chiliRaceSprite" src={ASSET_PATHS.npcSprite} alt="CPU" draggable={false} />
                <div className="chiliRaceName chiliRaceNameCpu">CPU</div>
                <div className="chiliRaceCount">{Math.floor(npcProgress)}/50</div>
                <div className="chiliRaceChiliRow">
                  {chiliSlots.map(s => (
                    <span key={s} className={"chiliRaceChiliSlot" + (chiliFilled(s, npcProgress) ? " chiliRaceChiliFilled" : " chiliRaceChiliEmpty")}>
                      <img src={ASSET_PATHS.chiliSprite} alt="" className="chiliRaceChiliImg" draggable={false} />
                    </span>
                  ))}
                </div>
              </div>
            </div>

            <button
              className={"chiliRaceButton" + (pressed ? " chiliRacePressed" : "")}
              onPointerDown={e => { setPressed(true); handleSmash(e.timeStamp) }}
              onPointerUp={() => setPressed(false)}
              onPointerLeave={() => setPressed(false)}
              onContextMenu={e => e.preventDefault()}
            >
              SMASH
            </button>
            <p className="chiliRaceHint">Step right up! Mash to eat! (mouse, touch, or spacebar)</p>

            {over && (
              <div className="chiliRaceFinish">
                <div className="chiliRaceFinishText">THE CHAMP WINS!!!</div>
              </div>
            )}
          </div>
        </div>
      </Layout>
    )
  }

  const peppers = Math.max(0, 5 - Math.floor(playerClicks / 10))
  const resultHead =
    playerClicks >= 42 ? "SO CLOSE!" :
    playerClicks >= 30 ? "SO CLOSE KINDA A LITTLE!" :
    "THE CHAMP WINS"
  const taunt =
    playerClicks >= 42
      ? "The Champ gobbled the last chili right under your nose! Try again!??"
      : playerClicks >= 30
        ? "You put up a fight, but the Champ's belly is bottomless. Maybe click faster?"
        : "Too slow, champ-to-be. Step right up and try again! And try to win this time."

  return (
    <Layout>
      <div className="gamePage chiliRacePage">
        <div className="chiliRaceBg">
          <div className="chiliRaceResults">
            <h1 className="chiliRaceTitle">{resultHead}</h1>
            <div className="chiliRaceResultCount">You ate {playerClicks}/50</div>
            <div className="chiliRacePeppers">{"🌶️".repeat(peppers)}</div>
            <p className="chiliRaceTaunt">{taunt}</p>

            <div className="gameResultsButtons">
              <button className="gameStartBtn" onClick={retry}>Retry</button>
              <Link to="/games" className="gameStartBtn gameExitBtn">Exit</Link>
            </div>

            <div className="gameLeaderboard">
              <div className="gameLeaderboardCol">
                <h3>Your Best</h3>
                <div className="gameLeaderboardList">
                  {localBest != null ? (
                    <div className="gameLeaderboardRow gameLeaderboardRowBest">
                      <span className="gameLbRank">#1</span>
                      <span className="gameLbTime">{localBest}/50</span>
                    </div>
                  ) : <p className="gameLbEmpty">No scores yet.</p>}
                </div>
                <h3 style={{ marginTop: '0.75rem' }}>Recent</h3>
                <div className="gameLeaderboardList">
                  {localResults.length > 0 ? localResults.slice(0, 5).map((r, i) => (
                    <div key={i} className="gameLeaderboardRow">
                      <span className="gameLbRank">#{i + 1}</span>
                      <span className="gameLbTime">{(typeof r === 'object' ? r.clicks : r)}/50</span>
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
                      <span className="gameLbTime">{Number(entry.score_value)}/50</span>
                    </div>
                  )) : (
                    <p className="gameLbEmpty">
                      {leaderboardLoading ? "Loading leaderboard..." : leaderboardError ? "Unable to load leaderboard." : "No global scores yet."}
                    </p>
                  )}
                </div>
                {submissionStatus === "submitting" && !user && (
                  <p className="gameLbEmpty" style={{ marginTop: '0.5rem' }}>Not logged in — score saved locally only.</p>
                )}
                {submissionStatus === "error" && (
                  <p className="gameSubmissionError">Unable to submit score.</p>
                )}
              </div>
            </div>
          </div>
        </div>
      </div>
    </Layout>
  )
}
