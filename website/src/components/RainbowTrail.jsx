import { useEffect, useRef } from "react"

export default function RainbowTrail({ children, count = 30, as: Tag = "div", style, ...props }) {
  const containerRef = useRef(null)
  const ghostRefs = useRef([])
  const historyRef = useRef([])

  useEffect(() => {
    const container = containerRef.current
    if (!container) return

    const history = historyRef.current
    let raf

    function frame() {
      const main = container.lastElementChild
      if (main) {
        const transform = main.style.transform
        const colorEl = main.querySelector(".mainLogo, h1, span")
        const color = colorEl ? colorEl.style.color : ""
        history.push({ transform, color })
        if (history.length > count * 8) {
          history.splice(0, history.length - count * 8)
        }
      }

      for (let i = 0; i < count; i++) {
        const ghost = ghostRefs.current[i]
        if (!ghost) continue
        const idx = history.length - 1 - (i + 1) * 2
        const entry = history[idx]
        if (entry) {
          ghost.style.transform = entry.transform || "none"
          ghost.style.color = entry.color || "inherit"
          const progress = (i + 1) / count
          ghost.style.opacity = Math.max(0, 1 - progress * 1.1)
        }
      }

      raf = requestAnimationFrame(frame)
    }

    raf = requestAnimationFrame(frame)
    return () => cancelAnimationFrame(raf)
  }, [count])

  return (
    <Tag
      ref={containerRef}
      {...props}
      style={{ position: "relative", display: "inline-block", ...style }}
    >
      {Array.from({ length: count }, (_, i) => (
        <div
          key={i}
          ref={(el) => (ghostRefs.current[i] = el)}
          className="mainLogo"
          style={{
            position: "absolute",
            top: 0,
            left: 0,
            width: "100%",
            height: "100%",
            pointerEvents: "none",
            zIndex: -1 - i,
            opacity: 0,
            willChange: "transform, color, opacity",
            display: "flex",
            alignItems: "center",
            justifyContent: "center",
          }}
        >
          MiMITA
        </div>
      ))}
      {children}
    </Tag>
  )
}
