const DEFAULT_SIZE = 80

export function initGameOfLife(world) {
  world.gol = {
    cols: DEFAULT_SIZE,
    rows: Math.floor(DEFAULT_SIZE * (world.height / world.width)),
    grid: null,
    running: false,
    generation: 0,
    speed: 5, // frames per step
    frameCount: 0,
    needDraw: true
  }
  world.gol.rows = Math.max(20, Math.min(200, world.gol.rows))
  world.gol.cols = Math.max(20, Math.min(200, world.gol.cols))
  world.gol.grid = createGrid(world.gol.cols, world.gol.rows)
  randomizeGrid(world.gol.grid, world.gol.cols, world.gol.rows)
  world.mode = "life"
}

function createGrid(cols, rows) {
  const g = new Array(rows)
  for (let r = 0; r < rows; r++) {
    g[r] = new Uint8Array(cols)
  }
  return g
}

function randomizeGrid(grid, cols, rows) {
  for (let r = 0; r < rows; r++) {
    for (let c = 0; c < cols; c++) {
      grid[r][c] = Math.random() < 0.3 ? 1 : 0
    }
  }
}

export function clearGameOfLife(world) {
  if (!world.gol) return
  const { cols, rows } = world.gol
  world.gol.grid = createGrid(cols, rows)
  world.gol.generation = 0
  world.gol.needDraw = true
}

export function randomizeGameOfLife(world) {
  if (!world.gol) return
  const { cols, rows, grid } = world.gol
  randomizeGrid(grid, cols, rows)
  world.gol.generation = 0
  world.gol.needDraw = true
}

export function toggleCell(world, px, py) {
  if (!world.gol) return
  const { cols, rows } = world.gol
  const c = Math.floor(px / world.width * cols)
  const r = Math.floor(py / world.height * rows)
  if (c >= 0 && c < cols && r >= 0 && r < rows) {
    world.gol.grid[r][c] = world.gol.grid[r][c] ? 0 : 1
    world.gol.needDraw = true
  }
}

export function tickGameOfLife(world, dt) {
  const gol = world.gol
  if (!gol || !gol.running) return

  gol.frameCount++
  if (gol.frameCount < gol.speed) return
  gol.frameCount = 0

  const { cols, rows, grid } = gol
  const next = createGrid(cols, rows)

  for (let r = 0; r < rows; r++) {
    for (let c = 0; c < cols; c++) {
      let neighbors = 0
      for (let dr = -1; dr <= 1; dr++) {
        for (let dc = -1; dc <= 1; dc++) {
          if (dr === 0 && dc === 0) continue
          const nr = (r + dr + rows) % rows
          const nc = (c + dc + cols) % cols
          neighbors += grid[nr][nc]
        }
      }
      if (grid[r][c] === 1) {
        next[r][c] = (neighbors === 2 || neighbors === 3) ? 1 : 0
      } else {
        next[r][c] = (neighbors === 3) ? 1 : 0
      }
    }
  }

  gol.grid = next
  gol.generation++
  gol.needDraw = true
}

export function renderGameOfLife(ctx, world) {
  const gol = world.gol
  if (!gol || !gol.needDraw) return

  const { cols, rows, grid, generation } = gol
  const w = world.width
  const h = world.height
  const dpr = world.dpr
  const cellW = Math.ceil(w * dpr / cols)
  const cellH = Math.ceil(h * dpr / rows)

  // Semi-transparent background
  ctx.fillStyle = "rgba(0,0,0,0.85)"
  ctx.fillRect(0, 0, w * dpr, h * dpr)

  for (let r = 0; r < rows; r++) {
    for (let c = 0; c < cols; c++) {
      if (grid[r][c]) {
        const age = grid[r][c] // use value for age coloring
        const hue = (generation * 0.5 + c * 0.5 + r * 0.7) % 360
        ctx.fillStyle = `hsla(${hue}, 100%, 60%, 0.9)`
        ctx.fillRect(c * cellW, r * cellH, cellW, cellH)
      }
    }
  }

  // Generation counter
  ctx.fillStyle = "rgba(255,255,255,0.25)"
  ctx.font = `${12 * dpr}px "MingLiU", monospace`
  ctx.fillText(`gen ${generation}`, 8 * dpr, 16 * dpr)

  gol.needDraw = false
}
