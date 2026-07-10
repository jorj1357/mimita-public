import { useRef, useEffect, useCallback } from "react"

export function useDreamInput(canvasRef) {
  const state = useRef({
    pointerX: 0, pointerY: 0,
    normX: 0, normY: 0,
    down: false,
    justDown: false,
    justUp: false,
    history: [],
    velX: 0, velY: 0,
    inside: false
  })

  const updateNorm = useCallback((x, y, w, h) => {
    const s = state.current
    s.pointerX = x
    s.pointerY = y
    s.normX = w > 0 ? x / w : 0
    s.normY = h > 0 ? y / h : 0
  }, [])

  useEffect(() => {
    const el = canvasRef.current
    if (!el) return

    const onDown = (e) => {
      const s = state.current
      s.down = true
      s.justDown = true
      s.history = []
      const rect = el.getBoundingClientRect()
      updateNorm(e.clientX - rect.left, e.clientY - rect.top, rect.width, rect.height)
    }

    const onMove = (e) => {
      const s = state.current
      const rect = el.getBoundingClientRect()
      const px = e.clientX - rect.left
      const py = e.clientY - rect.top
      if (s.history.length > 0) {
        const last = s.history[s.history.length - 1]
        s.velX = px - last.x
        s.velY = py - last.y
      }
      s.history.push({ x: px, y: py, t: performance.now() })
      if (s.history.length > 10) s.history.shift()
      updateNorm(px, py, rect.width, rect.height)
      s.inside = true
    }

    const onUp = (e) => {
      const s = state.current
      s.down = false
      s.justUp = true
      // Compute release velocity from last 100ms of history
      const now = performance.now()
      const recent = s.history.filter(h => now - h.t < 100)
      if (recent.length >= 2) {
        const first = recent[0]
        const last = recent[recent.length - 1]
        const dt = (last.t - first.t) / 1000
        if (dt > 0) {
          s.velX = (last.x - first.x) / dt
          s.velY = (last.y - first.y) / dt
        }
      }
    }

    const onLeave = () => { state.current.inside = false; state.current.down = false }

    el.addEventListener("pointerdown", onDown)
    el.addEventListener("pointermove", onMove)
    el.addEventListener("pointerup", onUp)
    el.addEventListener("pointerleave", onLeave)
    el.addEventListener("pointercancel", onLeave)

    return () => {
      el.removeEventListener("pointerdown", onDown)
      el.removeEventListener("pointermove", onMove)
      el.removeEventListener("pointerup", onUp)
      el.removeEventListener("pointerleave", onLeave)
      el.removeEventListener("pointercancel", onLeave)
    }
  }, [canvasRef, updateNorm])

  const resetJust = useCallback(() => {
    state.current.justDown = false
    state.current.justUp = false
  }, [])

  return { state, resetJust }
}
