const MAX_BLACK_HOLES = 5

export function spawnBlackHole(world, x, y, radius = 40, strength = 200, isDense = false) {
  if (world.blackHoles.length >= MAX_BLACK_HOLES) {
    // Remove oldest
    world.blackHoles.shift()
  }
  world.blackHoles.push({
    x, y,
    baseRadius: radius,
    radius,
    strength: isDense ? strength * 1.5 : strength,
    density: isDense ? 2.0 : 1.0,
    age: 0,
    isDense
  })
}

export function tickBlackHoles(world, dt) {
  for (const bh of world.blackHoles) {
    bh.age += dt
    bh.radius = bh.baseRadius + Math.sin(bh.age * 2) * bh.baseRadius * 0.1

    // Attract physics objects
    for (const obj of world.physicsObjects) {
      const dx = bh.x - obj.x
      const dy = bh.y - obj.y
      const dist = Math.sqrt(dx * dx + dy * dy)
      const consumeRadius = bh.isDense ? bh.radius * 0.5 : bh.radius
      if (dist < consumeRadius * 0.2) {
        obj.alpha = Math.max(0, obj.alpha - dt * 3 * bh.density)
        if (obj.alpha <= 0) { obj.x = -9999; obj.y = -9999 }
        continue
      }
      if (dist < bh.radius * 3) {
        const force = bh.strength * bh.density / (dist + 10)
        obj.vx += (dx / dist) * force * dt * 60
        obj.vy += (dy / dist) * force * dt * 60
      }
    }

    // Attract particles
    for (const p of world.particles) {
      const dx = bh.x - p.x
      const dy = bh.y - p.y
      const dist = Math.sqrt(dx * dx + dy * dy)
      if (dist < bh.radius) {
        p.life = 0
        continue
      }
      if (dist < bh.radius * 4) {
        const force = bh.strength * bh.density * 0.3 / (dist + 10)
        p.vx += (dx / dist) * force * dt * 60
        p.vy += (dy / dist) * force * dt * 60
      }
    }

    // Attract creatures
    for (const c of world.creatures) {
      const dx = bh.x - c.x
      const dy = bh.y - c.y
      const dist = Math.sqrt(dx * dx + dy * dy)
      if (dist < bh.radius * 2) {
        c.fleeTarget = { x: c.x - dx * 3, y: c.y - dy * 3 }
      }
    }
  }
}

export function removeDeadObjects(world) {
  world.physicsObjects = world.physicsObjects.filter(o => o.alpha > 0 && o.x > -1000)
}

export function clearBlackHoles(world) {
  world.blackHoles = []
}
