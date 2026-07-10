const MAX_CREATURES = 30

export function spawnCreature(world, x, y) {
  if (world.creatures.length >= MAX_CREATURES) return
  world.creatures.push({
    x, y,
    vx: (world.rng() - 0.5) * 50,
    vy: -50,
    target: null,
    fleeTarget: null,
    state: "wander",
    color: `hsl(${world.rng() * 360}, 100%, 70%)`,
    stateTimer: 0,
    soundCooldown: 0
  })
}

export function tickCreatures(world, dt, pointerX, pointerY, pointerDown) {
  for (const c of world.creatures) {
    c.soundCooldown -= dt
    c.stateTimer -= dt

    // Check for nearby explosions
    for (const e of world.explosions) {
      const dx = c.x - e.x
      const dy = c.y - e.y
      if (dx * dx + dy * dy < e.radius * e.radius) {
        c.fleeTarget = { x: c.x + dx * 3, y: c.y + dy * 3 }
        c.state = "flee"
        c.stateTimer = 1
      }
    }

    // State machine
    switch (c.state) {
      case "flee":
        if (c.fleeTarget) {
          const dx = c.fleeTarget.x - c.x
          const dy = c.fleeTarget.y - c.y
          c.vx += dx * dt * 5
          c.vy += dy * dt * 5
        }
        if (c.stateTimer <= 0) {
          c.state = "wander"
          c.target = null
        }
        break
      case "follow":
        if (pointerDown) {
          const dx = pointerX - c.x
          const dy = pointerY - c.y
          c.vx += dx * dt * 3
          c.vy += dy * dt * 3
        } else {
          c.state = "wander"
        }
        break
      default: // wander
        if (!c.target || Math.random() < 0.01) {
          c.target = {
            x: world.rng() * world.width,
            y: world.rng() * world.height * 0.5
          }
        }
        if (c.target) {
          const dx = c.target.x - c.x
          const dy = c.target.y - c.y
          c.vx += dx * dt * 1.5
          c.vy += dy * dt * 1.5
        }
        // React to pointer
        const pdx = pointerX - c.x
        const pdy = pointerY - c.y
        const pdist = Math.sqrt(pdx * pdx + pdy * pdy)
        if (pdist < 80) {
          c.vx -= (pdx / pdist) * 80 * dt
          c.vy -= (pdy / pdist) * 80 * dt
        }
        if (pdist < 30 && pointerDown) {
          c.state = "follow"
        }
        break
    }

    // Speed limit
    const speed = Math.sqrt(c.vx * c.vx + c.vy * c.vy)
    if (speed > 150) {
      c.vx = (c.vx / speed) * 150
      c.vy = (c.vy / speed) * 150
    }

    // Sound
    if (c.state !== "wander" && c.soundCooldown <= 0 && !world.muted) {
      c.soundCooldown = 0.5 + Math.random() * 0.5
    }
  }
}

export function clearCreatures(world) {
  world.creatures = []
}
