import { ZoomState } from "../../utils/highPrec"

export function initFractal(world) {
  world.fractal = {
    zoomState: new ZoomState(),
    autoZoom: false,
    needRender: true,
    rendering: false,
    lastRenderMs: 0,
    log: []
  }
  world.mode = "fractal"
}

export function tickFractal(world, dt) {
  const f = world.fractal
  if (!f) return
  if (f.autoZoom) {
    f.zoomState.autoZoom(dt)
    f.needRender = true
  }
}

export function renderFractal(ctx, world) {
  const f = world.fractal
  if (!f || !f.needRender || f.rendering) return

  f.rendering = true
  const t0 = performance.now()

  const w = world.width
  const h = world.height
  const dpr = Math.min(world.dpr, 1.5)
  const iw = Math.ceil(w * dpr)
  const ih = Math.ceil(h * dpr)

  const zs = f.zoomState
  const maxIter = zs.maxIter
  const zoomFactor = Math.pow(10, zs.zoomExp) * zs.zoomBase
  const span = 4 / zoomFactor
  const cx = zs.centerX.toNumber()
  const cy = zs.centerY.toNumber()

  const imageData = ctx.createImageData(iw, ih)
  const data = imageData.data

  for (let py = 0; py < ih; py++) {
    for (let px = 0; px < iw; px++) {
      // Direct Mandelbrot computation
      const x0 = cx + (px / iw - 0.5) * span
      const y0 = cy + (py / ih - 0.5) * span

      if (!isFinite(x0) || !isFinite(y0)) {
        const idx = (py * iw + px) * 4
        data[idx] = 0; data[idx + 1] = 0; data[idx + 2] = 0; data[idx + 3] = 255
        continue
      }

      let zx = 0, zy = 0
      let iter = 0
      while (iter < maxIter) {
        const zx2 = zx * zx
        const zy2 = zy * zy
        if (!isFinite(zx2) || !isFinite(zy2)) break
        if (zx2 + zy2 > 4) break
        const nzx = zx2 - zy2 + x0
        zy = 2 * zx * zy + y0
        zx = nzx
        iter++
        if (!isFinite(zx) || !isFinite(zy)) { iter = maxIter; break }
      }

      const idx = (py * iw + px) * 4
      if (iter >= maxIter) {
        data[idx] = 0; data[idx + 1] = 0; data[idx + 2] = 0
      } else {
        const t = iter / maxIter
        const hue = (t * 360 + Math.log(zs.totalExp + 1) * 50) % 360
        const sat = 80 + t * 20
        const light = 20 + t * 45
        const rgb = hslToRgb(hue / 360, sat / 100, light / 100)
        data[idx] = rgb[0]; data[idx + 1] = rgb[1]; data[idx + 2] = rgb[2]
      }
      data[idx + 3] = 255
    }
  }

  ctx.putImageData(imageData, 0, 0)
  f.lastRenderMs = performance.now() - t0
  f.needRender = false
  f.rendering = false
}

function hslToRgb(h, s, l) {
  let r, g, b
  if (s === 0) { r = g = b = l }
  else {
    const hue2rgb = (p, q, t) => {
      if (t < 0) t += 1
      if (t > 1) t -= 1
      if (t < 1/6) return p + (q - p) * 6 * t
      if (t < 1/2) return q
      if (t < 2/3) return p + (q - p) * (2/3 - t) * 6
      return p
    }
    const q = l < 0.5 ? l * (1 + s) : l + s - l * s
    const p = 2 * l - q
    r = hue2rgb(p, q, h + 1/3)
    g = hue2rgb(p, q, h)
    b = hue2rgb(p, q, h - 1/3)
  }
  return [r * 255, g * 255, b * 255]
}

export function zoomFractalAt(world, px, py) {
  const f = world.fractal
  if (!f || !f.zoomState) return
  const w = world.width
  const h = world.height
  const zs = f.zoomState

  // Compute click position in current fractal coords using DD precision
  const span = 4 / Math.pow(10, zs.zoomExp)
  zs.centerX = zs.centerX.addNum((px / w - 0.5) * span)
  zs.centerY = zs.centerY.addNum((py / h - 0.5) * span)
  zs.zoomExp += 1  // log10(10) = 1
  zs.totalExp += 1

  // At extreme zoom, recenter and continue to avoid precision loss
  if (zs.zoomExp > 12) {
    zs.zoomExp = 0
    zs.maxIter = Math.min(zs.maxIter + 30, 5000)
  }

  zs.maxIter = Math.min(Math.floor(64 + zs.totalExp * 0.8), 5000)
  f.needRender = true
}

export function toggleAutoZoom(world) {
  const f = world.fractal
  if (!f) return
  f.autoZoom = !f.autoZoom
  f.needRender = true
}

export function resetFractalView(world) {
  const f = world.fractal
  if (!f) return
  f.zoomState = new ZoomState()
  f.autoZoom = false
  f.needRender = true
  f.log = []
}

export function getZoomDisplay(f) {
  if (!f || !f.zoomState) return "1×"
  return f.zoomState.zoomDisplay()
}

export function getLastRenderMs(f) {
  if (!f) return 0
  return f.lastRenderMs
}

export function getRebaseCount(f) {
  if (!f) return 0
  return f.zoomState?.rebaseCount || 0
}
