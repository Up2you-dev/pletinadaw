import { readFileSync } from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';
import { describe, expect, it } from 'vitest';
import { METODOS } from '../src/shared/protocolo.js';

/**
 * El contrato del protocolo, vigilado desde los dos lados: la tabla METODOS
 * de la interfaz debe ser idéntica a la del motor (motor/src/protocolo.h).
 * Añadir un método en un solo lado rompe aquí, que es donde se ve barato.
 */

const raiz = path.join(path.dirname(fileURLToPath(import.meta.url)), '..', '..');
const cabecera = readFileSync(path.join(raiz, 'motor', 'src', 'protocolo.h'), 'utf8');

function metodosDelMotor() {
  // La tabla vive entre `METODOS[] = {` y su `};`, un método entre comillas
  // por línea. El formato es parte del contrato: lo dice el propio archivo.
  const bloque = cabecera.match(/METODOS\[\]\s*=\s*\{([\s\S]*?)\};/);
  if (!bloque) return [];
  return [...bloque[1].matchAll(/"([^"]+)"/g)].map((m) => m[1]);
}

describe('contrato UI⇄motor', () => {
  it('la tabla del motor existe y no está vacía', () => {
    expect(metodosDelMotor().length).toBeGreaterThan(0);
  });

  it('las dos tablas de métodos son idénticas, en el mismo orden', () => {
    expect(metodosDelMotor()).toEqual(METODOS);
  });
});
