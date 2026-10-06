import type { SimulationUiState } from '../../types/simulation'

interface SimulationControlsProps {
  state: SimulationUiState
  onValidate: () => void
  onSimulate: () => void
}

function getStatus(state: SimulationUiState): string {
  if (state.phase === 'validating') return 'Validating circuit...'
  if (state.phase === 'simulating') return 'Simulating...'
  if (state.phase === 'success') return state.action === 'validate' ? 'Circuit valid' : 'Simulation complete'
  if (state.phase === 'error') {
    if (state.errorKind === 'backend') return 'Backend unavailable'
    return state.errorKind === 'validation' ? 'Validation failed' : 'Simulation failed'
  }
  return 'Ready'
}

export default function SimulationControls({ state, onValidate, onSimulate }: SimulationControlsProps) {
  const busy = state.phase === 'validating' || state.phase === 'simulating'

  return (
    <div className="simulation-actions">
      <div className="simulation-action-row">
        <span className={`simulation-status simulation-status--${state.phase}`} role="status" aria-live="polite">
          {getStatus(state)}
        </span>
        {state.stale && <span className="stale-results-badge">Results stale</span>}
        <button className="editor-action-button" type="button" onClick={onValidate} disabled={busy}>
          Validate
        </button>
        <button className="editor-action-button editor-action-button--primary" type="button" onClick={onSimulate} disabled={busy}>
          Simulate
        </button>
      </div>
      {state.errors.length > 0 && (
        <div className="simulation-errors" role="alert">
          {state.errors.map((error, index) => <p key={`${index}-${error}`}>{error}</p>)}
        </div>
      )}
    </div>
  )
}
