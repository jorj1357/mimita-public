import { useRef } from "react"

function seededRandom(seed) {
  let s = seed
  return function () {
    s = (s * 16807) % 2147483647
    return (s - 1) / 2147483646
  }
}

const rand = seededRandom(Date.now())

export default function ProceduralOffset({ children, max = 3, style, ...props }) {
  const dx = useRef((rand() - 0.5) * 2 * max)
  const dy = useRef((rand() - 0.5) * 2 * max)

  return (
    <div
      {...props}
      style={{
        position: "relative",
        left: `${dx.current}px`,
        top: `${dy.current}px`,
        ...style,
      }}
    >
      {children}
    </div>
  )
}
