import type { CSSProperties } from 'react'
import type { EditorComponent } from '../../types/editor'

interface ComponentNodeProps {
  component: EditorComponent
  selected: boolean
  onSelect: (id: string) => void
}

export default function ComponentNode({ component, selected, onSelect }: ComponentNodeProps) {
  const style: CSSProperties = {
    left: component.position.x,
    top: component.position.y,
  }

  return (
    <button
      className={`canvas-component canvas-component--${component.type}${selected ? ' canvas-component--selected' : ''}`}
      type="button"
      style={style}
      onClick={() => onSelect(component.id)}
      aria-pressed={selected}
      aria-label={`Select ${component.type} ${component.name}`}
    >
      <span className="canvas-component-symbol" aria-hidden="true">{component.type.toUpperCase()}</span>
      <span className="canvas-component-name">{component.name}</span>
      {component.type === 'input' && <span className="canvas-component-value">Value: {component.value}</span>}
      {'inputCount' in component && <span className="canvas-component-pins">{component.inputCount} inputs</span>}
    </button>
  )
}
