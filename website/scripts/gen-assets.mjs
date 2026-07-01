import sharp from "sharp"
import { writeFileSync } from "fs"
import { join, dirname } from "path"
import { fileURLToPath } from "url"

const __dirname = dirname(fileURLToPath(import.meta.url))
const publicDir = join(__dirname, "..", "public")

// ── Target placeholder PNG (bullseye with crosshair) ──
async function genTargetPng() {
  const size = 200
  const half = size / 2
  const rings = [
    { r: 90, color: [239, 68, 68] },
    { r: 70, color: [255, 255, 255] },
    { r: 50, color: [239, 68, 68] },
    { r: 30, color: [255, 255, 255] },
    { r: 12, color: [239, 68, 68] },
  ]

  let svg = `<svg width="${size}" height="${size}" xmlns="http://www.w3.org/2000/svg">`
  for (const r of rings) {
    svg += `<circle cx="${half}" cy="${half}" r="${r.r}" fill="rgb(${r.color[0]},${r.color[1]},${r.color[2]})"/>`
  }
  svg += `<line x1="${half}" y1="10" x2="${half}" y2="${size - 10}" stroke="rgba(0,0,0,0.25)" stroke-width="1.5"/>`
  svg += `<line x1="10" y1="${half}" x2="${size - 10}" y2="${half}" stroke="rgba(0,0,0,0.25)" stroke-width="1.5"/>`
  svg += `</svg>`

  await sharp(Buffer.from(svg)).png().toFile(join(publicDir, "assets/images/target-placeholder.png"))
  console.log("  target-placeholder.png")
}

// ── Simple WAV helper ──
function writeWav(filePath, sampleRate, samples) {
  const numChannels = 1
  const bitsPerSample = 16
  const byteRate = sampleRate * numChannels * (bitsPerSample / 8)
  const blockAlign = numChannels * (bitsPerSample / 8)
  const dataSize = samples.length * (bitsPerSample / 8)
  const buf = Buffer.alloc(44 + dataSize)

  const writeStr = (off, str) => { for (let i = 0; i < str.length; i++) buf[off + i] = str.charCodeAt(i) }

  writeStr(0, "RIFF")
  buf.writeUInt32LE(36 + dataSize, 4)
  writeStr(8, "WAVE")
  writeStr(12, "fmt ")
  buf.writeUInt32LE(16, 16) // chunk size
  buf.writeUInt16LE(1, 20)  // PCM
  buf.writeUInt16LE(numChannels, 22)
  buf.writeUInt32LE(sampleRate, 24)
  buf.writeUInt32LE(byteRate, 28)
  buf.writeUInt16LE(blockAlign, 32)
  buf.writeUInt16LE(bitsPerSample, 34)
  writeStr(36, "data")
  buf.writeUInt32LE(dataSize, 40)

  for (let i = 0; i < samples.length; i++) {
    const val = Math.max(-32768, Math.min(32767, Math.round(samples[i] * 32767)))
    buf.writeInt16LE(val, 44 + i * 2)
  }

  writeFileSync(filePath, buf)
}

// ── target-hit.wav: short high-pitched beep ──
function genHitSound() {
  const sr = 44100
  const duration = 0.08
  const len = Math.floor(sr * duration)
  const samples = new Float32Array(len)
  for (let i = 0; i < len; i++) {
    const t = i / sr
    const envelope = Math.max(0, 1 - t / duration)
    samples[i] = Math.sin(2 * Math.PI * 880 * t) * envelope * 0.6 +
                 Math.sin(2 * Math.PI * 1320 * t) * envelope * 0.3
  }
  writeWav(join(publicDir, "assets/audio/target-hit.wav"), sr, samples)
  console.log("  target-hit.wav")
}

// ── miss-click.wav: short low thud ──
function genMissSound() {
  const sr = 44100
  const duration = 0.1
  const len = Math.floor(sr * duration)
  const samples = new Float32Array(len)
  for (let i = 0; i < len; i++) {
    const t = i / sr
    const envelope = Math.max(0, 1 - t / duration)
    // Low frequency + noise
    samples[i] = (Math.sin(2 * Math.PI * 150 * t) * 0.5 + (Math.random() - 0.5) * 0.15) * envelope * 0.5
  }
  writeWav(join(publicDir, "assets/audio/miss-click.wav"), sr, samples)
  console.log("  miss-click.wav")
}

console.log("Generating assets...")
await genTargetPng()
genHitSound()
genMissSound()
console.log("Done.")
