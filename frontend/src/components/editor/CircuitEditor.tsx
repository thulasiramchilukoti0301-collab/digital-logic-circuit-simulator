import { useState } from 'react'
import { createEmptyEditorState } from '../../types/editor'
import './CircuitEditor.css'

const paletteItems = [
  { type: 'input', label: 'INPUT', marker: 'IN' },
  { type: 'output', label: 'OUTPUT', marker: 'OUT' },
  { type: 'and', label: 'AND', marker: '&' },
  { type: 'or', label: 'OR', marker: '≥1' },
  { type: 'not', label: 'NOT', marker: '¬' },
  { type: 'xor', label: 'XOR', marker: '=1' },
  { type: 'nand', label: 'NAND', marker: '&̅' },
  { type: 'nor', label: 'NOR', marker: '≥̅' },
]

function ComponentPalette() {
  return (
    <aside className="editor-panel palette-panel" aria-labelledby="palette-title">
      <div className="panel-heading">
        <span className="panel-kicker">LIBRARY</span>
        <h3 id="palette-title">Components</h3>
      </div>
      <p className="panel-description">Available logic components</p>
      <div className="palette-list">
        {paletteItems.map((item) => (
          <button className="palette-item" key={item.type} type="button" aria-label={`${item.label} component`}>
            <span className={`palette-marker palette-marker--${item.type}`} aria-hidden="true">{item.marker}</span>
            <span>{item.label}</span>
            <span className="palette-add" aria-hidden="true">+</span>
          </button>
        ))}
      </div>
      <p className="palette-note">Placement controls are coming in a later editor milestone.</p>
    </aside>
  )
}

function CircuitCanvas({ componentCount, wireCount }: { componentCount: number; wireCount: number }) {
  return (
    <section className="canvas-panel" aria-labelledby="canvas-title">
      <div className="canvas-toolbar">
        <div>
          <p className="panel-kicker">WORKSPACE</p>
          <h3 id="canvas-title">Untitled circuit</h3>
        </div>
        <div className="canvas-counts" aria-label="Circuit contents">
          <span>{componentCount} components</span>
          <span>{wireCount} wires</span>
        </div>
      </div>
      <div className="circuit-canvas" role="region" aria-label="Circuit workspace">
        <div className="canvas-empty-state">
          <span className="canvas-empty-icon" aria-hidden="true">＋</span>
          <h4>Your circuit starts here</h4>
          <p>Choose components from the library to begin building a circuit.</p>
          <span className="canvas-empty-hint">VISUAL EDITOR FOUNDATION</span>
        </div>
      </div>
      <div className="canvas-status"><span className="status-dot" /> Ready for circuit components</div>
    </section>
  )
}

function InspectorPanel() {
  return (
    <aside className="editor-panel inspector-panel" aria-labelledby="inspector-title">
      <div className="panel-heading">
        <span className="panel-kicker">DETAILS</span>
        <h3 id="inspector-title">Inspector</h3>
      </div>
      <div className="inspector-empty">
        <span className="inspector-icon" aria-hidden="true">⌘</span>
        <p>Select a component to inspect its properties.</p>
      </div>
    </aside>
  )
}

export default function CircuitEditor() {
  const [editorState] = useState(createEmptyEditorState)

  return (
    <section className="editor-workspace" aria-label="Circuit editor">
      <div className="editor-actions">
        <div className="editor-document-title">
          <span className="document-icon" aria-hidden="true">◇</span>
          <span>New circuit</span>
          <span className="unsaved-indicator">EMPTY</span>
        </div>
        <div className="editor-action-status">
          <span className="engine-status-dot" />
          C++ engine is authoritative
        </div>
      </div>
      <div className="editor-grid">
        <ComponentPalette />
        <CircuitCanvas componentCount={editorState.components.length} wireCount={editorState.wires.length} />
        <InspectorPanel />
      </div>
    </section>
  )
}
