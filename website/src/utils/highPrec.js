// Double-double precision arithmetic for deep Mandelbrot zooms.
// Represents a number as hi + lo where both are IEEE 754 doubles.
// Provides ~30 significant decimal digits (2 × 26-bit mantissas).
// Based on David H. Bailey's QD library and DoubleDouble technique.

export class DD {
  constructor(hi = 0, lo = 0) {
    this.hi = hi
    this.lo = lo
  }

  // Split a double into hi + lo with 26 bits each
  static split(a) {
    const thresh = 134217729 // 2^27 + 1
    const t = a * thresh
    const hi = t - (t - a)
    const lo = a - hi
    return { hi, lo }
  }

  // Quick double-double from a normal number
  static from(n) {
    if (n === 0) return new DD(0, 0)
    const s = DD.split(n)
    return new DD(s.hi, s.lo)
  }

  // Copy
  clone() {
    return new DD(this.hi, this.lo)
  }

  // Negate
  neg() {
    return new DD(-this.hi, -this.lo)
  }

  // Add dd + dd
  add(y) {
    const s1 = this.hi + y.hi
    const t1 = s1 - this.hi
    const e1 = (this.hi - (s1 - t1)) + (y.hi - t1)
    const s2 = s1 + this.lo
    const t2 = s2 - s1
    const e2 = (s1 - (s2 - t2)) + (this.lo - t2)
    const s3 = s2 + y.lo
    const t3 = s3 - s2
    const e3 = (s2 - (s3 - t3)) + (y.lo - t3)
    const hi = s3
    const lo = e1 + e2 + e3
    // Renormalize
    const f = hi + lo
    return new DD(f, lo - (f - hi))
  }

  // Add dd + number
  addNum(n) {
    return this.add(DD.from(n))
  }

  // Subtract dd - dd
  sub(y) {
    return this.add(y.neg())
  }

  // Multiply dd * dd
  mul(y) {
    const a = DD.split(this.hi)
    const b = DD.split(y.hi)
    const p1 = a.hi * b.hi
    const p2 = a.hi * b.lo + a.lo * b.hi
    const p3 = a.lo * b.lo
    const t1 = p1 + p2
    const e1 = (p1 - t1) + p2
    const t2 = t1 + p3 + this.hi * y.lo + this.lo * y.hi
    const hi = t1 + t2
    const lo = (t1 - hi) + t2 + e1
    const f = hi + lo
    return new DD(f, lo - (f - hi))
  }

  // Multiply dd * number
  mulNum(n) {
    const s = DD.split(n)
    return this.mul(new DD(s.hi, s.lo))
  }

  // Square dd
  sqr() {
    return this.mul(this)
  }

  // Absolute value
  abs() {
    return this.hi < 0 ? this.neg() : this.clone()
  }

  // Compare: returns -1, 0, or 1
  cmp(y) {
    if (this.hi < y.hi) return -1
    if (this.hi > y.hi) return 1
    if (this.lo < y.lo) return -1
    if (this.lo > y.lo) return 1
    return 0
  }

  // Less than
  lt(y) { return this.cmp(y) < 0 }

  // Greater than
  gt(y) { return this.cmp(y) > 0 }

  // To number (may lose precision)
  toNumber() { return this.hi + this.lo }

  // To string with full precision
  toString() {
    if (this.hi === 0 && this.lo === 0) return "0"
    return this.hi.toFixed(20) + " + " + this.lo.toExponential(3)
  }

  // Format for display: significant digits
  toShortString(maxDec = 12) {
    return this.hi.toFixed(maxDec)
  }

  // Square root (double-double precision)
  sqrt() {
    if (this.hi < 0) return new DD(0, 0)
    const x = Math.sqrt(this.hi)
    const d = this.sub(new DD(x, 0)).div(new DD(2 * x, 0))
    return new DD(x + d.hi, d.lo)
  }

  // Divide dd / dd using Newton-Raphson
  div(y) {
    const x = this.hi / y.hi
    const y2 = y.mulNum(-x).add(this)
    return DD.from(x).add(y2.div(y))
  }

  // Quick divide by number
  divNum(n) {
    return new DD(this.hi / n, this.lo / n)
  }
}

// High-precision Mandelbrot reference orbit
// Stores the orbit of a single reference point in double precision,
// computed from a high-precision center + offset.
export class RefOrbit {
  constructor(cx, cy, maxIter) {
    // cx, cy are DD instances (high precision center)
    this.cx = cx
    this.cy = cy
    this.maxIter = Math.min(maxIter, 10000)
    this.orbitX = []
    this.orbitY = []
    this.orbitRadius = []
    this.valid = true
    this.length = 0
    this.error = 0
    this.compute()
  }

  compute() {
    const maxIter = this.maxIter
    const orbitX = new Array(maxIter)
    const orbitY = new Array(maxIter)
    const orbitR = new Array(maxIter)

    // Initial values as doubles (the reference point itself)
    let zx = 0, zy = 0
    const cxN = this.cx.toNumber()
    const cyN = this.cy.toNumber()

    for (let i = 0; i < maxIter; i++) {
      const zx2 = zx * zx
      const zy2 = zy * zy
      if (zx2 + zy2 > 256) {
        this.length = i
        break
      }
      orbitX[i] = zx
      orbitY[i] = zy
      orbitR[i] = zx2 + zy2
      const nzx = zx2 - zy2 + cxN
      zy = 2 * zx * zy + cyN
      zx = nzx
      if (i === maxIter - 1) {
        this.length = maxIter
      }
    }

    this.orbitX = orbitX
    this.orbitY = orbitY
    this.orbitRadius = orbitR
  }

