import { useRef } from 'react'
import type { CSSProperties, KeyboardEvent, PointerEvent } from 'react'
import type { EditorComponent, EditorWireDraft, EditorWireTarget } from '../../types/editor'
import { componentHasOutput, getComponentInputCount } from '../../types/editor'
import ConnectionPoint from './ConnectionPoint'
import { clientPointToCanvas } from './wireGeometry'

interface ComponentNodeProps {
  component: EditorComponent
  selected: boolean
  canvas: HTMLDivElement | null
  wireDraft: EditorWireDraft | null
  onSelect: (id: string) => void
  onMove: (id: string, x: number, y: number) => void
  onWireStart: (sourceId: string) => void
  onWireMove: (sourceId: string, x: number, y: number, target: EditorWireTarget | null) => void
  onWireEnd: (sourceId: string, target: EditorWireTarget | null) => void
}

interface DragState {
  pointerId: number
  grabOffsetX: number
  grabOffsetY: number
}

export default function ComponentNode({
  component,
  selected,
  canvas,
  wireDraft,
  onSelect,
  onMove,
  onWireStart,
  onWireMove,
  onWireEnd,
}: ComponentNodeProps) {
  const dragState = useRef<DragState | null>(null)
  const style: CSSProperties = {
    left: component.position.x,
    top: component.position.y,
  }

  const handlePointerDown = (event: PointerEvent<HTMLDivElement>) => {
    if (event.button !== 0 || !canvas || (event.target as Element).closest('.connection-point')) return
    event.preventDefault()
    onSelect(component.id)

    const nodeBounds = event.currentTarget.getBoundingClientRect()
    dragState.current = {
      pointerId: event.pointerId,
      grabOffsetX: event.clientX - nodeBounds.left,
      grabOffsetY: event.clientY - nodeBounds.top,
    }
    event.currentTarget.setPointerCapture(event.pointerId)
  }

  const handlePointerMove = (event: PointerEvent<HTMLDivElement>) => {
    const activeDrag = dragState.current
    if (!canvas || !activeDrag || event.pointerId !== activeDrag.pointerId) return

    const canvasBounds = canvas.getBoundingClientRect()
    const width = event.currentTarget.offsetWidth
    const height = event.currentTarget.offsetHeight
    const minX = canvas.scrollLeft
    const minY = canvas.scrollTop
    const maxX = Math.max(minX, minX + canvas.clientWidth - width)
    const maxY = Math.max(minY, minY + canvas.clientHeight - height)
    const pointerX = event.clientX - canvasBounds.left - canvas.clientLeft + canvas.scrollLeft - activeDrag.grabOffsetX
    const pointerY = event.clientY - canvasBounds.top - canvas.clientTop + canvas.scrollTop - activeDrag.grabOffsetY
    const x = Math.min(maxX, Math.max(minX, pointerX))
    const y = Math.min(maxY, Math.max(minY, pointerY))

    onMove(component.id, x, y)
  }

  const finishDrag = (event: PointerEvent<HTMLDivElement>) => {
    if (dragState.current?.pointerId !== event.pointerId) return
    dragState.current = null
    if (event.currentTarget.hasPointerCapture(event.pointerId)) {
      event.currentTarget.releasePointerCapture(event.pointerId)
    }
  }

  const handleKeyDown = (event: KeyboardEvent<HTMLDivElement>) => {
    if (event.target !== event.currentTarget) return
    if (event.key === 'Enter' || event.key === ' ') {
      event.preventDefault()
      onSelect(component.id)
    }
  }

  const handleWireMove = (
    sourceId: string,
    clientX: number,
    clientY: number,
    target: EditorWireTarget | null,
  ) => {
    if (!canvas) return
    const point = clientPointToCanvas(clientX, clientY, canvas)
    onWireMove(sourceId, point.x, point.y, target)
  }

  const inputCount = getComponentInputCount(component)
  const hoveredPin = wireDraft?.hoveredTarget?.destinationId === component.id
    ? wireDraft.hoveredTarget.destinationPin
    : null

  return (
    <div
      className={`canvas-component canvas-component--${component.type}${selected ? ' canvas-component--selected' : ''}`}
      role="group"
      tabIndex={0}
      style={style}
      onClick={() => onSelect(component.id)}
      onPointerDown={handlePointerDown}
      onPointerMove={handlePointerMove}
      onPointerUp={finishDrag}
      onPointerCancel={finishDrag}
      onLostPointerCapture={() => { dragState.current = null }}
      onDragStart={(event) => event.preventDefault()}
      onKeyDown={handleKeyDown}
      aria-label={`${component.type} ${component.name}`}
    >
      <span className="canvas-component-symbol" aria-hidden="true">{component.type.toUpperCase()}</span>
      <span className="canvas-component-name">{component.name}</span>
      {component.type === 'input' && <span className="canvas-component-value">Value: {component.value}</span>}
      {'inputCount' in component && <span className="canvas-component-pins">{component.inputCount} inputs</span>}
      {Array.from({ length: inputCount }, (_, pin) => (
        <ConnectionPoint
          key={`input-${pin}`}
          componentId={component.id}
          kind="input"
          pin={pin}
          pinCount={inputCount}
          onActivate={onSelect}
          targetState={hoveredPin === pin ? wireDraft?.targetIsValid ? 'valid' : 'invalid' : null}
        />
      ))}
      {componentHasOutput(component) && (
        <ConnectionPoint
          componentId={component.id}
          kind="output"
          active={wireDraft?.sourceId === component.id}
          onActivate={onSelect}
          onWireStart={onWireStart}
          onWireMove={handleWireMove}
          onWireEnd={onWireEnd}
        />
      )}
    </div>
  )
}
