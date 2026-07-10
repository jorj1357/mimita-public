import { useRef, useState, useCallback, useEffect } from "react"
import { useNavigate } from "react-router-dom"
import "./DreamToy.css"
import Layout from "../components/Layout"
import Rail from "../components/dream/Rail"
import DebugMetrics from "../components/dream/DebugMetrics"
import { useDreamEngine } from "../components/dream/Engine"
import { useDreamInput } from "../components/dream/Input"
import { useDreamAudio } from "../components/dream/Audio"
import { spawnParticle, spawnExplosionParticles, tickIdleParticles } from "../components/dream/Particles"
import { addDrawSegment, symmetryPoints } from "../components/dream/Drawing"
import { spawnBall, spawnCube, triggerExplosion, clearPhysics, tickExplosions } from "../components/dream/Physics"
import { spawnBlackHole, tickBlackHoles, removeDeadObjects, clearBlackHoles } from "../components/dream/BlackHole"
import { spawnCreature, tickCreatures, clearCreatures } from "../components/dream/Creatures"
import { tickTextAnomalies, clearTextAnomalies } from "../components/dream/TextAnomaly"
import { initFractal, tickFractal, zoomFractalAt, toggleAutoZoom, resetFractalView, getZoomDisplay, getLastRenderMs, getRebaseCount } from "../components/dream/Fractal"
import { initGameOfLife, tickGameOfLife, clearGameOfLife, randomizeGameOfLife, toggleCell } from "../components/dream/GameOfLife"

