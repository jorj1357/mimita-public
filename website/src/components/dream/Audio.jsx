import { useRef, useCallback, useEffect } from "react"

export function useDreamAudio() {
  const ctxRef = useRef(null)
  const masterRef = useRef(null)
  const catsRef = useRef({})
  const mutedRef = useRef(false)
  const voicesRef = useRef(0)
  const MAX_VOICES = 16

  const start = useCallback(() => {
    if (ctxRef.current?.state !== "suspended") return
    ctxRef.current.resume()
  }, [])

  const ensure = useCallback(() => {
    if (ctxRef.current) return ctxRef.current
    const ctx = new (window.AudioContext || window.webkitAudioContext)()
    const master = ctx.createGain()
    master.gain.value = 0.5
    master.connect(ctx.destination)
    ctxRef.current = ctx
    masterRef.current = master
    for (const cat of ["draw", "ui", "collision", "explosion", "blackhole", "creature", "particle", "ambient"]) {
      const g = ctx.createGain()
      g.gain.value = 0.7
      g.connect(master)
      catsRef.current[cat] = g
    }
    return ctx
  }, [])

  const playTone = useCallback((cat, freq, duration, type = "sine", vol = 0.15) => {
    if (mutedRef.current) return
    if (voicesRef.current >= MAX_VOICES) return
    const ctx = ensure()
    const catGain = catsRef.current[cat]
    if (!catGain) return
    const osc = ctx.createOscillator()
    const env = ctx.createGain()
    osc.type = type
    osc.frequency.value = freq
    env.gain.setValueAtTime(vol, ctx.currentTime)
    env.gain.exponentialRampToValueAtTime(0.001, ctx.currentTime + duration)
    osc.connect(env)
    env.connect(catGain)
    osc.start(ctx.currentTime)
    osc.stop(ctx.currentTime + duration + 0.05)
    voicesRef.current++
    osc.onended = () => voicesRef.current--
  }, [ensure])

  const playNoise = useCallback((cat, duration, vol = 0.1) => {
    if (mutedRef.current) return
    if (voicesRef.current >= MAX_VOICES) return
    const ctx = ensure()
    const catGain = catsRef.current[cat]
    if (!catGain) return
    const bufferSize = Math.floor(ctx.sampleRate * duration)
    const buffer = ctx.createBuffer(1, bufferSize, ctx.sampleRate)
    const data = buffer.getChannelData(0)
    for (let i = 0; i < bufferSize; i++) data[i] = Math.random() * 2 - 1
    const src = ctx.createBufferSource()
    src.buffer = buffer
    const env = ctx.createGain()
    env.gain.setValueAtTime(vol, ctx.currentTime)
    env.gain.exponentialRampToValueAtTime(0.001, ctx.currentTime + duration)
    src.connect(env)
    env.connect(catGain)
    src.start(ctx.currentTime)
    voicesRef.current++
    src.onended = () => voicesRef.current--
  }, [ensure])

  const playImpact = useCallback((intensity = 0.5) => {
    playNoise("collision", 0.08 + intensity * 0.08, 0.05 + intensity * 0.1)
    playTone("collision", 200 + intensity * 400, 0.06, "sine", 0.04)
  }, [playNoise, playTone])

  const playExplosion = useCallback(() => {
    playNoise("explosion", 0.3, 0.25)
    playTone("explosion", 80, 0.3, "sawtooth", 0.15)
    playTone("explosion", 40, 0.4, "sine", 0.2)
  }, [playNoise, playTone])

  const playBlackHole = useCallback((intensity = 0.5) => {
    playTone("blackhole", 30 + intensity * 20, 0.15, "sine", 0.08 + intensity * 0.1)
  }, [playTone])

  const playDraw = useCallback(() => {
    playTone("draw", 600 + Math.random() * 400, 0.04, "sine", 0.03)
  }, [playTone])

  const playCreature = useCallback(() => {
    playTone("creature", 800 + Math.random() * 600, 0.08, "sine", 0.04)
  }, [playTone])

  const setMuted = useCallback((m) => {
    mutedRef.current = m
    if (masterRef.current) masterRef.current.gain.value = m ? 0 : 0.5
  }, [])

  const isMuted = useCallback(() => mutedRef.current, [])

  return { start, ensure, playTone, playNoise, playImpact, playExplosion, playBlackHole, playDraw, playCreature, setMuted, isMuted }
}
