import { describe, expect, it } from 'vitest';
import { recortarPicos } from '../src/shared/ondas.js';

describe('recortar picos para pintar', () => {
  // Fuente de 4 segundos a 2 picos por segundo: [10, 20, 30, 40, 50, 60, 70, 80]
  const fuente = [10, 20, 30, 40, 50, 60, 70, 80];

  it('sin desfase y a la misma resolución devuelve la fuente', () => {
    expect(recortarPicos(fuente, 2, 0, 4, 8)).toEqual(fuente);
  });

  it('el desfase salta el principio de la fuente', () => {
    // Ventana de 2 s empezando en el segundo 2: los cubos 4..7.
    expect(recortarPicos(fuente, 2, 2, 2, 4)).toEqual([50, 60, 70, 80]);
  });

  it('reducir columnas se queda con el pico de cada tramo', () => {
    expect(recortarPicos(fuente, 2, 0, 4, 4)).toEqual([20, 40, 60, 80]);
  });

  it('pedir más columnas que cubos repite sin salirse', () => {
    const salida = recortarPicos([10, 90], 1, 0, 2, 8);
    expect(salida).toHaveLength(8);
    expect(Math.max(...salida)).toBe(90);
    expect(salida[0]).toBe(10);
  });

  it('fuera de la fuente pinta silencio, no basura', () => {
    const salida = recortarPicos(fuente, 2, 10, 2, 4);
    expect(salida).toEqual([0, 0, 0, 0]);
  });

  it('con entradas vacías o absurdas devuelve ceros del tamaño pedido', () => {
    expect(recortarPicos([], 2, 0, 4, 3)).toEqual([0, 0, 0]);
    expect(recortarPicos(fuente, 2, 0, 0, 3)).toEqual([0, 0, 0]);
    expect(recortarPicos(fuente, 2, 0, 4, 0)).toEqual([]);
  });
});
