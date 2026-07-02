import { useRef } from "react"

function seededRandom(seed) {
  let s = seed
  return function () {
    s = (s * 16807) % 2147483647
    return (s - 1) / 2147483646
  }
}

const rand = seededRandom(Date.now())

const COLORS = [
  "#00ff41",
  "#ff0044",
  "#aa3bff",
  "#00ffff",
  "#ffff00",
  "#ff6600",
  "#ff00ff",
  "#66ff00",
]

export default function GrossHeading({ children, as: Tag = "h2", style, ...props }) {
  const color = useRef(COLORS[Math.floor(rand() * COLORS.length)])
  const borderColor = useRef(COLORS[Math.floor(rand() * COLORS.length)])
  const pad = useRef(Math.floor(rand() * 8) + 4)

  return (
    <Tag
      {...props}
      style={{
        color: color.current,
        borderBottom: `2px solid ${borderColor.current}`,
        paddingBottom: `${pad.current}px`,
        display: "inline-block",
        ...style,
      }}
    >
      {children}
    </Tag>
  )
}
