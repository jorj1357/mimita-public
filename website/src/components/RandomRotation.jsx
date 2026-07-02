import { useRef } from "react"

function seededRandom(seed) {
  let s = seed
  return function () {
    s = (s * 16807) % 2147483647
    return (s - 1) / 2147483646
  }
}

const rand = seededRandom(Date.now())

export default function RandomRotation({ children, maxDeg = 3, style, ...props }) {
  const deg = useRef((rand() - 0.5) * 2 * maxDeg)

  return (
    <div
      {...props}
      style={{
        display: "inline-block",
        transform: `rotate(${deg.current}deg)`,
        ...style,
      }}
    >
      {children}
    </div>
  )
}
