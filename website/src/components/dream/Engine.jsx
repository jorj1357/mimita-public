import { useRef, useEffect, useCallback, useState } from "react"
import { renderFractal, tickFractal } from "./Fractal"
import { renderGameOfLife, tickGameOfLife } from "./GameOfLife"

const MAX_PARTICLES = 300
const MAX_PHYSICS_OBJECTS = 150
const MAX_CREATURES = 30
const MAX_TEXT_ANOMALIES = 20
const MAX_BLACK_HOLES = 3
const MAX_DRAW_POINTS = 50000

export function createWorld() {
  return {
    tick: 0,
    time: 0,
    width: 800,
    height: 600,
    dpr: 1,

    particles: [],
    physicsObjects: [],
    creatures: [],
    textAnomalies: [],
    blackHoles: [],
    drawSegments: [],
    sparks: [],
    explosions: [],

    bgHue: 0,
    palette: [0.6, 0.8, 1.0],
    mode: "dream",
    gravity: { x: 0, y: 500 },
    symmetry: 0,
    phase: 0,
    chaos: 0.3,
    energy: 0.5,

    rng() { return Math.random() },
    seed: 0
  }
}

export function useDreamEngine(canvasRef) {
  const worldRef = useRef(createWorld())
  const rafRef = useRef(null)
  const lastTimeRef = useRef(0)
  const [metrics, setMetrics] = useState({ fps: 0, objects: 0, particles: 0, creatures: 0, draws: 0 })
  const frameCountRef = useRef(0)
  const fpsTimeRef = useRef(0)

  const getWorld = useCallback(() => worldRef.current, [])

  useEffect(() => {
    const canvas = canvasRef.current
    if (!canvas) return
    const ctx = canvas.getContext("2d")
    const w = worldRef.current

    function resize() {
      const rect = canvas.parentElement.getBoundingClientRect()
      w.dpr = Math.min(window.devicePixelRatio || 1, 2)
      canvas.width = rect.width * w.dpr
      canvas.height = rect.height * w.dpr
      canvas.style.width = rect.width + "px"
      canvas.style.height = rect.height + "px"
      w.width = rect.width
      w.height = rect.height
    }

    resize()
    window.addEventListener("resize", resize)

    function tick(time) {
      if (!lastTimeRef.current) lastTimeRef.current = time
      let dt = (time - lastTimeRef.current) / 1000
      lastTimeRef.current = time
      dt = Math.min(dt, 0.05) // clamp to 50ms

      // FPS
      frameCountRef.current++
      if (time - fpsTimeRef.current >= 1000) {
        setMetrics({
          fps: frameCountRef.current,
          objects: w.physicsObjects.length,
          particles: w.particles.length,
          creatures: w.creatures.length,
          draws: w.drawSegments.length
        })
        frameCountRef.current = 0
        fpsTimeRef.current = time
      }

      // Update
      w.tick++
      w.time += dt
      w.bgHue = (w.bgHue + dt * 0.5) % 360

      // Fractal mode: render fractal instead of normal scene
      if (w.mode === "fractal" && w.fractal) {
        renderFractal(ctx, w)
        rafRef.current = requestAnimationFrame(tick)
        return
      }

      // Game of Life mode
      if (w.mode === "life" && w.gol) {
        renderGameOfLife(ctx, w)
        rafRef.current = requestAnimationFrame(tick)
        return
      }

      // Clear
      ctx.fillStyle = `hsl(${w.bgHue}, 30%, 3%)`
      ctx.fillRect(0, 0, canvas.width, canvas.height)

      // Draw particles
      for (let i = w.particles.length - 1; i >= 0; i--) {
        const p = w.particles[i]
        p.x += p.vx * dt
        p.y += p.vy * dt
        p.vy += 20 * dt
        p.life -= dt
        if (p.life <= 0) { w.particles.splice(i, 1); continue }
        const alpha = Math.min(1, p.life / p.maxLife)
        ctx.globalAlpha = alpha * 0.6
        ctx.fillStyle = p.color
        ctx.beginPath()
        ctx.arc(p.x * w.dpr, p.y * w.dpr, p.size * w.dpr, 0, Math.PI * 2)
        ctx.fill()
      }
      ctx.globalAlpha = 1

      // Object-object collision
      for (let i = 0; i < w.physicsObjects.length; i++) {
        for (let j = i + 1; j < w.physicsObjects.length; j++) {
          const a = w.physicsObjects[i]
          const b = w.physicsObjects[j]
          const dx = a.x - b.x
          const dy = a.y - b.y
          const dist = Math.sqrt(dx * dx + dy * dy)
          const minDist = (a.width + b.width) / 2
          if (dist < minDist && dist > 0.01) {
            const overlap = minDist - dist
            const nx = dx / dist
            const ny = dy / dist
            const totalMass = 2
            a.x += nx * overlap * 0.5
            a.y += ny * overlap * 0.5
            b.x -= nx * overlap * 0.5
            b.y -= ny * overlap * 0.5
            const relVx = a.vx - b.vx
            const relVy = a.vy - b.vy
            const relVn = relVx * nx + relVy * ny
            if (relVn < 0) {
              const impulse = relVn * (a.restitution + b.restitution) * 0.5
              a.vx -= nx * impulse
              a.vy -= ny * impulse
              b.vx += nx * impulse
              b.vy += ny * impulse
              // Sparks on collision
              const hitSpeed = Math.abs(relVn)
              if (hitSpeed > 30) {
                const sparkCount = Math.min(Math.floor(hitSpeed / 10), 8)
                for (let s = 0; s < sparkCount; s++) {
                  const angle = Math.atan2(ny, nx) + (Math.random() - 0.5) * Math.PI
                  const speed = 20 + Math.random() * hitSpeed * 0.5
                  w.sparks.push({
                    x: (a.x + b.x) / 2,
                    y: (a.y + b.y) / 2,
                    vx: Math.cos(angle) * speed,
                    vy: Math.sin(angle) * speed,
                    size: 1 + Math.random() * 3,
                    color: `hsl(${Math.random() * 360}, 100%, 70%)`,
                    life: 0.2 + Math.random() * 0.3,
                    maxLife: 0.5
                  })
                }
              }
            }
          }
        }
      }

      // Draw sparks
      for (let i = w.sparks.length - 1; i >= 0; i--) {
        const s = w.sparks[i]
        s.x += s.vx * dt
        s.y += s.vy * dt
        s.vy += 50 * dt
        s.life -= dt
        if (s.life <= 0) { w.sparks.splice(i, 1); continue }
        const alpha = Math.min(1, s.life / s.maxLife)
        ctx.globalAlpha = alpha
        ctx.fillStyle = s.color
        ctx.beginPath()
        ctx.arc(s.x * w.dpr, s.y * w.dpr, s.size * w.dpr, 0, Math.PI * 2)
        ctx.fill()
      }
      ctx.globalAlpha = 1

      // Draw physics objects
      for (const obj of w.physicsObjects) {
        obj.x += obj.vx * dt
        obj.y += obj.vy * dt
        obj.vy += w.gravity.y * dt / 1000
        obj.rotation += obj.angularVel * dt

        // Wall bounce
        const hw = obj.width / 2
        const hh = obj.height / 2
        if (obj.x - hw < 0) { obj.x = hw; obj.vx *= -obj.restitution }
        if (obj.x + hw > w.width) { obj.x = w.width - hw; obj.vx *= -obj.restitution }
        if (obj.y - hh < 0) { obj.y = hh; obj.vy *= -obj.restitution }
        if (obj.y + hh > w.height) { obj.y = w.height - hh; obj.vy *= -obj.restitution; obj.onGround = true }
        else obj.onGround = false

        ctx.save()
        ctx.translate(obj.x * w.dpr, obj.y * w.dpr)
        ctx.rotate(obj.rotation)
        ctx.globalAlpha = obj.alpha || 1
        ctx.fillStyle = obj.color
        if (obj.shape === "circle") {
          ctx.beginPath()
          ctx.arc(0, 0, obj.width / 2 * w.dpr, 0, Math.PI * 2)
          ctx.fill()
        } else {
          ctx.fillRect(-hw * w.dpr, -hh * w.dpr, obj.width * w.dpr, obj.height * w.dpr)
        }
        ctx.globalAlpha = 1
        ctx.restore()
      }

      // Draw black holes
      for (const bh of w.blackHoles) {
        const r = bh.radius * w.dpr
        const grad = ctx.createRadialGradient(bh.x * w.dpr, bh.y * w.dpr, 0, bh.x * w.dpr, bh.y * w.dpr, r)
        grad.addColorStop(0, "rgba(0,0,0,1)")
        grad.addColorStop(0.7, "rgba(80,0,160,0.6)")
        grad.addColorStop(1, "rgba(0,0,0,0)")
        ctx.fillStyle = grad
        ctx.beginPath()
        ctx.arc(bh.x * w.dpr, bh.y * w.dpr, r, 0, Math.PI * 2)
        ctx.fill()
        // Accretion ring
        ctx.strokeStyle = `hsla(${(w.time * 50 + bh.x * 10) % 360}, 100%, 60%, 0.5)`
        ctx.lineWidth = 2 * w.dpr
        ctx.beginPath()
        ctx.arc(bh.x * w.dpr, bh.y * w.dpr, r * 0.7, 0, Math.PI * 2)
        ctx.stroke()
      }

      // Draw creatures
      for (const c of w.creatures) {
        c.x += c.vx * dt
        c.y += c.vy * dt
        c.vy += 100 * dt
        if (c.x < 5) c.x = 5
        if (c.x > w.width - 5) c.x = w.width - 5
        if (c.y < 5) c.y = 5
        if (c.y > w.height - 5) { c.y = w.height - 5; c.vy = 0 }
        ctx.fillStyle = c.color
        ctx.beginPath()
        ctx.arc(c.x * w.dpr, c.y * w.dpr, 4 * w.dpr, 0, Math.PI * 2)
        ctx.fill()
        // Eyes
        ctx.fillStyle = "white"
        ctx.beginPath()
        ctx.arc((c.x - 1.5) * w.dpr, (c.y - 1) * w.dpr, 1.5 * w.dpr, 0, Math.PI * 2)
        ctx.arc((c.x + 1.5) * w.dpr, (c.y - 1) * w.dpr, 1.5 * w.dpr, 0, Math.PI * 2)
        ctx.fill()
      }

      // Draw text anomalies
      for (let i = w.textAnomalies.length - 1; i >= 0; i--) {
        const t = w.textAnomalies[i]
        t.life -= dt
        t.y -= 10 * dt
        if (t.life <= 0) { w.textAnomalies.splice(i, 1); continue }
        const alpha = Math.min(1, t.life / t.maxLife)
        ctx.globalAlpha = alpha
        ctx.font = `${t.size * w.dpr}px "MingLiU", monospace`
        if (t.bg) {
          ctx.fillStyle = t.bg
          const m = ctx.measureText(t.text)
          ctx.fillRect((t.x - 2) * w.dpr, (t.y - t.size) * w.dpr, (m.width + 4) * w.dpr, (t.size + 4) * w.dpr)
        }
        ctx.fillStyle = t.color
        ctx.fillText(t.text, t.x * w.dpr, t.y * w.dpr)
        ctx.globalAlpha = 1
      }

      // Draw segments
      for (let i = 0; i < w.drawSegments.length; i++) {
        const seg = w.drawSegments[i]
        ctx.strokeStyle = seg.color
        ctx.lineWidth = seg.width * w.dpr
        ctx.lineCap = "round"
        ctx.beginPath()
        ctx.moveTo(seg.x1 * w.dpr, seg.y1 * w.dpr)
        ctx.lineTo(seg.x2 * w.dpr, seg.y2 * w.dpr)
        ctx.stroke()
        // Update hue for rainbow cycling
        seg.hue = (seg.hue + dt * 60) % 360
        seg.color = `hsl(${seg.hue}, 100%, 60%)`
      }

      rafRef.current = requestAnimationFrame(tick)
    }

    rafRef.current = requestAnimationFrame(tick)

    return () => {
      cancelAnimationFrame(rafRef.current)
      window.removeEventListener("resize", resize)
    }
  }, [canvasRef])

  return { getWorld, metrics }
}
