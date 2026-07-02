import { useRef, useEffect, useState } from "react"
import { createPortal } from "react-dom"
import Sticker from "./Sticker"

function seededRandom(seed) {
  let s = seed
  return function () {
    s = (s * 16807) % 2147483647
    return (s - 1) / 2147483646
  }
}

function debugLog(...args) {
  const params = new URLSearchParams(window.location.search)
  if (params.get("debugWebsite") === "1") {
    console.log("[WebsiteDecor]", ...args)
  }
}

export default function StickerLayer({ count = 6 }) {
  const [ready, setReady] = useState(false)
  const positions = useRef([])

  useEffect(() => {
    const dh = Math.max(
      document.documentElement.scrollHeight,
      document.body.scrollHeight,
      window.innerHeight
    )
    const dw = Math.max(
      document.documentElement.scrollWidth,
      document.body.scrollWidth,
      window.innerWidth
    )

    const rand = seededRandom(Date.now() + Math.random())
    const items = []
    for (let i = 0; i < count; i++) {
      items.push({
        id: i,
        x: rand() * Math.max(dw - 100, 200) + 24,
        y: rand() * Math.max(dh - 100, 200) + 24,
      })
    }
    positions.current = items

    debugLog("Generated stickers:", items.map(s => ({
      id: s.id, x: Math.round(s.x), y: Math.round(s.y), rotation: `${((rand() - 0.5) * 60).toFixed(1)}deg`
    })))

    setReady(true)
  }, [count])

  if (!ready) return null

  const container = (
    <div
      style={{
        position: "absolute",
        top: 0,
        left: 0,
        width: "100%",
        height: Math.max(
          document.documentElement.scrollHeight,
          document.body.scrollHeight,
          window.innerHeight
        ),
        pointerEvents: "none",
        zIndex: 0,
        overflow: "hidden",
      }}
    >
      {positions.current.map((p) => (
        <span
          key={p.id}
          style={{
            position: "absolute",
            left: `${p.x}px`,
            top: `${p.y}px`,
          }}
        >
          <Sticker />
        </span>
      ))}
    </div>
  )

  return createPortal(container, document.body)
}
