import { useRef } from "react"

export default function RandomRotation({ children, maxDeg = 3, style, ...props }) {
  const deg = useRef((Math.random() - 0.5) * 2 * maxDeg)

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
