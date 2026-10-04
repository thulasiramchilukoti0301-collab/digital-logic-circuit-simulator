import type { EditorComponent, EditorPosition } from '../../types/editor'

export const COMPONENT_NODE_WIDTH = 108
export const COMPONENT_NODE_HEIGHT = 76

export function getOutputConnectionPosition(component: EditorComponent): EditorPosition {
  return {
    x: component.position.x + COMPONENT_NODE_WIDTH,
    y: component.position.y + COMPONENT_NODE_HEIGHT / 2,
  }
}

export function getInputConnectionPosition(component: EditorComponent, pin: number): EditorPosition {
  const inputCount = component.type === 'output' ? 1 : 'inputCount' in component ? component.inputCount : 0
  return {
    x: component.position.x,
    y: component.position.y + (COMPONENT_NODE_HEIGHT * (pin + 1)) / (inputCount + 1),
  }
}

export function clientPointToCanvas(
  clientX: number,
  clientY: number,
  canvas: HTMLDivElement,
): EditorPosition {
  const bounds = canvas.getBoundingClientRect()
  return {
    x: clientX - bounds.left - canvas.clientLeft + canvas.scrollLeft,
    y: clientY - bounds.top - canvas.clientTop + canvas.scrollTop,
  }
}

export function findInputPointUnderPointer(clientX: number, clientY: number) {
  const point = document.elementFromPoint(clientX, clientY)?.closest<HTMLElement>(
    '[data-connection-kind="input"]',
  )
  if (!point) return null

  const destinationId = point.dataset.componentId
  const destinationPin = Number(point.dataset.destinationPin)
  if (!destinationId || !Number.isInteger(destinationPin)) return null
  return { destinationId, destinationPin }
}
