import { describe, expect, it } from 'vitest';
import { estado, cambiar, aplicarModelo, pistasDeMaqueta } from '../src/renderer/estado.js';

const modeloDeMotor = () => ({
  proyecto: { ruta: '/tmp/demo', nombre: 'demo', modificado: true },
  bpm: 97,
  metronomo: true,
  bucle: { activo: true, inicio: 1, fin: 5 },
  pistas: [
    { indice: 0, nombre: 'Voz', mute: false, solo: false, volumenDb: -3, pan: 0, plugins: [], clips: [
      { id: 'c1', nombre: 'toma', inicio: 0, duracion: 2, desfase: 0, ruta: '/tmp/a.wav' },
    ] },
    { indice: 1, nombre: 'Bajo', mute: true, solo: false, volumenDb: 0, pan: -0.5, plugins: [], clips: [] },
  ],
  master: { volumenDb: 0, pan: 0, plugins: [{ indice: 0, tipo: 'medidor', nombre: 'Medidor', activo: true, parametros: [] }] },
});

describe('la réplica del modelo del motor', () => {
  it('vuelca pistas, tempo, metrónomo, bucle y proyecto', () => {
    aplicarModelo(modeloDeMotor());
    expect(estado.pistas).toHaveLength(2);
    expect(estado.bpm).toBe(97);
    expect(estado.metronomo).toBe(true);
    expect(estado.bucle.fin).toBe(5);
    expect(estado.proyecto.nombre).toBe('demo');
    expect(estado.master.plugins[0].tipo).toBe('medidor');
  });

  it('asigna colores estables por índice: son de la interfaz, no del motor', () => {
    aplicarModelo(modeloDeMotor());
    const color0 = estado.pistas[0].color;
    expect(color0).toMatch(/^#/);
    aplicarModelo(modeloDeMotor());
    expect(estado.pistas[0].color).toBe(color0);
  });

  it('poda de la selección los clips que ya no existen', () => {
    aplicarModelo(modeloDeMotor());
    cambiar({ seleccion: new Set(['c1', 'fantasma']) });
    aplicarModelo(modeloDeMotor());
    expect([...estado.seleccion]).toEqual(['c1']);
  });

  it('si desaparecen pistas, la seleccionada no apunta al vacío', () => {
    aplicarModelo(modeloDeMotor());
    cambiar({ pistaSeleccionada: 1 });
    const menos = modeloDeMotor();
    menos.pistas = [menos.pistas[0]];
    aplicarModelo(menos);
    expect(estado.pistaSeleccionada).toBeLessThanOrEqual(0);
  });
});

describe('la maqueta', () => {
  it('trae pistas con clips y color para recorrer la interfaz sin motor', () => {
    const pistas = pistasDeMaqueta();
    expect(pistas.length).toBeGreaterThanOrEqual(3);
    for (const pista of pistas) {
      expect(pista.color).toMatch(/^#/);
      expect(Array.isArray(pista.clips)).toBe(true);
    }
  });
});
