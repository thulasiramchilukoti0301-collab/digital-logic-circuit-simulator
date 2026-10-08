import type {
  ApiFailure,
  CircuitRequest,
  HealthResponse,
  SimulationResponse,
  TruthTableResponse,
  ValidationResponse,
  SavedCircuitResponse, SavedCircuitsResponse, OpenCircuitResponse,
} from '../types/api'

export class ApiRequestError extends Error {
  readonly status: number
  readonly details: ApiFailure | null

  constructor(message: string, status: number, details: ApiFailure | null = null) {
    super(message)
    this.name = 'ApiRequestError'
    this.status = status
    this.details = details
  }
}

async function request<T>(path: string, init?: RequestInit): Promise<T> {
  let response: Response
  try {
    response = await fetch(path, init)
  } catch {
    throw new ApiRequestError('The local C++ backend could not be reached.', 0)
  }

  let body: unknown
  try {
    body = await response.json()
  } catch {
    throw new ApiRequestError('The backend returned an unreadable response.', response.status)
  }

  if (!response.ok) {
    const details = isApiFailure(body) ? body : null
    const message = details?.errors.map((error) => error.message).join(' ') ||
      `The backend request failed with status ${response.status}.`
    throw new ApiRequestError(message, response.status, details)
  }

  return body as T
}

function isApiFailure(value: unknown): value is ApiFailure {
  if (typeof value !== 'object' || value === null || !('success' in value) ||
      !('errors' in value) || !Array.isArray(value.errors)) {
    return false
  }
  return value.success === false
}

function postCircuit<T>(path: string, circuit: CircuitRequest): Promise<T> {
  return request<T>(path, {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify(circuit),
  })
}

export const apiClient = {
  health: () => request<HealthResponse>('/api/health'),
  validate: (circuit: CircuitRequest) =>
    postCircuit<ValidationResponse>('/api/validate', circuit),
  simulate: (circuit: CircuitRequest) =>
    postCircuit<SimulationResponse>('/api/simulate', circuit),
  truthTable: (circuit: CircuitRequest) =>
    postCircuit<TruthTableResponse>('/api/truth-table', circuit),
  listCircuits: () => request<SavedCircuitsResponse>('/api/circuits'),
  openCircuit: (id: number) => request<OpenCircuitResponse>(`/api/circuits/${id}`),
  saveCircuit: (name: string, circuit: CircuitRequest, id?: number) => request<SavedCircuitResponse>(id ? `/api/circuits/${id}` : '/api/circuits', {
    method: id ? 'PUT' : 'POST', headers: { 'Content-Type': 'application/json' }, body: JSON.stringify({ name, circuit }),
  }),
}
