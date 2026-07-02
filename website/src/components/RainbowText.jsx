import { useEffect, useRef } from "react"

export default function RainbowText({ children, as: Tag = "span", style, ...props }) {
  const ref = useRef(null)

  useEffect(() => {
    const el = ref.current
    if (!el) return
    let start = null
    function frame(t) {
      if (!start) start = t
      const progress = ((t - start) / 5000) % 1
      el.style.color = `hsl(${Math.floor(progress * 360)}, 100%, 60%)`
      raf = requestAnimationFrame(frame)
    }
    let raf = requestAnimationFrame(frame)
    return () => cancelAnimationFrame(raf)
  }, [])

  return (
    <Tag ref={ref} {...props} style={{ transition: "none", ...style }}>
      {children}
    </Tag>
  )
}
