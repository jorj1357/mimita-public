import { useState, useRef, useCallback, useEffect } from "react"

export default function AvatarCropModal({ file, onSave, onClose }) {
    const [image, setImage] = useState(null)
    const [zoom, setZoom] = useState(1)
    const [pan, setPan] = useState({ x: 0, y: 0 })
    const [dragging, setDragging] = useState(false)
    const [dragStart, setDragStart] = useState({ x: 0, y: 0 })
    const [panStart, setPanStart] = useState({ x: 0, y: 0 })
    const imgRef = useRef(null)
    const containerRef = useRef(null)
    const previewRef = useRef(null)

    useEffect(() => {
        const url = URL.createObjectURL(file)
        const img = new Image()
        img.onload = () => {
            setImage(img)
            URL.revokeObjectURL(url)
        }
        img.src = url
        return () => URL.revokeObjectURL(url)
    }, [file])

    const drawPreview = useCallback(() => {
        if (!image || !previewRef.current) return
        const canvas = previewRef.current
        const ctx = canvas.getContext("2d")
        const size = 512
        canvas.width = size
        canvas.height = size

        const container = containerRef.current
        if (!container) return
        const cw = container.clientWidth
        const ch = container.clientHeight
        const s = Math.min(cw, ch)

        const scale = (s * zoom) / Math.min(image.width, image.height)
        const iw = image.width * scale
        const ih = image.height * scale
        const ix = (s - iw) / 2 + pan.x
        const iy = (s - ih) / 2 + pan.y
        const cropSize = s

        const sx = Math.max(0, -ix)
        const sy = Math.max(0, -iy)
        const sw = Math.min(cropSize, iw - Math.max(0, ix))
        const sh = Math.min(cropSize, ih - Math.max(0, iy))

        if (sw <= 0 || sh <= 0) return

        const srcX = (sx / scale) + Math.max(0, (image.width - iw / scale) / 2)
        const srcY = (sy / scale) + Math.max(0, (image.height - ih / scale) / 2)
        const srcW = sw / scale
        const srcH = sh / scale

        ctx.drawImage(image, srcX, srcY, srcW, srcH, 0, 0, size, size)
    }, [image, zoom, pan])

    useEffect(() => {
        drawPreview()
    }, [drawPreview])

    function handleWheel(e) {
        e.preventDefault()
        const delta = e.deltaY > 0 ? -0.1 : 0.1
        setZoom(z => Math.max(0.5, Math.min(5, z + delta)))
    }

    function handleMouseDown(e) {
        setDragging(true)
        setDragStart({ x: e.clientX, y: e.clientY })
        setPanStart({ x: pan.x, y: pan.y })
    }

    function handleMouseMove(e) {
        if (!dragging) return
        const dx = e.clientX - dragStart.x
        const dy = e.clientY - dragStart.y
        setPan({ x: panStart.x + dx, y: panStart.y + dy })
    }

    function handleMouseUp() {
        setDragging(false)
    }

    function handleSave() {
        const canvas = previewRef.current
        if (!canvas) return
        canvas.toBlob(blob => {
            if (blob) onSave(blob)
        }, "image/png")
    }

    const containerStyle = image
        ? {
            width: "min(60vw, 400px)",
            height: "min(60vw, 400px)",
            overflow: "hidden",
            position: "relative",
            background: "#111",
            borderRadius: "8px",
            cursor: dragging ? "grabbing" : "grab",
            margin: "0 auto"
        }
        : {}

    const imgStyle = image
        ? {
            position: "absolute",
            left: "50%",
            top: "50%",
            width: `${zoom * 100}%`,
            height: `${zoom * 100}%`,
            objectFit: "cover",
            transform: `translate(calc(-50% + ${pan.x}px), calc(-50% + ${pan.y}px))`,
            pointerEvents: "none",
            userSelect: "none"
        }
        : {}

    return (
        <div
            style={{
                position: "fixed",
                inset: 0,
                zIndex: 9999,
                background: "rgba(0,0,0,0.85)",
                display: "flex",
                alignItems: "center",
                justifyContent: "center"
            }}
            onClick={onClose}
        >
            <div
                style={{
                    background: "#1a1a1a",
                    border: "1px solid rgba(255,255,255,0.14)",
                    borderRadius: "12px",
                    padding: "2rem",
                    maxWidth: "500px",
                    width: "90%"
                }}
                onClick={e => e.stopPropagation()}
            >
                <h2 style={{ color: "white", margin: "0 0 1rem", textAlign: "center", fontSize: "1.25rem" }}>
                    Crop Avatar
                </h2>

                {!image ? (
                    <p style={{ color: "rgba(255,255,255,0.6)", textAlign: "center" }}>loading...</p>
                ) : (
                    <>
                        <div
                            ref={containerRef}
                            style={containerStyle}
                            onWheel={handleWheel}
                            onMouseDown={handleMouseDown}
                            onMouseMove={handleMouseMove}
                            onMouseUp={handleMouseUp}
                            onMouseLeave={handleMouseUp}
                        >
                            <img ref={imgRef} src={image.src} alt="" style={imgStyle} draggable={false} />
                            <div
                                style={{
                                    position: "absolute",
                                    inset: 0,
                                    border: "2px solid white",
                                    borderRadius: "50%",
                                    boxShadow: "0 0 0 9999px rgba(0,0,0,0.5)",
                                    pointerEvents: "none"
                                }}
                            />
                        </div>

                        <div style={{ margin: "1rem 0" }}>
                            <label style={{ color: "rgba(255,255,255,0.6)", fontSize: "0.8rem", display: "block", marginBottom: "0.3rem" }}>
                                Zoom: {Math.round(zoom * 100)}%
                            </label>
                            <input
                                type="range"
                                min="50"
                                max="500"
                                value={Math.round(zoom * 100)}
                                onChange={e => setZoom(Number(e.target.value) / 100)}
                                style={{ width: "100%" }}
                            />
                        </div>

                        <div style={{ textAlign: "center", marginBottom: "1rem" }}>
                            <p style={{ color: "rgba(255,255,255,0.4)", fontSize: "0.75rem", margin: "0 0 0.5rem" }}>
                                Preview (512x512)
                            </p>
                            <canvas
                                ref={previewRef}
                                style={{
                                    width: "128px",
                                    height: "128px",
                                    borderRadius: "50%",
                                    border: "2px solid rgba(255,255,255,0.2)"
                                }}
                            />
                        </div>

                        <div style={{ display: "flex", gap: "0.75rem" }}>
                            <button
                                type="button"
                                onClick={onClose}
                                style={{
                                    flex: 1,
                                    padding: "0.75rem",
                                    background: "transparent",
                                    color: "white",
                                    border: "1px solid rgba(255,255,255,0.2)",
                                    cursor: "pointer",
                                    fontWeight: 700
                                }}
                            >
                                Cancel
                            </button>
                            <button
                                type="button"
                                onClick={handleSave}
                                style={{
                                    flex: 1,
                                    padding: "0.75rem",
                                    background: "white",
                                    color: "black",
                                    border: "none",
                                    cursor: "pointer",
                                    fontWeight: 700
                                }}
                            >
                                Save
                            </button>
                        </div>
                    </>
                )}
            </div>
        </div>
    )
}
