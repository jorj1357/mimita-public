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
    const [user, setUser] = useState(null)
    const playRef = useRef(null)
    const timerRef = useRef(null)
    const startTimeRef = useRef(0)
    const targetsHitRef = useRef(0)

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

        // Load leaderboard (cached first, then background refresh if stale)
        const cached = loadCachedLeaderboard()
        if (cached) setLeaderboard(cached)
        fetchLeaderboard()
    }, [])

    function fetchLeaderboard() {
        const cached = loadCachedLeaderboard()
        if (cached) return
        setLeaderboardLoading(true)
        fetch("/api/games/aim-test-v1/leaderboard")
            .then(r => r.json())
            .then(d => {
                if (d.success && d.leaderboard) {
                    setLeaderboard(d.leaderboard)
                    cacheLeaderboard(d.leaderboard)
                }
                setLeaderboardLoading(false)
            })
            .catch(() => setLeaderboardLoading(false))
    }

    const spawnTarget = useCallback(() => {
        const el = playRef.current
        if (!el) return
        const rect = el.getBoundingClientRect()
        const pos = randomInRect(rect.width, rect.height, TARGET_RADIUS)
        setTargetPos(pos)
    }, [])

    function startGame() {
        setPhase("playing")
        setTargetsHit(0)
        setTime(0)
        targetsHitRef.current = 0
        startTimeRef.current = performance.now()
        const playAgain = () => {
            setTime((performance.now() - startTimeRef.current) / 1000)
            timerRef.current = requestAnimationFrame(playAgain)
        }
        timerRef.current = requestAnimationFrame(playAgain)
        spawnTarget()
    }

    function handleTargetClick(e) {
        e.stopPropagation()
        const hit = targetsHitRef.current + 1
        targetsHitRef.current = hit
        setTargetsHit(hit)
        if (hit >= TOTAL_TARGETS) {
            const elapsed = (performance.now() - startTimeRef.current) / 1000
            setTime(elapsed)
            cancelAnimationFrame(timerRef.current)
            finishGame(elapsed)
            return
        }
        spawnTarget()
    }

    function finishGame(elapsed) {
        const { results, best } = saveLocalResult(elapsed)
        setLocalResults(results)
        setLocalBest(best)

        if (user) {
            fetch("/api/games/aim-test-v1/scores", {
                method: "POST", credentials: "include",
                headers: { "Content-Type": "application/json" },
                body: JSON.stringify({ time: elapsed }),
            }).then(() => {
                fetch("/api/games/aim-test-v1/scores", { credentials: "include" })
                    .then(r => r.json())
                    .then(d => { if (d.success) { if (d.best != null) setServerBest(d.best); if (d.recent) setServerResults(d.recent) } })
                    .catch(() => {})
                // Refresh leaderboard after new score
                const cached = loadCachedLeaderboard()
                if (cached) { setLeaderboard(cached); return }
                fetch("/api/games/aim-test-v1/leaderboard")
                    .then(r => r.json())
                    .then(d => { if (d.success && d.leaderboard) { setLeaderboard(d.leaderboard); cacheLeaderboard(d.leaderboard) } })
                    .catch(() => {})
            }).catch(() => {})
        }

        // Invalidate leaderboard cache so it refreshes next time
        try { localStorage.removeItem(LEADERBOARD_KEY) } catch {}
        setPhase("results")

        // Refresh leaderboard in background
        fetch("/api/games/aim-test-v1/leaderboard")
            .then(r => r.json())
            .then(d => {
                if (d.success && d.leaderboard) {
                    setLeaderboard(d.leaderboard)
                    cacheLeaderboard(d.leaderboard)
                }
            })
            .catch(() => {})
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
            <div className="gamePage">
                <div className="gameInner">
                    <div className="gameHud">
                        <div className="gameTimer">{time.toFixed(2)}</div>
                        <div className="gameTargetCount">{targetsHit}/{TOTAL_TARGETS}</div>
                    </div>
                    <div className="gamePlayArea" ref={playRef}>
                        {targetsHit < TOTAL_TARGETS && (
                            <div
                                className="gameTarget"
                                style={{
                                    left: targetPos.x,
                                    top: targetPos.y,
                                    width: TARGET_RADIUS * 2,
                                    height: TARGET_RADIUS * 2,
                                }}
                                onClick={handleTargetClick}
                            />
                        )}
                    </div>
                </div>
            </div>
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
                                        <div key={i} className="gameLeaderboardRow">
                                            <span className="gameLbRank">#{entry.rank || (i + 1)}</span>
                                            <span className="gameLbName">{entry.username || "?"}</span>
                                            <span className="gameLbTime">{Number(entry.score_value).toFixed(2)}s</span>
                                        </div>
                                    )) : (
                                        <p className="gameLbEmpty">
                                            {leaderboardLoading ? "Loading..." : "No global scores yet."}
                                        </p>
                                    )}
                                </div>
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
