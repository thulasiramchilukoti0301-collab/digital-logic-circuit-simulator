import { useRef, useState } from 'react'
import type { EditorComponent, EditorState } from '../../types/editor'
import { createEmptyEditorState } from '../../types/editor'
import ComponentInspector from './ComponentInspector'
import ComponentNode from './ComponentNode'
import './CircuitEditor.css'

type PaletteType = EditorComponent['type']

const paletteItems: { type: PaletteType; label: string; marker: string }[] = [
  { type: 'input', label: 'INPUT', marker: 'IN' },
  { type: 'output', label: 'OUTPUT', marker: 'OUT' },
  { type: 'and', label: 'AND', marker: '&' },
  { type: 'or', label: 'OR', marker: 'OR' },
  { type: 'not', label: 'NOT', marker: 'NOT' },
  { type: 'xor', label: 'XOR', marker: '=1' },
  { type: 'nand', label: 'NAND', marker: 'NAND' },
  { type: 'nor', label: 'NOR', marker: 'NOR' },
]

function ComponentPalette({ onAdd }: { onAdd: (type: PaletteType) => void }) {
  return (
    <aside className="editor-panel palette-panel" aria-labelledby="palette-title">
      <div className="panel-heading">
        <span className="panel-kicker">LIBRARY</span>
        <h3 id="palette-title">Components</h3>
      </div>
      <p className="panel-description">Click a component to add it to the workspace</p>
      <div className="palette-list">
        {paletteItems.map((item) => (
          <button
            className="palette-item"
            key={item.type}
            type="button"
            aria-label={`Add ${item.label} component`}
            onClick={() => onAdd(item.type)}
          >
            <span className={`palette-marker palette-marker--${item.type}`} aria-hidden="true">{item.marker}</span>
            <span>{item.label}</span>
            <span className="palette-add" aria-hidden="true">+</span>
          </button>
        ))}
      </div>
    </aside>
  )
}

function CircuitCanvas({
  state,
  onSelect,
  onMove,
}: {
  state: EditorState
  onSelect: (id: string) => void
  onMove: (id: string, x: number, y: number) => void
}) {
  const canvasRef = useRef<HTMLDivElement>(null)

  return (
    <section className="canvas-panel" aria-labelledby="canvas-title">
      <div className="canvas-toolbar">
        <div>
          <p className="panel-kicker">WORKSPACE</p>
          <h3 id="canvas-title">Untitled circuit</h3>
        </div>
        <div className="canvas-counts" aria-label="Circuit contents">
          <span>{state.components.length} components</span>
          <span>{state.wires.length} wires</span>
        </div>
      </div>
      <div className="circuit-canvas" ref={canvasRef} role="region" aria-label="Circuit workspace">
        {state.components.length === 0 ? (
          <div className="canvas-empty-state">
            <span className="canvas-empty-icon" aria-hidden="true">+</span>
            <h4>Your circuit starts here</h4>
            <p>Choose components from the library to begin building a circuit.</p>
            <span className="canvas-empty-hint">VISUAL EDITOR FOUNDATION</span>
          </div>
        ) : state.components.map((component) => (
          <ComponentNode
            key={component.id}
            component={component}
            selected={state.selectedComponentId === component.id}
            canvas={canvasRef.current}
            onSelect={onSelect}
            onMove={onMove}
          />
        ))}
      </div>
      <div className="canvas-status">
        <span className="status-dot" />
        {state.components.length ? 'Select a component to inspect it' : 'Ready for circuit components'}
      </div>
    </section>
  )
}

function nextAvailableId(components: EditorComponent[], counter: { current: bigint }): string {
  const usedIds = new Set(components.map((component) => component.id))
  let candidate = counter.current
  while (usedIds.has(candidate.toString())) candidate += 1n
  counter.current = candidate + 1n
  return candidate.toString()
}

function createComponent(type: PaletteType, id: string, components: EditorComponent[], index: number, canvasWidth: number): EditorComponent {
  const label = type === 'input' ? 'Input' : type === 'output' ? 'Output' : type.toUpperCase()
  const sameTypeCount = components.filter((component) => component.type === type).length
  const columns = Math.max(1, Math.floor((canvasWidth - 24) / 126))
  const position = { x: 12 + (index % columns) * 126, y: 12 + Math.floor(index / columns) * 90 }

  if (type === 'input') {
    return { id, type, name: `${label} ${sameTypeCount + 1}`, value: 0, position }
  }
  if (type === 'output') {
    return { id, type, name: `${label} ${sameTypeCount + 1}`, position }
  }
  return { id, type, name: label, inputCount: type === 'not' ? 1 : 2, position }
}

export default function CircuitEditor() {
  const [editorState, setEditorState] = useState(createEmptyEditorState)
  const nextIdCounter = useRef(1n)
  const canvasWidth = typeof window === 'undefined' ? 500 : window.innerWidth - 470

  const addComponent = (type: PaletteType) => {
    const id = nextAvailableId(editorState.components, nextIdCounter)
    const component = createComponent(type, id, editorState.components, editorState.components.length, Math.max(180, canvasWidth))
    setEditorState((current) => ({
      ...current,
      components: [...current.components, component],
      selectedComponentId: component.id,
    }))
  }

  const selectComponent = (id: string) => {
    setEditorState((current) => ({ ...current, selectedComponentId: id }))
  }

  const moveComponent = (id: string, x: number, y: number) => {
    setEditorState((current) => ({
      ...current,
      components: current.components.map((component) =>
        component.id === id ? { ...component, position: { x, y } } : component,
      ),
      selectedComponentId: id,
    }))
  }

  const selectedComponent = editorState.components.find(
    (component) => component.id === editorState.selectedComponentId,
  ) ?? null

  return (
    <section className="editor-workspace" aria-label="Circuit editor">
      <div className="editor-actions">
        <div className="editor-document-title">
          <span className="document-icon" aria-hidden="true">C</span>
          <span>New circuit</span>
          <span className="unsaved-indicator">UNSAVED</span>
        </div>
        <div className="editor-action-status">
          <span className="engine-status-dot" />
          C++ engine is authoritative
        </div>
      </div>
      <div className="editor-grid">
        <ComponentPalette onAdd={addComponent} />
        <CircuitCanvas state={editorState} onSelect={selectComponent} onMove={moveComponent} />
        <ComponentInspector component={selectedComponent} />
      </div>
    </section>
  )
}
