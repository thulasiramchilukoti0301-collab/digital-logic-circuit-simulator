import type { BinaryValue, CircuitRequest, GateType } from './api'

export interface EditorPosition {
  x: number
  y: number
}

export interface EditorInput {
  id: string
  type: 'input'
  name: string
  value: BinaryValue
  position: EditorPosition
}

export interface EditorOutput {
  id: string
  type: 'output'
  name: string
  position: EditorPosition
}

export interface EditorGate {
  id: string
  type: GateType
  name: string
  inputCount: number
  position: EditorPosition
}

export type EditorComponent = EditorInput | EditorOutput | EditorGate

export interface EditorWire {
  sourceId: string
  destinationId: string
  destinationPin: number
}

export interface EditorState {
  components: EditorComponent[]
  wires: EditorWire[]
  selectedComponentId: string | null
}

export function createEmptyEditorState(): EditorState {
  return {
    components: [],
    wires: [],
    selectedComponentId: null,
  }
}

/** Maps editor data to the backend request schema; simulation remains in C++. */
export function editorStateToCircuitRequest(state: EditorState): CircuitRequest {
  return {
    version: 1,
    components: state.components.map((component) => {
      const position = { x: component.position.x, y: component.position.y }

      if (component.type === 'input') {
        return {
          id: component.id,
          type: 'input' as const,
          name: component.name,
          value: component.value,
          position,
        }
      }

      if (component.type === 'output') {
        return {
          id: component.id,
          type: 'output' as const,
          name: component.name,
          position,
        }
      }

      return {
        id: component.id,
        type: component.type,
        name: component.name,
        inputCount: component.inputCount,
        position,
      }
    }),
    wires: state.wires.map((wire) => ({
      sourceId: wire.sourceId,
      destinationId: wire.destinationId,
      destinationPin: wire.destinationPin,
    })),
  }
}
