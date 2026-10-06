import type { BinaryValue } from './api'

export type SimulationPhase = 'idle' | 'validating' | 'simulating' | 'success' | 'error'
export type SimulationAction = 'validate' | 'simulate' | null
export type SimulationErrorKind = 'validation' | 'simulation' | 'backend' | null

export interface SimulationUiState {
  phase: SimulationPhase
  action: SimulationAction
  errorKind: SimulationErrorKind
  errors: string[]
  outputs: Record<string, BinaryValue>
  stale: boolean
}

export function createInitialSimulationState(): SimulationUiState {
  return {
    phase: 'idle',
    action: null,
    errorKind: null,
    errors: [],
    outputs: {},
    stale: false,
  }
}
