const MAX_ANOMALIES = 20

const symbols = [
  "⟁", "∞", "▓", "hello?", "0.00001", "夢", "∴", "✦", "???",
  "⚡", "☯", "∆", "π", "φ", "λ", "ψ", "Ω", "∑",
  "¤", "〰", "➜", "◈", "◇", "⏣", "⊚", "⨁", "⨂",
  "⨀", "⎔", "⎈", "⏥", "⌘", "⌥", "⏎", "✧", "❖"
]

export function tickTextAnomalies(world, dt) {
  if (world.textAnomalies.length < MAX_ANOMALIES && world.rng() < dt * 0.5) {
    const hasBg = world.rng() > 0.5
    world.textAnomalies.push({
      x: world.rng() * (world.width - 50) + 25,
      y: world.height + 10,
      text: symbols[Math.floor(world.rng() * symbols.length)],
      size: 12 + world.rng() * 16,
      color: `hsl(${world.rng() * 360}, 100%, 70%)`,
      bg: hasBg ? `hsla(${world.rng() * 360}, 60%, 30%, 0.6)` : null,
      life: 3 + world.rng() * 4,
      maxLife: 7
    })
  }
}

export function clearTextAnomalies(world) {
  world.textAnomalies = []
}