export default function DreamToy() {
  const navigate = useNavigate()
  const canvasRef = useRef(null)
  const { getWorld, metrics } = useDreamEngine(canvasRef)
  const { state: input, resetJust } = useDreamInput(canvasRef)
  const audio = useDreamAudio()
  const [mode, setMode] = useState("dream")
  const [muted, setMuted] = useState(false)
  const [showHint, setShowHint] = useState(true)
  const lastDrawRef = useRef({ x: 0, y: 0 })
  const hueRef = useRef(0)
  const spawnCooldownRef = useRef(0)
  const clickCountRef = useRef(0)
  const lastClickTimeRef = useRef(0)

  // Hide hint on first interaction
  useEffect(() => {
    if (showHint && (input.current.down || input.current.history.length > 5)) {
      setShowHint(false)
    }
  }, [input.current.down, input.current.history.length, showHint])

  // Main update loop — called from Engine via a custom event pattern
  // Since the engine has its own rAF, we use the world state directly
  const updateRef = useRef(null)
  updateRef.current = () => {
    const w = getWorld()
    const s = input.current
    const px = s.pointerX
    const py = s.pointerY

    // Idle systems
    tickIdleParticles(w, 1/60)
    tickTextAnomalies(w, 1/60)
    tickExplosions(w, 1/60)
    tickBlackHoles(w, 1/60)
    tickCreatures(w, 1/60, px, py, s.down)
    tickFractal(w, 1/60)
    tickGameOfLife(w, 1/60)

    // Mode-specific behavior
    if (mode === "dream") {
      // Cursor influence on particles
      for (const p of w.particles) {
        const dx = px - p.x
        const dy = py - p.y
        const dist = Math.sqrt(dx * dx + dy * dy)
        if (dist < 100 && dist > 1) {
          p.vx += (dx / dist) * 60 * (1 - dist / 100)
          p.vy += (dy / dist) * 60 * (1 - dist / 100)
        }
      }
    }

    if (mode === "draw" || mode === "dream") {
      if (s.down && s.history.length >= 2) {
        const last = s.history[s.history.length - 2]
        const cur = s.history[s.history.length - 1]
        const sym = 0 // symmetry level; could be controlled by a slider
        const pts = symmetryPoints(cur.x, cur.y, last.x, last.y, sym, w.width / 2, w.height / 2)
        for (const p of pts) {
          addDrawSegment(w, p.x1, p.y1, p.x2, p.y2, hueRef.current)
        }
        hueRef.current = (hueRef.current + 2) % 360
        if (w.tick % 3 === 0) audio.playDraw()
      }
    }

    // Click actions
    if (s.justDown) {
      audio.start()
      const now = performance.now()
      const timeSinceLastClick = now - lastClickTimeRef.current
      const isDoubleClick = timeSinceLastClick < 250
      lastClickTimeRef.current = now
      if (timeSinceLastClick < 300) clickCountRef.current++
      else clickCountRef.current = 1
      const clickBonus = Math.min(clickCountRef.current, 20)

      switch (mode) {
        case "fractal":
          if (w.fractal) {
            zoomFractalAt(w, px, py)
            audio.playTone("ui", 300, 0.1, "sine", 0.08)
          }
          break
        case "life":
          toggleCell(w, px, py)
          audio.playTone("ui", 500, 0.04, "sine", 0.04)
          break
        case "physics":
          if (spawnCooldownRef.current <= 0) {
            spawnBall(w, px, py, (s.velX || 0) * 0.3, (s.velY || 0) * 0.3)
            spawnCooldownRef.current = 0.1
            spawnParticle(w, px, py, 15 + clickBonus * 3)
          }
          break
        case "life":
          spawnCreature(w, px, py)
          audio.playCreature()
          spawnParticle(w, px, py, 10 + clickBonus * 2)
          break
        case "space":
          if (isDoubleClick) {
            // Double-click: dense small black hole
            spawnBlackHole(w, px, py, 30, 350, true)
            audio.playBlackHole()
          }
          break
        case "draw":
          lastDrawRef.current = { x: px, y: py }
          break
        default:
          spawnParticle(w, px, py, 20 + clickBonus * 5)
          break
      }
      audio.playTone("ui", 400 + clickBonus * 30, 0.06, "sine", 0.06)
    }

    // Black hole drag: hold and drag to create larger black hole on release
    if (mode === "space" && s.justUp && s.history.length > 10) {
      const first = s.history[0]
      const last = s.history[s.history.length - 1]
      const dragDist = Math.sqrt((last.x - first.x) ** 2 + (last.y - first.y) ** 2)
      if (dragDist > 30) {
        const size = Math.min(dragDist * 0.5, 200)
        const strength = Math.min(dragDist * 1.5, 600)
        spawnBlackHole(w, px, py, 30 + size, 200 + strength, false)
        audio.playBlackHole()
        audio.playExplosion()
      }
    }

    // Right-click / long press for explosion
    if (s.justUp && mode === "physics") {
      const vel = Math.sqrt(s.velX * s.velX + s.velY * s.velY)
      if (vel > 200) {
        triggerExplosion(w, px, py, 50 + vel * 0.2, 200 + vel * 0.5)
        spawnExplosionParticles(w, px, py, 30)
        audio.playExplosion()
      }
    }

    // Physics object spawning while holding
    spawnCooldownRef.current = Math.max(0, (spawnCooldownRef.current || 0) - 1/60)
    if (mode === "physics" && s.down && spawnCooldownRef.current <= 0 && s.history.length > 5) {
      // Throw objects while dragging
      const speed = Math.sqrt(s.velX * s.velX + s.velY * s.velY)
      if (speed > 50) {
        spawnCube(w, px, py, s.velX * 0.3, s.velY * 0.3)
        spawnCooldownRef.current = 0.15
      }
    }

    // Explosions on click in physics mode
    if (mode === "physics" && s.justDown) {
      // Will trigger explosion on release based on velocity
    }

    removeDeadObjects(w)
    resetJust()
  }

  // Tick the update every frame via interval tied to engine
  useEffect(() => {
    const interval = setInterval(() => {
      if (updateRef.current) updateRef.current()
    }, 16) // ~60fps update
    return () => clearInterval(interval)
  }, [])

  // Right-click clears drawings, prevents browser menu
  useEffect(() => {
    const canvas = canvasRef.current
    if (!canvas) return
    function onContextMenu(e) {
      e.preventDefault()
      const w = getWorld()
      w.drawSegments = []
    }
    canvas.addEventListener("contextmenu", onContextMenu)
    return () => canvas.removeEventListener("contextmenu", onContextMenu)
  }, [getWorld])

  // F11 fullscreen toggle + fractal auto-zoom
  useEffect(() => {
    function onKey(e) {
      if (e.key === "F11") {
        e.preventDefault()
        if (document.fullscreenElement) {
          document.exitFullscreen()
        } else {
          document.documentElement.requestFullscreen()
        }
      }
      if ((e.key === "a" || e.key === "A") && mode === "fractal") {
        const w = getWorld()
        toggleAutoZoom(w)
      }
      if (e.key === "r" && mode === "fractal") {
        const w = getWorld()
        resetFractalView(w)
      }
      if (e.key === " " && mode === "life") {
        e.preventDefault()
        const w = getWorld()
        if (w.gol) w.gol.running = !w.gol.running
      }
    }
    window.addEventListener("keydown", onKey)
    return () => window.removeEventListener("keydown", onKey)
  }, [mode, getWorld])

  // Handle mode changes
  const handleSetMode = useCallback((newMode) => {
    const w = getWorld()
    if (newMode === "fractal") {
      w.drawSegments = []
      clearPhysics(w)
      clearBlackHoles(w)
      clearCreatures(w)
      clearTextAnomalies(w)
      w.particles = []
      w.sparks = []
      initFractal(w)
      return
    } else if (newMode === "life") {
      w.drawSegments = []
      clearPhysics(w)
      clearBlackHoles(w)
      clearCreatures(w)
      clearTextAnomalies(w)
      w.particles = []
      w.sparks = []
      w.gol = null
      initGameOfLife(w)
      return
    }
    // Exit special modes
    w.fractal = null
    w.gol = null
    setMode(newMode)
  }, [getWorld])

  // Randomize
  const onRandomize = useCallback(() => {
    const w = getWorld()
    w.bgHue = Math.random() * 360
    w.chaos = 0.1 + Math.random() * 0.8
    w.energy = 0.2 + Math.random() * 0.8
  }, [getWorld])

  // Clear
  const onClear = useCallback(() => {
    const w = getWorld()
    w.drawSegments = []
    clearPhysics(w)
    clearBlackHoles(w)
    clearCreatures(w)
    clearTextAnomalies(w)
    w.particles = []
    w.sparks = []
    if (w.fractal) resetFractalView(w)
  }, [getWorld])

  return (
    <Layout>
      <div className="dreamToyPage">
        <Rail mode={mode} setMode={handleSetMode} onRandomize={onRandomize} onClear={onClear}
              muted={muted} setMuted={setMuted} />
        <button className="dreamBackBtn" onClick={() => navigate("/games")} title="Back to games" aria-label="Back to games">
          ←
        </button>
        <canvas ref={canvasRef} className="dreamCanvas" />
        <DebugMetrics metrics={metrics} />
        {mode === "fractal" && (
          <div className="dreamFractalInfo">
            <div>{getZoomDisplay(getWorld().fractal)}</div>
            <div>iter {getWorld().fractal?.zoomState?.maxIter || 0}</div>
            <div>rebase #{getRebaseCount(getWorld().fractal)}</div>
            <div>{getLastRenderMs(getWorld().fractal).toFixed(0)}ms</div>
          </div>
        )}
        {mode === "life" && (
          <div className="dreamFractalInfo">
            <div>gen {getWorld().gol?.generation || 0}</div>
            <div>{getWorld().gol?.running ? "RUNNING" : "PAUSED"}</div>
            <div>{getWorld().gol?.cols}×{getWorld().gol?.rows}</div>
          </div>
        )}
        {showHint && mode === "life" && <div className="dreamHint">Click to toggle cells · Space to run/stop · F11 fullscreen</div>}
        {showHint && mode === "fractal" && <div className="dreamHint">Click to zoom 10× · A auto-zoom · R reset · F11 fullscreen</div>}
        {showHint && mode !== "life" && mode !== "fractal" && <div className="dreamHint">Click, drag, throw, draw, or move your mouse · F11 fullscreen</div>}
      </div>
    </Layout>
  )
}
