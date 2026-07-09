import { useEffect, useRef } from "react"

export default function ChaosSway({ children, as: Tag = "div", style, ...props }) {
  const ref = useRef(null)

  useEffect(() => {
    const el = ref.current
    if (!el) return

    const seed = Date.now()
    function seededRand(s) {
      s = (s * 16807) % 2147483647
      return (s - 1) / 2147483646
    }

    const ox = (seededRand(seed + 1) - 0.5) * 3
    const oy = (seededRand(seed + 2) - 0.5) * 3
    const r0 = (seededRand(seed + 3) - 0.5) * 6
    const s0 = 0.92 + seededRand(seed + 4) * 0.16

    const freqX = (0.4 + seededRand(seed + 5) * 0.3) / 25
    const freqY = (0.5 + seededRand(seed + 6) * 0.3) / 25
    const freqR = (0.3 + seededRand(seed + 7) * 0.2) / 25
    const freqSX = (0.6 + seededRand(seed + 8) * 0.2) / 25
    const freqSY = (0.7 + seededRand(seed + 9) * 0.2) / 25

    const ampX = 140 + seededRand(seed + 10) * 60
    const ampY = 100 + seededRand(seed + 11) * 50
    const ampR = 1.5 + seededRand(seed + 12) * 2.5
    const ampSX = 0.04 + seededRand(seed + 13) * 0.06
    const ampSY = 0.04 + seededRand(seed + 14) * 0.06

    let start = null
    let raf

    function frame(t) {
      if (!start) start = t
      const sec = (t - start) / 1000

      const x = ox + Math.sin(sec * freqX * Math.PI * 2) * ampX
      const y = oy + Math.sin(sec * freqY * Math.PI * 2) * ampY
      const r = r0 + Math.sin(sec * freqR * Math.PI * 2) * ampR
      const sx = s0 + Math.sin(sec * freqSX * Math.PI * 2) * ampSX
      const sy = s0 + Math.sin(sec * freqSY * Math.PI * 2) * ampSY

      el.style.transform = `translate(${x}px, ${y}px) rotate(${r}deg) scale(${sx}, ${sy})`

      raf = requestAnimationFrame(frame)
    }

    raf = requestAnimationFrame(frame)
    return () => cancelAnimationFrame(raf)
  }, [])

  return (
    <Tag
      ref={ref}
      {...props}
      style={{ display: "inline-block", willChange: "transform", ...style }}
    >
      {children}
    </Tag>
  )
}
