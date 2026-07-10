const modes = [
  { id: "dream", label: "Dream", icon: "✦" },
  { id: "draw", label: "Draw", icon: "✎" },
  { id: "physics", label: "Physics", icon: "⬟" },
  { id: "space", label: "Space", icon: "☄" },
  { id: "fractal", label: "Fractal", icon: "❋" },
  { id: "life", label: "Life", icon: "◉" }
]

export default function Rail({ mode, setMode, onRandomize, onClear, muted, setMuted }) {
  return (
    <div className="dreamRail">
      {modes.map(m => (
        <button
          key={m.id}
          className={`dreamRailBtn ${mode === m.id ? "active" : ""}`}
          onClick={() => setMode(m.id)}
          title={m.label}
          aria-label={m.label}
        >
          {m.icon}
        </button>
      ))}
      <div className="dreamRailSpacer" />
      <button className="dreamRailBtn" onClick={onRandomize} title="Randomize" aria-label="Randomize">🎲</button>
      <button className="dreamRailBtn" onClick={onClear} title="Clear" aria-label="Clear">✕</button>
      <button className="dreamRailBtn" onClick={() => setMuted(!muted)} title={muted ? "Unmute" : "Mute"} aria-label={muted ? "Unmute" : "Mute"}>
        {muted ? "🔇" : "🔊"}
      </button>
    </div>
  )
}
