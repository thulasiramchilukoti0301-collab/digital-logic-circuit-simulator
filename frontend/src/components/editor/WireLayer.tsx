import type { EditorComponent, EditorWire, EditorWireDraft } from '../../types/editor'
import { getInputConnectionPosition, getOutputConnectionPosition } from './wireGeometry'

interface WireLayerProps {
  components: EditorComponent[]
  wires: EditorWire[]
  draft: EditorWireDraft | null
  canvas: HTMLDivElement | null
}

function pathFor(start: { x: number; y: number }, end: { x: number; y: number }) {
  return `M ${start.x} ${start.y} L ${end.x} ${end.y}`
}

export default function WireLayer({ components, wires, draft, canvas }: WireLayerProps) {
  const width = Math.max(canvas?.clientWidth ?? 0, canvas?.scrollWidth ?? 0)
  const height = Math.max(canvas?.clientHeight ?? 0, canvas?.scrollHeight ?? 0)

  return (
    <svg
      className="wire-layer"
      width={width}
      height={height}
      viewBox={`0 0 ${Math.max(1, width)} ${Math.max(1, height)}`}
      aria-hidden="true"
    >
      {wires.map((wire, index) => {
        const source = components.find((component) => component.id === wire.sourceId)
        const destination = components.find((component) => component.id === wire.destinationId)
        if (!source || !destination) return null
        const start = getOutputConnectionPosition(source)
        const end = getInputConnectionPosition(destination, wire.destinationPin)
        return <path className="committed-wire" d={pathFor(start, end)} key={`${wire.destinationId}-${wire.destinationPin}-${index}`} />
      })}
      {draft && (() => {
        const source = components.find((component) => component.id === draft.sourceId)
        if (!source) return null
        return <path className={`temporary-wire${draft.targetIsValid ? ' temporary-wire--valid' : ''}`} d={pathFor(getOutputConnectionPosition(source), draft.pointer)} />
      })()}
    </svg>
  )
}
