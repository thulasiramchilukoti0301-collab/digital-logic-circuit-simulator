import type { PointerEvent } from 'react'
import type { BinaryValue } from '../../types/api'

interface InputValueControlProps {
  value: BinaryValue
  onToggle: () => void
}

export default function InputValueControl({ value, onToggle }: InputValueControlProps) {
  const isolatePointer = (event: PointerEvent<HTMLButtonElement>) => {
    event.stopPropagation()
  }

  return (
    <button
      className={`input-value-control input-value-control--${value}`}
      type="button"
      aria-label={`Input value ${value}. Toggle to ${value === 0 ? 1 : 0}`}
      aria-pressed={value === 1}
      title="Toggle binary input"
      onPointerDown={isolatePointer}
      onPointerMove={isolatePointer}
      onPointerUp={isolatePointer}
      onPointerCancel={isolatePointer}
      onClick={(event) => {
        event.stopPropagation()
        onToggle()
      }}
      onDragStart={(event) => event.preventDefault()}
    >
      <span>Value</span>
      <strong>{value}</strong>
    </button>
  )
}
