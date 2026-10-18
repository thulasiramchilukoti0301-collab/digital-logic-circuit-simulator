import type { EditorComponent, EditorWire } from '../../types/editor'

function Detail({ label, value }: { label: string; value: string }) {
  return (
    <div className="inspector-detail">
      <dt>{label}</dt>
      <dd>{value}</dd>
    </div>
  )
}

interface ComponentInspectorProps {
  component: EditorComponent | null
  selectedWire: EditorWire | null
  onDeleteComponent: () => void
  onDeleteWire: () => void
}

export default function ComponentInspector({ component, selectedWire, onDeleteComponent, onDeleteWire }: ComponentInspectorProps) {
  return (
    <aside className="editor-panel inspector-panel" aria-labelledby="inspector-title">
      <div className="panel-heading">
        <span className="panel-kicker">DETAILS</span>
        <h3 id="inspector-title">Inspector</h3>
      </div>
      {component ? (
        <>
        <dl className="inspector-details">
          <Detail label="Type" value={component.type.toUpperCase()} />
          <Detail label="ID" value={component.id} />
          <Detail label="Name" value={component.name} />
          <Detail label="Position" value={`x: ${component.position.x}, y: ${component.position.y}`} />
          {'inputCount' in component && <Detail label="Input count" value={String(component.inputCount)} />}
          {component.type === 'input' && <Detail label="Value" value={String(component.value)} />}
        </dl>
        <button className="inspector-delete-button" type="button" onClick={onDeleteComponent}>
          Delete Component
        </button>
        </>
      ) : selectedWire ? (
        <>
          <dl className="inspector-details">
            <Detail label="Type" value="WIRE" />
            <Detail label="Source component ID" value={selectedWire.sourceId} />
            <Detail label="Destination component ID" value={selectedWire.destinationId} />
            <Detail label="Destination pin" value={String(selectedWire.destinationPin)} />
          </dl>
          <button className="inspector-delete-button" type="button" onClick={onDeleteWire}>
            Delete Wire
          </button>
        </>
      ) : (
        <div className="inspector-empty">
          <span className="inspector-icon" aria-hidden="true">INFO</span>
          <p>Select a component to inspect its properties.</p>
        </div>
      )}
    </aside>
  )
}
