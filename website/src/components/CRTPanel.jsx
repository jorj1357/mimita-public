export default function CRTPanel({ children, style, ...props }) {
  return (
    <div
      {...props}
      style={{
        position: "relative",
        overflow: "hidden",
        ...style,
      }}
    >
      {children}
      <div
        style={{
          position: "absolute",
          inset: 0,
          background: "repeating-linear-gradient(0deg, rgba(0,0,0,0.08) 0px, rgba(0,0,0,0.08) 1px, transparent 1px, transparent 3px)",
          pointerEvents: "none",
          zIndex: 1,
        }}
      />
      <div
        style={{
          position: "absolute",
          inset: 0,
          background: "radial-gradient(ellipse at center, transparent 60%, rgba(0,0,0,0.4) 100%)",
          pointerEvents: "none",
          zIndex: 1,
        }}
      />
    </div>
  )
}
