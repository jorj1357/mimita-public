import { useEffect, useRef } from "react"

const R = 55
const GRAVITY = 0.35
const FRICTION = 0.9995
const RESTITUTION = 0.97
const FRAME_HISTORY = 5

function debugLog(...args) {
  const params = new URLSearchParams(window.location.search)
  if (params.get("debugWebsite") === "1") {
    console.log("[WebsiteDecor]", ...args)
  }
}

function docW() {
  return Math.max(
    document.documentElement.scrollWidth,
    document.body.scrollWidth,
    window.innerWidth
  )
}

function docH() {
  return Math.max(
    document.documentElement.scrollHeight,
    document.body.scrollHeight,
    window.innerHeight
  )
}

export default function BounceBall() {
  const cleanup = useRef(null)

  useEffect(() => {
    const maxX = docW() - R * 2
    const maxY = docH() - R * 2

    const pos = { x: maxX / 2, y: 0 }
    const vel = { x: 1, y: 6 }
    let drag = false
    const off = { x: 0, y: 0 }
    const hist = []
    let rafId = null
    let boundsW = maxX
    let boundsH = maxY

    debugLog("Ball Position", { x: pos.x, y: pos.y })
    debugLog("Document Bounds", { width: docW(), height: docH() })
    debugLog("Viewport", {
      scrollX: window.scrollX,
      scrollY: window.scrollY,
      width: window.innerWidth,
      height: window.innerHeight,
    })

    const container = document.createElement("div")
    container.style.cssText =
      "position:absolute;top:0;left:0;width:0;height:0;pointer-events:none"
    document.body.appendChild(container)

    const ball = document.createElement("div")
    ball.style.cssText =
      "position:absolute;" +
      `left:${pos.x}px;` +
      `top:${pos.y}px;` +
      `width:${R * 2}px;` +
      `height:${R * 2}px;` +
      "border-radius:50%;" +
      "background:radial-gradient(circle at 35% 35%, #ff4488, #aa0044);" +
      "cursor:grab;" +
      "touch-action:none;" +
      "pointer-events:auto;" +
      "z-index:99999;" +
      "box-shadow:0 0 12px rgba(255,68,136,0.5);" +
      "user-select:none;"
    container.appendChild(ball)

    function measureBounds() {
      boundsW = docW() - R * 2
      boundsH = docH() - R * 2
    }

    function tick() {
      boundsW = docW() - R * 2
      boundsH = docH() - R * 2

      if (!drag) {
        vel.y += GRAVITY
        vel.x *= FRICTION
        vel.y *= FRICTION

        pos.x += vel.x
        pos.y += vel.y

        if (pos.x < 0) {
          pos.x = 0
          vel.x = Math.abs(vel.x) * RESTITUTION
        } else if (pos.x > boundsW) {
          pos.x = boundsW
          vel.x = -Math.abs(vel.x) * RESTITUTION
        }

        if (pos.y < 0) {
          pos.y = 0
          vel.y = Math.abs(vel.y) * RESTITUTION
        } else if (pos.y > boundsH) {
          pos.y = boundsH
          vel.y = -Math.abs(vel.y) * RESTITUTION
          if (Math.abs(vel.x) < 0.02 && Math.abs(vel.y) < 0.02) {
            vel.x = 0
            vel.y = 0
          }
        }
      }

      ball.style.left = pos.x + "px"
      ball.style.top = pos.y + "px"

      rafId = requestAnimationFrame(tick)
    }

    rafId = requestAnimationFrame(tick)

    function onPointerDown(e) {
      e.preventDefault()
      drag = true
      off.x = e.clientX + window.scrollX - pos.x
      off.y = e.clientY + window.scrollY - pos.y
      hist.length = 0
      ball.style.cursor = "grabbing"
      document.body.style.cursor = "grabbing"
      ball.setPointerCapture(e.pointerId)
    }

    function onPointerMove(e) {
      if (!drag) return
      e.preventDefault()
      const dx = e.clientX + window.scrollX - off.x
      const dy = e.clientY + window.scrollY - off.y
      pos.x = Math.max(0, Math.min(boundsW, dx))
      pos.y = Math.max(0, Math.min(boundsH, dy))
      hist.push({ x: e.clientX + window.scrollX, y: e.clientY + window.scrollY, t: performance.now() })
      if (hist.length > FRAME_HISTORY) hist.shift()
    }

    function onPointerUp(e) {
      if (!drag) return
      drag = false
      document.body.style.cursor = ""
      ball.style.cursor = "grab"
      ball.releasePointerCapture(e.pointerId)

      if (hist.length >= 2) {
        const first = hist[0]
        const last = hist[hist.length - 1]
        const dt = Math.max(last.t - first.t, 16)
        vel.x = ((last.x - first.x) / dt) * 16 * 1.2
        vel.y = ((last.y - first.y) / dt) * 16 * 1.2
        const maxV = 25
        const spd = Math.sqrt(vel.x * vel.x + vel.y * vel.y)
        if (spd > maxV) {
          vel.x = (vel.x / spd) * maxV
          vel.y = (vel.y / spd) * maxV
        }
      }
    }

    function onResize() {
      measureBounds()
      pos.x = Math.min(pos.x, boundsW)
      pos.y = Math.min(pos.y, boundsH)
    }

    ball.addEventListener("pointerdown", onPointerDown)
    document.addEventListener("pointermove", onPointerMove)
    document.addEventListener("pointerup", onPointerUp)
    window.addEventListener("resize", onResize)

    cleanup.current = () => {
      if (rafId) cancelAnimationFrame(rafId)
      ball.removeEventListener("pointerdown", onPointerDown)
      document.removeEventListener("pointermove", onPointerMove)
      document.removeEventListener("pointerup", onPointerUp)
      window.removeEventListener("resize", onResize)
      container.remove()
    }
  }, [])

  useEffect(() => {
    return () => {
      if (cleanup.current) cleanup.current()
    }
  }, [])

  return null
}
