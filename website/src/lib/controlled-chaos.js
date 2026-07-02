function seededRandom(seed) {
  let s = seed
  return function () {
    s = (s * 16807) % 2147483647
    return (s - 1) / 2147483646
  }
}

const _cache = {}
let _counter = 0
const _rand = seededRandom(Date.now())

function hashStr(str) {
  let h = 0
  for (let i = 0; i < str.length; i++) {
    h = ((h << 5) - h + str.charCodeAt(i)) | 0
  }
  return h
}

export function getControlledChaos(label) {
  const key = label != null ? String(label) : `_auto_${_counter++}`
  if (_cache[key]) return _cache[key]

  let r
  if (label != null) {
    const h = hashStr(key)
    const localRand = seededRandom(h * 9301 + 49297)
    r = localRand()
    localRand() // discard second
  } else {
    r = _rand()
  }

  const result = {
    xOffset: (r - 0.5) * 40,
    yOffset: (r - 0.5) * 40,
    rotation: (r - 0.5) * 4,
    scale: 0.98 + r * 0.04,
  }

  _cache[key] = result

  const params = new URLSearchParams(window.location.search)
  if (params.get("debugWebsite") === "1") {
    console.log("[ControlledChaos]", `Element: ${key}`, {
      x: Math.round(result.xOffset * 10) / 10,
      y: Math.round(result.yOffset * 10) / 10,
      rotation: Math.round(result.rotation * 10) / 10,
      scale: Math.round(result.scale * 100) / 100,
    })
  }

  return result
}
