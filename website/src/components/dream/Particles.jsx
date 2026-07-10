export const MAX_PARTICLES = 300

export function spawnParticle(world, x, y, count = 1) {
  for (let i = 0; i < count; i++) {
    if (world.particles.length >= MAX_PARTICLES) break
    const angle = world.rng() * Math.PI * 2
    const speed = 20 + world.rng() * 80
    world.particles.push({
      x, y,
      vx: Math.cos(angle) * speed,
      vy: Math.sin(angle) * speed - 50,
      size: 1 + world.rng() * 3,
      color: `hsl(${world.rng() * 360}, 100%, 70%)`,
      life: 0.5 + world.rng() * 1.5,
      maxLife: 2
    })
  }
}

export function spawnExplosionParticles(world, x, y, count = 40) {
  for (let i = 0; i < count; i++) {
    if (world.particles.length >= MAX_PARTICLES) break
    const angle = world.rng() * Math.PI * 2
    const speed = 50 + world.rng() * 200
    world.particles.push({
      x, y,
      vx: Math.cos(angle) * speed,
      vy: Math.sin(angle) * speed,
      size: 2 + world.rng() * 4,
      color: `hsl(${world.rng() * 60 + 10}, 100%, 60%)`,
      life: 0.3 + world.rng() * 0.5,
      maxLife: 0.8
    })
  }
}

export function tickIdleParticles(world, dt) {
  if (world.particles.length < 200 && world.rng() < dt * 3) {
    const x = world.rng() * world.width
    world.particles.push({
      x, y: -5,
      vx: (world.rng() - 0.5) * 15,
      vy: 10 + world.rng() * 20,
      size: 1 + world.rng() * 2,
      color: `hsla(${(world.time * 20 + x) % 360}, 80%, 70%, 0.5)`,
      life: 3 + world.rng() * 3,
      maxLife: 6
    })
  }
}
