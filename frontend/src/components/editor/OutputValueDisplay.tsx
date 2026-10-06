import type { BinaryValue } from '../../types/api'

export default function OutputValueDisplay({ value, stale }: { value: BinaryValue | undefined; stale: boolean }) {
  if (value === undefined) {
    return <span className="output-result output-result--empty">No result</span>
  }

  return (
    <span className={`output-result output-result--${value}${stale ? ' output-result--stale' : ''}`}>
      <span>Result</span>
      <strong>{value}</strong>
      {stale && <small>STALE</small>}
    </span>
  )
}
