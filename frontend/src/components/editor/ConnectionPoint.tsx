import { useRef } from 'react'
import type { PointerEvent } from 'react'
import type { EditorWireTarget } from '../../types/editor'
import { findInputPointUnderPointer } from './wireGeometry'

interface ConnectionPointProps {
  componentId: string
  kind: 'input' | 'output'
  pin?: number
  pinCount?: number
  active?: boolean
  targetState?: 'valid' | 'invalid' | null
  onActivate?: (componentId: string) => void
  onWireStart?: (sourceId: string) => void
  onWireMove?: (sourceId: string, clientX: number, clientY: number, target: EditorWireTarget | null) => void
  onWireEnd?: (sourceId: string, target: EditorWireTarget | null) => void
}

export default function ConnectionPoint({
  componentId,
  kind,
  pin = 0,
  pinCount = 1,
  active = false,
  targetState = null,
  onActivate,
  onWireStart,
  onWireMove,
  onWireEnd,
}: ConnectionPointProps) {
  const pointerId = useRef<number | null>(null)
  const isOutput = kind === 'output'

  const handlePointerDown = (event: PointerEvent<HTMLButtonElement>) => {
    event.stopPropagation()
    event.preventDefault()
    if (!isOutput) {
      onActivate?.(componentId)
      return
    }
    if (event.button !== 0) return
    pointerId.current = event.pointerId
    event.currentTarget.setPointerCapture(event.pointerId)
    onActivate?.(componentId)
    onWireStart?.(componentId)
  }

  const handlePointerMove = (event: PointerEvent<HTMLButtonElement>) => {
    event.stopPropagation()
    if (!isOutput || pointerId.current !== event.pointerId) return
    onWireMove?.(
      componentId,
      event.clientX,
      event.clientY,
      findInputPointUnderPointer(event.clientX, event.clientY),
    )
  }

  const finishPointer = (event: PointerEvent<HTMLButtonElement>, canceled: boolean) => {
    event.stopPropagation()
    if (!isOutput || pointerId.current !== event.pointerId) return
    pointerId.current = null
    onWireEnd?.(
      componentId,
      canceled ? null : findInputPointUnderPointer(event.clientX, event.clientY),
    )
    if (event.currentTarget.hasPointerCapture(event.pointerId)) {
      event.currentTarget.releasePointerCapture(event.pointerId)
    }
  }

  return (
    <button
      className={`connection-point connection-point--${kind}${active ? ' connection-point--active' : ''}${targetState ? ` connection-point--${targetState}` : ''}`}
      type="button"
      style={isOutput ? undefined : { top: `${((pin + 1) / (pinCount + 1)) * 100}%` }}
      data-connection-kind={kind}
      data-component-id={componentId}
      data-destination-pin={isOutput ? undefined : pin}
      aria-label={isOutput ? `Start wire from component ${componentId}` : `Input pin ${pin + 1} on component ${componentId}`}
      title={isOutput ? 'Drag to an input pin to connect' : `Input pin ${pin + 1}`}
      onPointerDown={handlePointerDown}
      onPointerMove={handlePointerMove}
      onPointerUp={(event) => finishPointer(event, false)}
      onPointerCancel={(event) => finishPointer(event, true)}
      onLostPointerCapture={() => { pointerId.current = null }}
      onClick={(event) => {
        event.stopPropagation()
        if (!isOutput) onActivate?.(componentId)
      }}
      onDragStart={(event) => event.preventDefault()}
    />
  )
}
