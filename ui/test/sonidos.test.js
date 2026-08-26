import { describe, expect, it } from 'vitest';
import { filtrarSonidos, normalizar } from '../src/shared/sonidos.js';

const archivos = [
  { nombre: 'Zumbido.wav', ruta: '/z/zumbido.wav' },
  { nombre: 'caja Épica.wav', ruta: '/a/caja.wav' },
  { nombre: 'Bombo seco.wav', ruta: '/a/bombo.wav' },
  { nombre: 'ambiente.flac', ruta: '/b/ambiente.flac' },
];

describe('el navegador de sonidos', () => {
  it('normaliza: sin mayúsculas ni diacríticos (la ñ pliega a n, a propósito)', () => {
    expect(normalizar('Épica Ñu')).toBe('epica nu');
    expect(normalizar('CAJA')).toBe('caja');
    expect(normalizar('Montaña')).toBe('montana'); // "montana" la encuentra
  });

  it('sin búsqueda ordena alfabético y no muta la entrada', () => {
    const copia = [...archivos];
    const lista = filtrarSonidos(archivos, '');
    expect(lista.map((a) => a.nombre)).toEqual(
      ['ambiente.flac', 'Bombo seco.wav', 'caja Épica.wav', 'Zumbido.wav']);
    expect(archivos).toEqual(copia);
  });

  it('busca por subcadena sin acentos ni mayúsculas', () => {
    expect(filtrarSonidos(archivos, 'epi').map((a) => a.nombre)).toEqual(['caja Épica.wav']);
    expect(filtrarSonidos(archivos, 'ÉPI').map((a) => a.nombre)).toEqual(['caja Épica.wav']);
    expect(filtrarSonidos(archivos, 'nada')).toEqual([]);
  });

  it('los favoritos van siempre arriba, también buscando', () => {
    const favoritos = new Set(['/z/zumbido.wav']);
    expect(filtrarSonidos(archivos, '', favoritos)[0].nombre).toBe('Zumbido.wav');
    const conBusqueda = filtrarSonidos(archivos, 'o', favoritos);
    expect(conBusqueda[0].nombre).toBe('Zumbido.wav');
  });
});
