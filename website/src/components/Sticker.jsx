import { useRef } from "react"

function seededRandom(seed) {
  let s = seed
  return function () {
    s = (s * 16807) % 2147483647
    return (s - 1) / 2147483646
  }
}

const rand = seededRandom(Date.now())

const STICKERS = [
  { text: "★", size: 88 },
  { text: "𖤐", size: 88 },
  { text: "✪", size: 88 },
  { text: "⍟", size: 88 },
  { text: "⭑", size: 88 },
  { text: "⭒", size: 88 },
  { text: "Mason troy  adams.", size: 88 },
  { text: "7 2 2026 JORJ1357 WAS HERE", size: 88 },
  { text: "✰", size: 88 },
  { text: "✱", size: 88 },
  { text: "Давай 1 на 1 на Dust 2, бро.", size: 88 },
  { text: "خلنا نلعب مواجهة فردية (1 ضد 1) في خريطة Dust 2 يا صاحبي.", size: 88 },
  { text: "Dust 2で1対1やろうぜ、ブラザー。", size: 88 },
  { text: "Vamos de 1v1 na Dust 2, mano.!!!!!!!!!!!!", size: 88 },
  { text: "#baidexweb", size: 88 },
  { text: "#retweet", size: 88 },
  { text: "#haveyoueverplayedrugby", size: 88 },
  { text: "DONT CLICK THis it literally doesnt   do anything", size: 88 },
  { text: "✸҉✸҉✸҉✸҉✸҉✸҉✸҉✸҉✸҉", size: 88 },
  { text: "֍֍֍֍֍֍Right-Facing Armenian Eternity Sign", size: 88 },
  { text: "０００", size: 88 },
  { text: "１１１", size: 88 },
  { text: "２２２", size: 88 },
  { text: "３３３", size: 88 },
  { text: "４４４", size: 88 },
  { text: "５５５", size: 88 },
  { text: "６６６", size: 88 },
  { text: "７７７", size: 88 },
  { text: "８８８", size: 88 },
  { text: "９９９", size: 88 },
  { text: "ｯ", size: 882 },
  { text: "￣￣￣￣Fullwidth macron", size: 88 },
  { text: "i love you", size: 88 },
  { text: "i am proud of you", size: 88 },
  { text: "you are doing great", size: 88 },
  { text: "i want to see you succeed", size: 88 },
  { text: "i believe in you", size: 88 },
  { text: "you CAN do it bro i KNOW u can", size: 88 },
  { text: "life is tuff.....you are tuffer 💖", size: 88 },
  { text: "kitty in the snow..kitty cold. kitty still breathing... kitty still going...", size: 88 },
  { text: "✦", size: 20 },
  { text: "✧", size: 22 },
  { text: ":3", size: 18 },
  { text: ":D", size: 20 },
  { text: "!!!!!!!!", size: 18 },
  { text: "1v1 me bro", size: 18 },
  { text: "XxXxXxX", size: 20 },
  { text: "O_o", size: 18 },
  { text: ">:D", size: 20 },
  { text: "c:", size: 20 },
  { text: "allalalalbabbbbab", size: 18 },
  { text: "Rag is watching", size: 18 },
]

export default function Sticker({ style, ...props }) {
  const idx = useRef(Math.floor(rand() * STICKERS.length))
  const sticker = STICKERS[idx.current]
  const x = useRef(rand() * 80 + 10)
  const y = useRef(rand() * 80 + 10)
  const rot = useRef((rand() - 0.5) * 60)
  const hue = useRef(Math.floor(rand() * 360))

  return (
    <span
      {...props}
      style={{
        position: "absolute",
        left: `${x.current}%`,
        top: `${y.current}%`,
        transform: `rotate(${rot.current}deg)`,
        fontSize: `${sticker.size}px`,
        color: `hsl(${hue.current}, 90%, 60%)`,
        opacity: 0.5,
        pointerEvents: "none",
        userSelect: "none",
        zIndex: 0,
        ...style,
      }}
    >
      {sticker.text}
    </span>
  )
}
