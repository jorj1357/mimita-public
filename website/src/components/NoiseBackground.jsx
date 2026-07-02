import { useRef, useEffect } from "react"

export default function NoiseBackground({ opacity = 0.03, style, children, ...props }) {
  const canvasRef = useRef(null)

  useEffect(() => {
    const c = canvasRef.current
    if (!c) return
    const ctx = c.getContext("2d")
    const w = (c.width = 256)
    const h = (c.height = 256)
    const img = ctx.createImageData(w, h)
    for (let i = 0; i < img.data.length; i += 4) {
      const v = Math.floor(Math.random() * 256)
      img.data[i] = v
      img.data[i + 1] = v
      img.data[i + 2] = v
      img.data[i + 3] = 255
    }
    ctx.putImageData(img, 0, 0)
  }, [])

  return (
    <div {...props} style={{ position: "relative", overflow: "hidden", ...style }}>
      <canvas
        ref={canvasRef}
        style={{
          position: "absolute",
          inset: 0,
          width: "100%",
          height: "100%",
          opacity,
          pointerEvents: "none",
          imageRendering: "pixelated",
          zIndex: 0,
        }}
      />
      <div style={{ position: "relative", zIndex: 1 }}>
        {children}
      </div>
    </div>
  )
}
