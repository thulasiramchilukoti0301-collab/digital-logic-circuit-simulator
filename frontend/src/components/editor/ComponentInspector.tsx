import type { EditorComponent } from '../../types/editor'

function Detail({ label, value }: { label: string; value: string }) {
  return (
    <div className="inspector-detail">
      <dt>{label}</dt>
      <dd>{value}</dd>
    </div>
  )
}

export default function ComponentInspector({ component }: { component: EditorComponent | null }) {
  return (
    <aside className="editor-panel inspector-panel" aria-labelledby="inspector-title">
      <div className="panel-heading">
        <span className="panel-kicker">DETAILS</span>
        <h3 id="inspector-title">Inspector</h3>
      </div>
      {component ? (
        <dl className="inspector-details">
          <Detail label="Type" value={component.type.toUpperCase()} />
          <Detail label="ID" value={component.id} />
          <Detail label="Name" value={component.name} />
          <Detail label="Position" value={`x: ${component.position.x}, y: ${component.position.y}`} />
          {'inputCount' in component && <Detail label="Input count" value={String(component.inputCount)} />}
          {component.type === 'input' && <Detail label="Value" value={String(component.value)} />}
        </dl>
      ) : (
        <div className="inspector-empty">
          <span className="inspector-icon" aria-hidden="true">INFO</span>
          <p>Select a component to inspect its properties.</p>
        </div>
      )}
    </aside>
  )
}
