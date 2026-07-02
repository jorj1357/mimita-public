import { useRef } from "react"

function seededRandom(seed) {
  let s = seed
  return function () {
    s = (s * 16807) % 2147483647
    return (s - 1) / 2147483646
  }
}

const rand = seededRandom(Date.now())

const BORDERS = [1, 1, 1, 2, 2, 3, 3, 4]
const ACCENTS = [
  "rgba(0,255,65,0.3)",
  "rgba(255,0,68,0.3)",
  "rgba(170,59,255,0.3)",
  "rgba(0,255,255,0.3)",
  "rgba(255,255,0,0.3)",
  "rgba(255,102,0,0.3)",
]

export default function PixelBox({ children, style, ...props }) {
  const borderW = useRef(BORDERS[Math.floor(rand() * BORDERS.length)])
  const accent = useRef(ACCENTS[Math.floor(rand() * ACCENTS.length)])

  return (
    <div
      {...props}
      style={{
        border: `${borderW.current}px solid ${accent.current}`,
        padding: "1rem",
        ...style,
      }}
    >
      {children}
    </div>
  )
}
