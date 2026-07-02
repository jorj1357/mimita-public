import { useRef, useEffect } from "react"
import { createPortal } from "react-dom"

const R = 18
const GRAVITY = 0.35
const FRICTION = 0.992
const RESTITUTION = 0.88
const FRAME_HISTORY = 5

function debugLog(...args) {
  const params = new URLSearchParams(window.location.search)
  if (params.get("debugWebsite") === "1") {
    console.log("[WebsiteDecor]", ...args)
  }
}

export default function BounceBall() {
  const el = useRef(null)
  const pos = useRef({ x: 0, y: 0 })
  const vel = useRef({ x: 1, y: 6 })
  const drag = useRef(false)
  const off = useRef({ x: 0, y: 0 })
  const hist = useRef([])
  const raf = useRef(null)
  const bounds = useRef({ w: 0, h: 0 })

  useEffect(() => {
    function updateBounds() {
      bounds.current.w = window.innerWidth - R * 2
      bounds.current.h = window.innerHeight - R * 2
    }

    updateBounds()
    pos.current.x = bounds.current.w / 2
    pos.current.y = R

    debugLog("Ball bounds:", {
      left: 0,
      right: bounds.current.w + R * 2,
      top: 0,
      bottom: bounds.current.h + R * 2,
      ballRadius: R,
    })

    function tick() {
      if (!drag.current) {
        vel.current.y += GRAVITY
        vel.current.x *= FRICTION
        vel.current.y *= FRICTION

        pos.current.x += vel.current.x
        pos.current.y += vel.current.y

        const { w: maxX, h: maxY } = bounds.current

        if (pos.current.x < 0) {
          pos.current.x = 0
          vel.current.x = Math.abs(vel.current.x) * RESTITUTION
        } else if (pos.current.x > maxX) {
          pos.current.x = maxX
          vel.current.x = -Math.abs(vel.current.x) * RESTITUTION
        }

        if (pos.current.y < 0) {
          pos.current.y = 0
          vel.current.y = Math.abs(vel.current.y) * RESTITUTION
        } else if (pos.current.y > maxY) {
          pos.current.y = maxY
          vel.current.y = -Math.abs(vel.current.y) * RESTITUTION
          if (Math.abs(vel.current.x) < 0.3 && Math.abs(vel.current.y) < 0.3) {
            vel.current.x = 0
            vel.current.y = 0
          }
        }
      }

      if (el.current) {
        el.current.style.transform = `translate(${pos.current.x}px, ${pos.current.y}px)`
      }

      raf.current = requestAnimationFrame(tick)
    }

    raf.current = requestAnimationFrame(tick)

    function onPointerDown(e) {
      e.preventDefault()
      drag.current = true
      off.current.x = e.clientX - pos.current.x
      off.current.y = e.clientY - pos.current.y
      hist.current = []
      if (el.current) {
        el.current.style.cursor = "grabbing"
        el.current.setPointerCapture(e.pointerId)
      }
      document.body.style.cursor = "grabbing"
    }

    function onPointerMove(e) {
      if (!drag.current) return
      e.preventDefault()
      const { w: maxX, h: maxY } = bounds.current
      pos.current.x = Math.max(0, Math.min(maxX, e.clientX - off.current.x))
      pos.current.y = Math.max(0, Math.min(maxY, e.clientY - off.current.y))
      hist.current.push({ x: e.clientX, y: e.clientY, t: performance.now() })
      if (hist.current.length > FRAME_HISTORY) hist.current.shift()
    }

    function onPointerUp(e) {
      if (!drag.current) return
      drag.current = false
      document.body.style.cursor = ""
      if (el.current) {
        el.current.style.cursor = "grab"
        el.current.releasePointerCapture(e.pointerId)
      }

      if (hist.current.length >= 2) {
        const first = hist.current[0]
        const last = hist.current[hist.current.length - 1]
        const dt = Math.max(last.t - first.t, 16)
        vel.current.x = ((last.x - first.x) / dt) * 16 * 1.2
        vel.current.y = ((last.y - first.y) / dt) * 16 * 1.2

        const maxV = 25
        const spd = Math.sqrt(vel.current.x * vel.current.x + vel.current.y * vel.current.y)
        if (spd > maxV) {
          vel.current.x = (vel.current.x / spd) * maxV
          vel.current.y = (vel.current.y / spd) * maxV
        }
      }
    }

    function onResizeOrScroll() {
      updateBounds()
      const { w: maxX, h: maxY } = bounds.current
      pos.current.x = Math.min(pos.current.x, maxX)
      pos.current.y = Math.min(pos.current.y, maxY)
    }

    const ball = el.current
    if (ball) {
      ball.addEventListener("pointerdown", onPointerDown)
    }
    document.addEventListener("pointermove", onPointerMove)
    document.addEventListener("pointerup", onPointerUp)
    window.addEventListener("resize", onResizeOrScroll)
    window.addEventListener("scroll", onResizeOrScroll)

    return () => {
      if (raf.current) cancelAnimationFrame(raf.current)
      if (ball) {
        ball.removeEventListener("pointerdown", onPointerDown)
      }
      document.removeEventListener("pointermove", onPointerMove)
      document.removeEventListener("pointerup", onPointerUp)
      window.removeEventListener("resize", onResizeOrScroll)
      window.removeEventListener("scroll", onResizeOrScroll)
    }
  }, [])

  const ball = (
    <div
      ref={el}
      style={{
        position: "fixed",
        left: 0,
        top: 0,
        width: R * 2,
        height: R * 2,
        borderRadius: "50%",
        background: "radial-gradient(circle at 35% 35%, #ff4488, #aa0044)",
        cursor: "grab",
        touchAction: "none",
        zIndex: 99999,
        pointerEvents: "auto",
        willChange: "transform",
        boxShadow: "0 0 12px rgba(255,68,136,0.5)",
        userSelect: "none",
      }}
    />
  )

  return createPortal(ball, document.body)
}
