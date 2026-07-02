import { useRef } from "react"
import { getControlledChaos } from "../lib/controlled-chaos"

export default function ChaosBox({ seed, children, as: Tag = "div", style, ...props }) {
  const chaos = useRef(getControlledChaos(seed))
  const t = chaos.current

  return (
    <Tag
      {...props}
      style={{
        transform: `translate(${t.xOffset}px, ${t.yOffset}px) rotate(${t.rotation}deg) scale(${t.scale})`,
        ...style,
      }}
    >
      {children}
    </Tag>
  )
}
