import { useCallback, useEffect, useState } from 'react'
import { apiClient } from './services/apiClient'
import CircuitEditor from './components/editor/CircuitEditor'
import './App.css'

type BackendState = 'checking' | 'connected' | 'unavailable'

function App() {
  const [backendState, setBackendState] = useState<BackendState>('checking')

  const checkBackend = useCallback(async () => {
    setBackendState('checking')
    try {
      const health = await apiClient.health()
      setBackendState(health.status === 'ok' ? 'connected' : 'unavailable')
    } catch {
      setBackendState('unavailable')
    }
  }, [])

  useEffect(() => {
    void checkBackend()
  }, [checkBackend])

  const statusText = {
    checking: 'Checking connection',
    connected: 'Backend connected',
    unavailable: 'Backend unavailable',
  }[backendState]

  return (
    <main className="app-shell">
      <header className="topbar">
        <a className="brand" href="/" aria-label="Digital Logic Circuit Simulator home">
          <span className="brand-mark" aria-hidden="true"><span /><span /><span /></span>
          <span className="brand-name">Digital Logic <strong>Simulator</strong></span>
        </a>
        <div className="topbar-status">
          <div className={`connection-pill connection-pill--${backendState}`} role="status" aria-live="polite">
            <span className="connection-dot" />
            {statusText}
          </div>
          {backendState !== 'connected' && (
            <button
              className="retry-button"
              type="button"
              onClick={() => void checkBackend()}
              disabled={backendState === 'checking'}
            >
              {backendState === 'checking' ? 'Checking…' : 'Retry'}
            </button>
          )}
        </div>
      </header>

      <section className="page-heading" aria-labelledby="page-title">
        <div>
          <p className="eyebrow"><span className="eyebrow-line" /> CIRCUIT DESIGN</p>
          <h1 id="page-title">Circuit editor</h1>
          <p className="page-description">Arrange logic components and connections in a visual workspace.</p>
        </div>
        <div className="authority-note"><span /> C++17 ENGINE · AUTHORITATIVE SIMULATION</div>
      </section>

      <CircuitEditor />

      <footer className="footer">
        <span>React · TypeScript · Vite</span>
        <span className="footer-engine"><span /> C++ IS THE SIMULATION ENGINE</span>
      </footer>
    </main>
  )
}

export default App
