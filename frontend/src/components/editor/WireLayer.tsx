import type { EditorComponent, EditorWire, EditorWireDraft } from '../../types/editor'
import { getInputConnectionPosition, getOutputConnectionPosition } from './wireGeometry'

interface WireLayerProps {
  components: EditorComponent[]
  wires: EditorWire[]
  draft: EditorWireDraft | null
  canvas: HTMLDivElement | null
  selectedWireIndex: number | null
  onSelectWire: (index: number) => void
  onDeleteWire: () => void
}

function pathFor(start: { x: number; y: number }, end: { x: number; y: number }) {
  return `M ${start.x} ${start.y} L ${end.x} ${end.y}`
}

export default function WireLayer({ components, wires, draft, canvas, selectedWireIndex, onSelectWire, onDeleteWire }: WireLayerProps) {
  const width = Math.max(canvas?.clientWidth ?? 0, canvas?.scrollWidth ?? 0)
  const height = Math.max(canvas?.clientHeight ?? 0, canvas?.scrollHeight ?? 0)

  return (
    <svg
      className="wire-layer"
      width={width}
      height={height}
      viewBox={`0 0 ${Math.max(1, width)} ${Math.max(1, height)}`}
      role="group"
      aria-label="Circuit wires"
    >
      {wires.map((wire, index) => {
        const source = components.find((component) => component.id === wire.sourceId)
        const destination = components.find((component) => component.id === wire.destinationId)
        if (!source || !destination) return null
        const start = getOutputConnectionPosition(source)
        const end = getInputConnectionPosition(destination, wire.destinationPin)
        const wireLabel = `Wire from component ${wire.sourceId} to component ${wire.destinationId}, input pin ${wire.destinationPin + 1}`
        return (
          <g key={`${wire.destinationId}-${wire.destinationPin}-${index}`}>
            <path
              className={`committed-wire${selectedWireIndex === index ? ' committed-wire--selected' : ''}`}
              d={pathFor(start, end)}
              aria-hidden="true"
            />
            <path
              className="wire-hit-target"
              d={pathFor(start, end)}
              role="button"
              tabIndex={0}
              style={{ pointerEvents: 'stroke' }}
              aria-label={wireLabel}
              onPointerDown={(event) => event.stopPropagation()}
              onClick={(event) => {
                event.stopPropagation()
                onSelectWire(index)
              }}
              onKeyDown={(event) => {
                if (event.key === 'Enter' || event.key === ' ') {
                  event.preventDefault()
                  event.stopPropagation()
                  onSelectWire(index)
                } else if ((event.key === 'Delete' || event.key === 'Backspace') && selectedWireIndex === index) {
                  event.preventDefault()
                  event.stopPropagation()
                  onDeleteWire()
                }
              }}
            />
          </g>
        )
      })}
      {draft && (() => {
        const source = components.find((component) => component.id === draft.sourceId)
        if (!source) return null
        return <path className={`temporary-wire${draft.targetIsValid ? ' temporary-wire--valid' : ''}`} d={pathFor(getOutputConnectionPosition(source), draft.pointer)} />
      })()}
    </svg>
  )
}
