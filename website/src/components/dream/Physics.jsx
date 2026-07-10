export const MAX_PHYSICS_OBJECTS = 150

export function spawnBall(world, x, y, vx = 0, vy = 0) {
  if (world.physicsObjects.length >= MAX_PHYSICS_OBJECTS) return
  world.physicsObjects.push({
    x, y, vx, vy,
    width: 20 + world.rng() * 20,
    height: 20 + world.rng() * 20,
    shape: "circle",
    restitution: 0.5 + world.rng() * 0.3,
    rotation: 0,
    angularVel: (world.rng() - 0.5) * 5,
    color: `hsl(${world.rng() * 360}, 100%, 60%)`,
    alpha: 1,
    onGround: false
  })
}

export function spawnCube(world, x, y, vx = 0, vy = 0) {
  if (world.physicsObjects.length >= MAX_PHYSICS_OBJECTS) return
  world.physicsObjects.push({
    x, y, vx, vy,
    width: 20 + world.rng() * 30,
    height: 20 + world.rng() * 30,
    shape: "rect",
    restitution: 0.2 + world.rng() * 0.3,
    rotation: 0,
    angularVel: (world.rng() - 0.5) * 5,
    color: `hsl(${world.rng() * 360}, 100%, 60%)`,
    alpha: 1,
    onGround: false
  })
}

export function clearPhysics(world) {
  world.physicsObjects = []
  world.explosions = []
}

export function triggerExplosion(world, x, y, radius = 100, force = 500) {
  world.explosions.push({ x, y, radius, force, time: 0, maxTime: 0.3 })
  // Apply impulse to nearby objects
  for (const obj of world.physicsObjects) {
    const dx = obj.x - x
    const dy = obj.y - y
    const dist = Math.sqrt(dx * dx + dy * dy)
    if (dist < radius && dist > 0) {
      const falloff = 1 - dist / radius
      obj.vx += (dx / dist) * force * falloff
      obj.vy += (dy / dist) * force * falloff
      obj.angularVel += (world.rng() - 0.5) * 10 * falloff
    }
  }
}

export function tickExplosions(world, dt) {
  for (let i = world.explosions.length - 1; i >= 0; i--) {
    const e = world.explosions[i]
    e.time += dt
    if (e.time >= e.maxTime) world.explosions.splice(i, 1)
  }
}
