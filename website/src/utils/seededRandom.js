// Seeded pseudo-random number generator (Mulberry32)
export function createRng(seed) {
  let s = seed | 0
  return {
    next() {
      s = (s + 0x6d2b79f5) | 0
      let t = Math.imul(s ^ (s >>> 15), 1 | s)
      t = (t + Math.imul(t ^ (t >>> 7), 61 | t)) ^ t
      return ((t ^ (t >>> 14)) >>> 0) / 4294967296
    },
    nextInt(min, max) {
      return Math.floor(this.next() * (max - min + 1)) + min
    },
    nextFloat(min, max) {
      return this.next() * (max - min) + min
    },
    pick(arr) {
      return arr[Math.floor(this.next() * arr.length)]
    },
    get seed() { return seed }
  }
}
