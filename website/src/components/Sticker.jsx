import { useRef } from "react"

function seededRandom(seed) {
  let s = seed
  return function () {
    s = (s * 16807) % 2147483647
    return (s - 1) / 2147483646
  }
}

const rand = seededRandom(Date.now())

const STICKERS = [
  { text: "★", size: 24 },
  { text: "✦", size: 20 },
  { text: "✧", size: 22 },
  { text: "⚡", size: 18 },
  { text: "🔥", size: 20 },
  { text: "💀", size: 18 },
  { text: "🎮", size: 18 },
  { text: "👾", size: 20 },
  { text: "✨", size: 18 },
  { text: "❓", size: 20 },
  { text: "⚠", size: 20 },
  { text: "⌨", size: 18 },
  { text: "🖱", size: 18 },
]

export default function Sticker({ index = 0, style, ...props }) {
  const sticker = STICKERS[index % STICKERS.length]
  const x = useRef(rand() * 80 + 10)
  const y = useRef(rand() * 80 + 10)
  const rot = useRef((rand() - 0.5) * 60)
  const hue = useRef(Math.floor(rand() * 360))

  return (
    <span
      {...props}
      style={{
        position: "absolute",
        left: `${x.current}%`,
        top: `${y.current}%`,
        transform: `rotate(${rot.current}deg)`,
        fontSize: `${sticker.size}px`,
        color: `hsl(${hue.current}, 90%, 60%)`,
        opacity: 0.5,
        pointerEvents: "none",
        userSelect: "none",
        zIndex: 0,
        ...style,
      }}
    >
      {sticker.text}
    </span>
  )
}
