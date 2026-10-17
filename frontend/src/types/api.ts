export type BinaryValue = 0 | 1

export type GateType = 'and' | 'or' | 'not' | 'xor' | 'nand' | 'nor'

export interface InputComponentRequest {
  id: string
  type: 'input'
  name: string
  value: BinaryValue
  position?: { x: number; y: number }
}

export interface OutputComponentRequest {
  id: string
  type: 'output'
  name: string
  position?: { x: number; y: number }
}

export interface GateComponentRequest {
  id: string
  type: GateType
  name?: string
  inputCount: number
  position?: { x: number; y: number }
}

export type ComponentRequest =
  | InputComponentRequest
  | OutputComponentRequest
  | GateComponentRequest

export interface WireRequest {
  sourceId: string
  destinationId: string
  destinationPin: number
}

export interface CircuitRequest {
  version: 1
  components: ComponentRequest[]
  wires: WireRequest[]
}

export interface ApiError {
  code: string
  message: string
}

export interface ApiFailure {
  success: false
  errors: ApiError[]
}

export interface HealthResponse {
  status: 'ok'
  service: string
}

export interface SavedCircuitSummary { id: number; name: string; updatedAt: string }
export interface SavedCircuit extends SavedCircuitSummary, CircuitRequest {}
export interface SavedCircuitResponse { success: true; circuit: SavedCircuitSummary }
export interface SavedCircuitsResponse { success: true; circuits: SavedCircuitSummary[] }
export interface OpenCircuitResponse { success: true; circuit: SavedCircuit }

export interface ValidationResponse {
  success: true
}

export interface SimulationOutput {
  id: string
  name: string
  value: BinaryValue
}

export interface SimulationResponse {
  success: true
  outputs: SimulationOutput[]
}

export interface TruthTableColumn {
  id: string
  name: string
}

export interface TruthTableRow {
  inputs: BinaryValue[]
  outputs: BinaryValue[]
}

export interface TruthTableResponse {
  success: true
  inputs: TruthTableColumn[]
  outputs: TruthTableColumn[]
  rows: TruthTableRow[]
}
