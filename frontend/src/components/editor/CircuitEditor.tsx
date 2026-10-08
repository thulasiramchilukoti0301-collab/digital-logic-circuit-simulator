import { useEffect, useRef, useState } from 'react'
import type { EditorComponent, EditorState, EditorWireDraft, EditorWireTarget } from '../../types/editor'
import { canConnectComponents, componentHasOutput, createEmptyEditorState } from '../../types/editor'
import type { BinaryValue, SavedCircuitSummary } from '../../types/api'
import { ApiRequestError, apiClient } from '../../services/apiClient'
import { circuitRequestToEditorState, editorStateToCircuitRequest } from '../../types/editor'
import type { SimulationAction, SimulationErrorKind, SimulationUiState } from '../../types/simulation'
import { createInitialSimulationState } from '../../types/simulation'
import ComponentInspector from './ComponentInspector'
import ComponentNode from './ComponentNode'
import SimulationControls from './SimulationControls'
import WireLayer from './WireLayer'
import { getOutputConnectionPosition } from './wireGeometry'
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
  componentKey,
  wireDraft,
  onSelect,
  onMove,
  onWireStart,
  onWireMove,
  onWireEnd,
  onInputValueChange,
  simulationOutputs,
  simulationStale,
}: {
  state: EditorState
  componentKey: number
  wireDraft: EditorWireDraft | null
  onSelect: (id: string) => void
  onMove: (id: string, x: number, y: number) => void
  onWireStart: (sourceId: string) => void
  onWireMove: (sourceId: string, x: number, y: number, target: EditorWireTarget | null) => void
  onWireEnd: (sourceId: string, target: EditorWireTarget | null) => void
  onInputValueChange: (inputId: string, value: 0 | 1) => void
  simulationOutputs: Record<string, BinaryValue>
  simulationStale: boolean
}) {
  // A ref assignment does not schedule a render. Keep the mounted canvas in
  // state so WireLayer receives real dimensions immediately after mount/open.
  const [canvasElement, setCanvasElement] = useState<HTMLDivElement | null>(null)

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
      <div className="circuit-canvas" ref={setCanvasElement} role="region" aria-label="Circuit workspace">
        <WireLayer
          components={state.components}
          wires={state.wires}
          draft={wireDraft}
          canvas={canvasElement}
        />
        {state.components.length === 0 ? (
          <div className="canvas-empty-state">
            <span className="canvas-empty-icon" aria-hidden="true">+</span>
            <h4>Your circuit starts here</h4>
            <p>Choose components from the library to begin building a circuit.</p>
            <span className="canvas-empty-hint">VISUAL EDITOR FOUNDATION</span>
          </div>
        ) : state.components.map((component) => (
          <ComponentNode
            key={`${componentKey}-${component.id}`}
            component={component}
            selected={state.selectedComponentId === component.id}
            canvas={canvasElement}
            wireDraft={wireDraft}
            onSelect={onSelect}
            onMove={onMove}
            onWireStart={onWireStart}
            onWireMove={onWireMove}
            onWireEnd={onWireEnd}
            onInputValueChange={onInputValueChange}
            simulationValue={component.type === 'output' ? simulationOutputs[component.id] : undefined}
            simulationStale={simulationStale}
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

function getRequestErrors(error: unknown): string[] {
  if (error instanceof ApiRequestError && error.details?.errors.length) {
    return error.details.errors.map(({ code, message }) => `${code}: ${message}`)
  }
  if (error instanceof Error && error.message) return [error.message]
  return ['The request failed unexpectedly. Please try again.']
}

interface CircuitEditorProps {
  backendState: 'checking' | 'connected' | 'unavailable'
  onBackendUnavailable: () => void
}

export default function CircuitEditor({ backendState, onBackendUnavailable }: CircuitEditorProps) {
  const [editorState, setEditorState] = useState(createEmptyEditorState)
  const [wireDraft, setWireDraft] = useState<EditorWireDraft | null>(null)
  const [simulationState, setSimulationState] = useState<SimulationUiState>(createInitialSimulationState)
  const circuitRevision = useRef(0)
  const requestInFlight = useRef(false)
  const nextIdCounter = useRef(1n)
  const [savedId, setSavedId] = useState<number | null>(null)
  const [documentName, setDocumentName] = useState('New circuit')
  const [savedBaseline, setSavedBaseline] = useState('')
  const [savedCircuits, setSavedCircuits] = useState<SavedCircuitSummary[]>([])
  const [persistenceStatus, setPersistenceStatus] = useState('')
  const [circuitViewKey, setCircuitViewKey] = useState(0)
  const persistenceBusy = useRef(false)
  const requestSnapshot = editorStateToCircuitRequest(editorState)
  const dirty = JSON.stringify(requestSnapshot) !== savedBaseline
  useEffect(() => { void apiClient.listCircuits().then((result) => setSavedCircuits(result.circuits)).catch(() => setPersistenceStatus('Could not load saved circuits.')) }, [])
  const canvasWidth = typeof window === 'undefined' ? 500 : window.innerWidth - 470

  const addComponent = (type: PaletteType) => {
    const id = nextAvailableId(editorState.components, nextIdCounter)
    const component = createComponent(type, id, editorState.components, editorState.components.length, Math.max(180, canvasWidth))
    markCircuitChanged()
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
    markCircuitChanged()
    setEditorState((current) => ({
      ...current,
      components: current.components.map((component) =>
        component.id === id ? { ...component, position: { x, y } } : component,
      ),
      selectedComponentId: id,
    }))
  }

  const startWire = (sourceId: string) => {
    const source = editorState.components.find((component) => component.id === sourceId)
    if (!source || !componentHasOutput(source)) return
    setEditorState((current) => ({ ...current, selectedComponentId: sourceId }))
    setWireDraft({
      sourceId,
      pointer: getOutputConnectionPosition(source),
      hoveredTarget: null,
      targetIsValid: false,
    })
  }

  const moveWire = (sourceId: string, x: number, y: number, target: EditorWireTarget | null) => {
    const source = editorState.components.find((component) => component.id === sourceId)
    const destination = target && editorState.components.find((component) => component.id === target.destinationId)
    const targetIsValid = target !== null && !!source && !!destination &&
      canConnectComponents(source, destination, target.destinationPin, editorState.wires)
    setWireDraft((current) => current?.sourceId === sourceId
      ? { ...current, pointer: { x, y }, hoveredTarget: target, targetIsValid }
      : current)
  }

  const finishWire = (sourceId: string, target: EditorWireTarget | null) => {
    setWireDraft(null)
    if (!target) return
    const sourceAtDrop = editorState.components.find((component) => component.id === sourceId)
    const destinationAtDrop = editorState.components.find((component) => component.id === target.destinationId)
    if (!sourceAtDrop || !destinationAtDrop ||
      !canConnectComponents(sourceAtDrop, destinationAtDrop, target.destinationPin, editorState.wires)) return
    markCircuitChanged()
    setEditorState((current) => {
      const source = current.components.find((component) => component.id === sourceId)
      const destination = current.components.find((component) => component.id === target.destinationId)
      if (!source || !destination || !canConnectComponents(source, destination, target.destinationPin, current.wires)) {
        return current
      }
      return {
        ...current,
        wires: [...current.wires, {
          sourceId,
          destinationId: target.destinationId,
          destinationPin: target.destinationPin,
        }],
      }
    })
  }

  const updateInputValue = (inputId: string, value: 0 | 1) => {
    markCircuitChanged()
    setEditorState((current) => ({
      ...current,
      components: current.components.map((component) =>
        component.id === inputId && component.type === 'input'
          ? { ...component, value }
          : component,
      ),
      selectedComponentId: inputId,
    }))
  }

  function markCircuitChanged() {
    circuitRevision.current += 1
    setSimulationState((current) => {
      const hasPreviousOutputs = Object.keys(current.outputs).length > 0
      const changedState: SimulationUiState = {
        ...current,
        stale: current.stale || hasPreviousOutputs,
      }
      if (current.phase === 'validating' || current.phase === 'simulating') return changedState
      return {
        ...changedState,
        phase: 'idle',
        action: null,
        errorKind: null,
        errors: [],
      }
    })
  }

  const showBackendUnavailable = (action: Exclude<SimulationAction, null>, clearOutputs: boolean) => {
    onBackendUnavailable()
    setSimulationState((current) => ({
      ...current,
      phase: 'error',
      action,
      errorKind: 'backend',
      errors: ['The C++ backend is unavailable. Start the server and retry the action.'],
      outputs: clearOutputs ? {} : current.outputs,
      stale: clearOutputs ? false : current.stale,
    }))
  }

  const reportRequestFailure = (error: unknown, action: Exclude<SimulationAction, null>, kind: Exclude<SimulationErrorKind, null>, clearOutputs: boolean) => {
    if (error instanceof ApiRequestError && error.status === 0) {
      showBackendUnavailable(action, clearOutputs)
      return
    }
    setSimulationState((current) => ({
      ...current,
      phase: 'error',
      action,
      errorKind: kind,
      errors: getRequestErrors(error),
      outputs: clearOutputs ? {} : current.outputs,
      stale: clearOutputs ? false : current.stale,
    }))
  }

  const validateCircuit = async () => {
    if (requestInFlight.current || persistenceBusy.current) return
    if (backendState === 'unavailable') {
      showBackendUnavailable('validate', false)
      return
    }
    requestInFlight.current = true
    const requestRevision = circuitRevision.current
    const request = editorStateToCircuitRequest(editorState)
    setSimulationState((current) => ({
      ...current,
      phase: 'validating',
      action: 'validate',
      errorKind: null,
      errors: [],
    }))

    try {
      await apiClient.validate(request)
      if (requestRevision !== circuitRevision.current) {
        setSimulationState((current) => ({
          ...current,
          phase: 'error',
          action: 'validate',
          errorKind: 'validation',
          errors: ['The circuit changed during validation. Validate again to check the current circuit.'],
        }))
        return
      }
      setSimulationState((current) => ({
        ...current,
        phase: 'success',
        action: 'validate',
        errorKind: null,
        errors: [],
      }))
    } catch (error) {
      if (requestRevision !== circuitRevision.current) return
      reportRequestFailure(error, 'validate', 'validation', false)
    } finally {
      requestInFlight.current = false
    }
  }

  const simulateCircuit = async () => {
    if (requestInFlight.current || persistenceBusy.current) return
    if (backendState === 'unavailable') {
      showBackendUnavailable('simulate', true)
      return
    }
    requestInFlight.current = true
    const requestRevision = circuitRevision.current
    const request = editorStateToCircuitRequest(editorState)
    const expectedOutputIds = new Set(
      request.components.filter((component) => component.type === 'output').map((component) => component.id),
    )
    let stage: 'validation' | 'simulation' = 'validation'
    setSimulationState((current) => ({
      ...current,
      phase: 'validating',
      action: 'simulate',
      errorKind: null,
      errors: [],
      outputs: {},
      stale: false,
    }))

    try {
      await apiClient.validate(request)
      if (requestRevision !== circuitRevision.current) {
        setSimulationState((current) => ({
          ...current,
          phase: 'error',
          action: 'simulate',
          errorKind: 'validation',
          errors: ['The circuit changed during validation. Simulate again to evaluate the current circuit.'],
          outputs: {},
          stale: false,
        }))
        return
      }

      stage = 'simulation'
      setSimulationState((current) => ({ ...current, phase: 'simulating' }))
      const result = await apiClient.simulate(request)
      if (requestRevision !== circuitRevision.current) return
      const outputs: Record<string, BinaryValue> = {}
      for (const output of result.outputs) {
        if (expectedOutputIds.has(output.id)) outputs[output.id] = output.value
      }
      setSimulationState((current) => ({
        ...current,
        phase: 'success',
        action: 'simulate',
        errorKind: null,
        errors: [],
        outputs,
        stale: requestRevision !== circuitRevision.current,
      }))
    } catch (error) {
      if (requestRevision !== circuitRevision.current) return
      const errorKind = error instanceof ApiRequestError && error.status === 0 ? 'backend' : stage
      reportRequestFailure(error, 'simulate', errorKind, true)
    } finally {
      requestInFlight.current = false
    }
  }

  const saveCircuit = async () => {
    if (persistenceBusy.current || requestInFlight.current) return
    const name = savedId === null ? window.prompt('Name this circuit:') : documentName
    if (!name?.trim()) return
    const snapshot = editorStateToCircuitRequest(editorState)
    const revision = circuitRevision.current
    persistenceBusy.current = true; setPersistenceStatus('Saving…')
    try {
      const result = await apiClient.saveCircuit(name.trim(), snapshot, savedId ?? undefined)
      setSavedId(result.circuit.id); setDocumentName(result.circuit.name); setSavedBaseline(JSON.stringify(snapshot))
      setSavedCircuits((current) => [result.circuit, ...current.filter((entry) => entry.id !== result.circuit.id)])
      setPersistenceStatus(revision === circuitRevision.current ? 'Saved.' : 'Saved snapshot; newer edits are unsaved.')
    } catch (error) { setPersistenceStatus(getRequestErrors(error).join(' ')) }
    finally { persistenceBusy.current = false }
  }
  const openCircuit = async (id: number) => {
    if (persistenceBusy.current || requestInFlight.current) return
    if (dirty && !window.confirm('Discard unsaved circuit changes and open the selected circuit?')) return
    const revision = circuitRevision.current
    persistenceBusy.current = true; setPersistenceStatus('Opening…')
    try {
      const result = await apiClient.openCircuit(id)
      if (revision !== circuitRevision.current) { setPersistenceStatus('The editor changed while opening. Open again to replace the current changes.'); return }
      const doc = { version: result.circuit.version, components: result.circuit.components, wires: result.circuit.wires }
      const restored = circuitRequestToEditorState(doc)
      setEditorState(restored); setWireDraft(null); setSimulationState(createInitialSimulationState())
      setCircuitViewKey((key) => key + 1)
      circuitRevision.current += 1
      nextIdCounter.current = restored.components.reduce((next, component) => { const n = /^\d+$/.test(component.id) ? BigInt(component.id) + 1n : 1n; return n > next ? n : next }, 1n)
      // Baseline and dirty-state comparison use the same editor-to-request
      // normalization, avoiding false changes from JSON property ordering.
      setSavedId(id); setDocumentName(result.circuit.name); setSavedBaseline(JSON.stringify(editorStateToCircuitRequest(restored))); setPersistenceStatus('Opened.')
    } catch (error) { setPersistenceStatus(getRequestErrors(error).join(' ')) }
    finally { persistenceBusy.current = false }
  }

  const selectedComponent = editorState.components.find(
    (component) => component.id === editorState.selectedComponentId,
  ) ?? null

  return (
    <section className="editor-workspace" aria-label="Circuit editor">
      <div className="editor-actions">
        <div className="editor-document-title">
          <span className="document-icon" aria-hidden="true">C</span>
          <span>{documentName}</span>
          {dirty && <span className="unsaved-indicator">UNSAVED</span>}
        </div>
        <div className="circuit-persistence-controls">
          <button type="button" onClick={() => void saveCircuit()} disabled={persistenceBusy.current}>{savedId === null ? 'Save as…' : 'Save'}</button>
          <select aria-label="Open saved circuit" value="" disabled={persistenceBusy.current} onChange={(event) => { if (event.target.value) void openCircuit(Number(event.target.value)) }}>
            <option value="">Open saved…</option>{savedCircuits.map((item) => <option key={item.id} value={item.id}>{item.name}</option>)}
          </select>
          <span role="status" aria-live="polite">{persistenceStatus}</span>
        </div>
        <div className="editor-action-status">
          <span className="engine-status-dot" />
          C++ engine is authoritative
        </div>
        <SimulationControls state={simulationState} onValidate={() => void validateCircuit()} onSimulate={() => void simulateCircuit()} />
      </div>
      <div className="editor-grid">
        <ComponentPalette onAdd={addComponent} />
        <CircuitCanvas
          state={editorState}
          componentKey={circuitViewKey}
          wireDraft={wireDraft}
          onSelect={selectComponent}
          onMove={moveComponent}
          onWireStart={startWire}
          onWireMove={moveWire}
          onWireEnd={finishWire}
          onInputValueChange={updateInputValue}
          simulationOutputs={simulationState.outputs}
          simulationStale={simulationState.stale}
        />
        <ComponentInspector component={selectedComponent} />
      </div>
    </section>
  )
}
