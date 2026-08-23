import { describe, expect, it } from 'vitest';
import { CATALOGO, CADENA_MAESTRA, buscarDispositivo } from '../src/shared/catalogo.js';

describe('el catálogo de la suite', () => {
  const todos = CATALOGO.flatMap((g) => g.dispositivos);

  it('no repite nombres: cada dispositivo es uno', () => {
    const nombres = todos.map((d) => d.nombre);
    expect(new Set(nombres).size).toBe(nombres.length);
  });

  it('toda entrada tiene nombre, detalle y una fase del roadmap', () => {
    for (const d of todos) {
      expect(d.nombre.length).toBeGreaterThan(1);
      expect(d.detalle.length).toBeGreaterThan(2);
      expect([0, 1, 2, 3, 4, 5, 6]).toContain(d.fase);
    }
  });

  it('la cadena del máster solo enseña dispositivos que existen en el catálogo', () => {
    for (const nombre of CADENA_MAESTRA) {
      expect(buscarDispositivo(nombre), `falta ${nombre}`).not.toBeNull();
    }
  });

  it('los pilares del mastering están y llegan pronto: es la prioridad declarada', () => {
    for (const nombre of ['Techo', 'Medidor', 'Anchura', 'Dither']) {
      const d = buscarDispositivo(nombre);
      expect(d, `falta ${nombre}`).not.toBeNull();
      expect(d.fase).toBeLessThanOrEqual(2);
    }
  });

  it('buscar algo que no existe devuelve null, no un accidente', () => {
    expect(buscarDispositivo('Reverb Cuántica')).toBeNull();
  });
});