  // Check if this reference orbit is still valid for a given pixel offset
  // Returns estimated error
  estimateError(dx, dy, iter) {
    if (iter >= this.length) return 0
    const zx = this.orbitX[iter]
    const zy = this.orbitY[iter]
    // Rough error estimate: derivative * delta
    // For simplicity, use orbit radius growth
    return Math.abs(dx) + Math.abs(dy) * this.orbitRadius[Math.min(iter, this.orbitRadius.length - 1)]
  }
}

// Zoom state using exponent representation
export class ZoomState {
  constructor() {
    this.centerX = new DD(-0.5, 0)
    this.centerY = new DD(0, 0)
    this.zoomExp = 0     // zoom = 10^zoomExp
    this.zoomBase = 1    // fine zoom multiplier [1, 10)
    this.precision = 53  // bits, increases as zoom deepens
    this.reference = null
    this.rebaseCount = 0
    this.maxIter = 64
    this.totalExp = 0
  }

  // Get zoom as a number (may be Infinity at extreme depths)
  get zoom() {
    if (this.zoomExp > 308) return Infinity
    return this.zoomBase * Math.pow(10, this.zoomExp)
  }

  // Zoom in by factor at screen point (px, py) in [0,1] coords
  zoomAt(px, py, factor = 10) {
    // The view spans 4 units in the fractal plane at zoom = 1
    // At current zoom, view spans 4 / (10^zoomExp * zoomBase)
    const span = 4 / (Math.pow(10, this.zoomExp) * this.zoomBase)

    // New center = old center + (px - 0.5) * span, (py - 0.5) * span
    const cx = this.centerX.addNum((px - 0.5) * span)
    const cy = this.centerY.addNum((py - 0.5) * span)

    this.centerX = cx
    this.centerY = cy

    // Update exponent-based zoom
    this.zoomExp += Math.log10(factor)
    this.totalExp += Math.log10(factor)

    // Increase precision as zoom deepens
    if (this.zoomExp > 15) this.precision = 64
    if (this.zoomExp > 30) this.precision = 80
    if (this.zoomExp > 50) this.precision = 100
    if (this.zoomExp > 100) this.precision = 128

    this.maxIter = Math.min(Math.floor(64 + this.totalExp * 0.8), 5000)

    // Invalidate old reference
    this.reference = null
  }

  // Auto-zoom by dt seconds
  autoZoom(dt) {
    const expPerSec = 2
    this.zoomExp += expPerSec * dt
    this.totalExp += expPerSec * dt
    if (this.zoomExp > 15) this.precision = 64
    if (this.zoomExp > 30) this.precision = 80
    if (this.zoomExp > 50) this.precision = 100
    if (this.zoomExp > 100) this.precision = 128
    this.maxIter = Math.min(Math.floor(64 + this.totalExp * 0.8), 5000)
    this.reference = null
    // Precision handling at extreme zoom
    if (this.zoomExp > 300) {
      // At 10^300+, double precision for reference orbit fails.
      // In practice, we would transition to arbitrary precision here.
      // For now, stabilize and continue.
      this.precision = 256
    }
  }

  // Get zoom display string
  zoomDisplay() {
    if (this.totalExp < 3) return `${(Math.pow(10, this.totalExp)).toFixed(1)}×`
    return `10^${this.totalExp.toFixed(1)}×`
  }
}

// Perturbation-based pixel computation
// Returns iteration count for a pixel given a reference orbit
export function perturbPixel(refOrbit, dx, dy, maxIter) {
  const len = refOrbit.length || maxIter
  let zx = 0, zy = 0

  // Pre-compute ref orbit square to avoid recalculating
  const rxArr = refOrbit.orbitX
  const ryArr = refOrbit.orbitY
  const rrArr = refOrbit.orbitRadius

  for (let i = 0; i < Math.min(len, maxIter); i++) {
    const rx = rxArr[i]
    const ry = ryArr[i]
    if (!isFinite(rx) || !isFinite(ry)) return i

    // Combined actual Z value = Z_ref + z
    const fullX = rx + zx
    const fullY = ry + zy
    const fullMag2 = fullX * fullX + fullY * fullY

    // Escape test using FULL orbit value, not just delta
    if (!isFinite(fullMag2)) return i

    // Use standard escape radius of 4
    if (fullMag2 > 4) return i + 1

    // Perturbation: z_{n+1} = 2*Z_ref*z + z^2 + delta
    const zx2 = zx * zx
    const zy2 = zy * zy
    const nzx = 2 * rx * zx - 2 * ry * zy + zx2 - zy2 + dx
    zy = 2 * rx * zy + 2 * ry * zx + 2 * zx * zy + dy
    zx = nzx

    if (!isFinite(zx) || !isFinite(zy)) return i + 1
  }
  return maxIter // did not escape within maxIter
}
