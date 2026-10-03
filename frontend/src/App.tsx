import { useCallback, useEffect, useState } from 'react'
import { apiClient } from './services/apiClient'
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
          <span className="brand-mark" aria-hidden="true">
            <span />
            <span />
            <span />
          </span>
          <span className="brand-name">Digital Logic <strong>Simulator</strong></span>
        </a>
        <div className={`connection-pill connection-pill--${backendState}`} role="status" aria-live="polite">
          <span className="connection-dot" />
          {statusText}
        </div>
      </header>

      <section className="intro" aria-labelledby="page-title">
        <p className="eyebrow"><span className="eyebrow-line" /> C++17 SIMULATION WORKSPACE</p>
        <h1 id="page-title">Think in gates.<br /><span>See the logic.</span></h1>
        <p className="intro-copy">
          A browser shell for exploring digital circuits. The C++17 simulation engine
          is authoritative for validation, simulation, and truth-table results.
        </p>
      </section>

      <section className="service-card" aria-labelledby="service-title">
        <div className="service-icon" aria-hidden="true">
          <svg viewBox="0 0 24 24" fill="none">
            <path d="M4 7h5m6 0h5M4 17h5m6 0h5M9 7a3 3 0 1 0 6 0 3 3 0 0 0-6 0Zm0 10a3 3 0 1 0 6 0 3 3 0 0 0-6 0Z" />
          </svg>
        </div>
        <div className="service-copy">
          <p className="eyebrow">LOCAL SIMULATION ENGINE</p>
          <h2 id="service-title">{statusText}</h2>
          <p>
            {backendState === 'connected'
              ? 'The browser can reach your C++ API at 127.0.0.1:8080.'
              : backendState === 'checking'
                ? 'Looking for the local C++ service…'
                : 'Start the C++ server, then check the connection again.'}
          </p>
        </div>
        {backendState !== 'connected' && (
          <button
            className="retry-button"
            type="button"
            onClick={() => void checkBackend()}
            disabled={backendState === 'checking'}
          >
            {backendState === 'checking' ? 'Checking…' : 'Try again'}
            <span aria-hidden="true">↗</span>
          </button>
        )}
      </section>

      <section className="workspace" aria-labelledby="workspace-title">
        <div className="workspace-heading">
          <div>
            <p className="eyebrow">YOUR WORKSPACE</p>
            <h2 id="workspace-title">Circuit editor</h2>
          </div>
          <span className="coming-soon">COMING NEXT</span>
        </div>

        <div className="workspace-placeholder">
          <div className="placeholder-symbol" aria-hidden="true">
            <span className="symbol-core">＋</span>
            <span className="symbol-orbit symbol-orbit--one" />
            <span className="symbol-orbit symbol-orbit--two" />
          </div>
          <h3>A clear space for your next idea</h3>
          <p>The interactive circuit editor will live here.</p>
          <div className="placeholder-caption"><span /> EDITOR FOUNDATION IN PROGRESS</div>
        </div>
      </section>

      <footer className="footer">
        <span>HTML · CSS · React · TypeScript</span>
        <span className="footer-engine"><span /> C++ IS THE SIMULATION ENGINE</span>
      </footer>
    </main>
  )
}

export default App
