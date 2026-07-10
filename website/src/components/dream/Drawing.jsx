export const MAX_DRAW_POINTS = 50000

export function addDrawSegment(world, x1, y1, x2, y2, hue, width = 3) {
  if (world.drawSegments.length >= MAX_DRAW_POINTS) return
  world.drawSegments.push({ x1, y1, x2, y2, hue, width, color: `hsl(${hue}, 100%, 60%)` })
}

// Apply symmetry: returns array of transformed point pairs
export function symmetryPoints(x, y, px, py, sym, cx, cy) {
  const pts = [{ x1: x, y1: y, x2: px, y2: py }]

  if (sym >= 1) { // Horizontal mirror
    pts.push({ x1: cx + (cx - x), y1: y, x2: cx + (cx - px), y2: py })
  }
  if (sym >= 2) { // Vertical mirror
    pts.push({ x1: x, y1: cy + (cy - y), x2: px, y2: cy + (cy - py) })
    pts.push({ x1: cx + (cx - x), y1: cy + (cy - y), x2: cx + (cx - px), y2: cy + (cy - py) })
  }
  if (sym >= 3) { // Radial 4
    for (let a = 90; a < 360; a += 90) {
      const rad = a * Math.PI / 180
      const c = Math.cos(rad)
      const s = Math.sin(rad)
      const rx = x - cx, ry = y - cy
      const rpx = px - cx, rpy = py - cy
      pts.push({
        x1: cx + rx * c - ry * s, y1: cy + rx * s + ry * c,
        x2: cx + rpx * c - rpy * s, y2: cy + rpx * s + rpy * c
      })
    }
  }
  if (sym >= 4) { // Radial 8
    for (let a = 45; a < 360; a += 45) {
      const rad = a * Math.PI / 180
      const c = Math.cos(rad)
      const s = Math.sin(rad)
      const rx = x - cx, ry = y - cy
      const rpx = px - cx, rpy = py - cy
      pts.push({
        x1: cx + rx * c - ry * s, y1: cy + rx * s + ry * c,
        x2: cx + rpx * c - rpy * s, y2: cy + rpy * s + rpy * c
      })
    }
  }
  return pts
}
