import { useRef } from "react"

export default function ProceduralOffset({ children, max = 3, style, ...props }) {
  const dx = useRef((Math.random() - 0.5) * 2 * max)
  const dy = useRef((Math.random() - 0.5) * 2 * max)

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
