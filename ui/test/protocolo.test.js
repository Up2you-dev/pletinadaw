import { describe, expect, it } from 'vitest';
import {
  METODOS,
  codificarPeticion,
  decodificarLinea,
  partirLineas,
} from '../src/shared/protocolo.js';

describe('codificar peticiones', () => {
  it('produce una línea NDJSON con id y método', () => {
    const linea = codificarPeticion(7, 'transporte.tocar');
    expect(linea.endsWith('\n')).toBe(true);
    expect(JSON.parse(linea)).toEqual({ id: 7, metodo: 'transporte.tocar' });
  });

  it('incluye los params solo cuando los hay', () => {
    const linea = codificarPeticion(1, 'edit.cargarAudio', { ruta: '/tmp/a.wav' });
    expect(JSON.parse(linea).params).toEqual({ ruta: '/tmp/a.wav' });
  });

  it('rechaza métodos que no estén en el protocolo', () => {
    expect(() => codificarPeticion(1, 'motor.autodestruccion')).toThrow(/fuera del protocolo/);
  });
});

describe('decodificar líneas', () => {
  it('reconoce respuestas con resultado', () => {
    const m = decodificarLinea('{"id":3,"resultado":{"reproduciendo":true}}');
    expect(m).toMatchObject({ tipo: 'respuesta', id: 3, resultado: { reproduciendo: true } });
  });

  it('reconoce respuestas con error', () => {
    const m = decodificarLinea('{"id":3,"error":{"codigo":-32000,"mensaje":"no existe"}}');
    expect(m.tipo).toBe('respuesta');
    expect(m.error.mensaje).toBe('no existe');
  });

  it('reconoce eventos sin id', () => {
    const m = decodificarLinea('{"evento":"medidores","datos":{"izq":-12}}');
    expect(m).toMatchObject({ tipo: 'evento', nombre: 'medidores', datos: { izq: -12 } });
  });

  it('lo que no es JSON del protocolo es ruido, no una excepción', () => {
    expect(decodificarLinea('JUCE v8.0.0').tipo).toBe('ruido');
    expect(decodificarLinea('').tipo).toBe('ruido');
    expect(decodificarLinea('42').tipo).toBe('ruido');
  });
});

describe('partir el flujo en líneas', () => {
  it('devuelve las líneas cerradas y guarda la cortada', () => {
    const a = partirLineas('', '{"id":1}\n{"id":2}\n{"id');
    expect(a.lineas).toEqual(['{"id":1}', '{"id":2}']);
    expect(a.resto).toBe('{"id');

    const b = partirLineas(a.resto, '":3}\n');
    expect(b.lineas).toEqual(['{"id":3}']);
    expect(b.resto).toBe('');
  });

  it('ignora líneas en blanco', () => {
    expect(partirLineas('', '\n\n{"id":1}\n\n').lineas).toEqual(['{"id":1}']);
  });
});

describe('la tabla de métodos', () => {
  it('no tiene duplicados', () => {
    expect(new Set(METODOS).size).toBe(METODOS.length);
  });

  it('usa nombres con forma de orden: zona.accion o palabra suelta', () => {
    for (const metodo of METODOS) expect(metodo).toMatch(/^[a-z]+(\.[a-zA-Z]+)?$/);
  });
});
