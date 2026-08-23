import { describe, expect, it } from 'vitest';
import {
  bpmValido,
  formatoMusical,
  formatoReloj,
  imanASubdivision,
  posicionMusical,
  principioDeCompas,
  segundosPorCompas,
  segundosPorPulso,
} from '../src/shared/tiempo.js';

describe('duraciones', () => {
  it('a 120 BPM la negra dura medio segundo y el compás de 4/4 dos', () => {
    expect(segundosPorPulso(120)).toBe(0.5);
    expect(segundosPorCompas(120, 4)).toBe(2);
  });

  it('el compás de 3/4 dura tres pulsos', () => {
    expect(segundosPorCompas(60, 3)).toBe(3);
  });
});

describe('posición musical', () => {
  it('el cero es el uno del compás uno', () => {
    expect(posicionMusical(0, 120)).toEqual({ compas: 1, pulso: 1, dieciseisavo: 1 });
  });

  it('dos segundos a 120 BPM son el arranque del compás dos', () => {
    expect(posicionMusical(2, 120)).toEqual({ compas: 2, pulso: 1, dieciseisavo: 1 });
  });

  it('cuenta pulsos y dieciseisavos dentro del compás', () => {
    expect(posicionMusical(0.5, 120)).toEqual({ compas: 1, pulso: 2, dieciseisavo: 1 });
    expect(posicionMusical(0.125, 120).dieciseisavo).toBe(2);
  });

  it('los segundos negativos no rompen: se tratan como cero', () => {
    expect(posicionMusical(-3, 120)).toEqual({ compas: 1, pulso: 1, dieciseisavo: 1 });
  });

  it('formatea como compás.pulso.dieciseisavo', () => {
    expect(formatoMusical(2.5, 120)).toBe('2.2.1');
  });
});

describe('reloj', () => {
  it('formatea minutos, segundos y décimas', () => {
    expect(formatoReloj(0)).toBe('0:00.0');
    expect(formatoReloj(61.27)).toBe('1:01.2');
    expect(formatoReloj(599.99)).toBe('9:59.9');
  });

  it('no enseña tiempos negativos', () => {
    expect(formatoReloj(-5)).toBe('0:00.0');
  });
});

describe('rejilla', () => {
  it('encuentra el principio del compás', () => {
    expect(principioDeCompas(3.7, 120, 4)).toBe(2);
    expect(principioDeCompas(1.99, 120, 4)).toBe(0);
  });

  it('el imán va a la subdivisión más cercana, también hacia delante', () => {
    // A 120 BPM y 16 subdivisiones, la rejilla cae cada 0.125 s.
    expect(imanASubdivision(0.13, 120, 16)).toBeCloseTo(0.125, 10);
    expect(imanASubdivision(0.19, 120, 16)).toBeCloseTo(0.25, 10);
    expect(imanASubdivision(0, 120, 16)).toBe(0);
  });
});

describe('bpm válido', () => {
  it('acepta números razonables y recorta los absurdos', () => {
    expect(bpmValido('128')).toBe(128);
    expect(bpmValido(5)).toBe(20);
    expect(bpmValido(4000)).toBe(999);
  });

  it('con basura devuelve el tempo por defecto', () => {
    expect(bpmValido('rápido', 120)).toBe(120);
    expect(bpmValido(NaN, 97)).toBe(97);
  });
});
