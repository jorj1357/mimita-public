export default function DebugMetrics({ metrics }) {
  return (
    <div className="dreamMetrics">
      <div>{metrics.fps} FPS</div>
      <div>obj:{metrics.objects} part:{metrics.particles} cr:{metrics.creatures} draw:{metrics.draws}</div>
    </div>
  )
}
