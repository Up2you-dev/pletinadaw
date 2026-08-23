/**
 * Recorte y remuestreo de picos de onda: el motor manda los picos del archivo
 * fuente entero y aquí se corta la ventana que enseña cada clip (su desfase y
 * su duración) al número de columnas que el canvas vaya a pintar.
 * Puro y probado: pintar bonito depende de que esto no se salga del clip.
 */

/**
 * @param {number[]} picos      picos 0..100 del archivo fuente completo
 * @param {number} porSegundo   resolución de esos picos
 * @param {number} desfase      segundos de fuente saltados al principio del clip
 * @param {number} duracion     segundos visibles del clip
 * @param {number} columnas     cuántos valores se quieren para pintar
 * @returns {number[]}          columnas valores 0..100
 */
export function recortarPicos(picos, porSegundo, desfase, duracion, columnas) {
  const salida = new Array(Math.max(0, columnas)).fill(0);
  if (!picos?.length || columnas <= 0 || duracion <= 0 || porSegundo <= 0) return salida;

  const desde = desfase * porSegundo;
  const cubosPorColumna = (duracion * porSegundo) / columnas;

  for (let i = 0; i < columnas; i += 1) {
    const a = Math.floor(desde + i * cubosPorColumna);
    const b = Math.max(a + 1, Math.ceil(desde + (i + 1) * cubosPorColumna));
    let pico = 0;
    for (let j = a; j < b && j < picos.length; j += 1) {
      if (j >= 0 && picos[j] > pico) pico = picos[j];
    }
    salida[i] = pico;
  }
  return salida;
}

/** Un identificador estable para cachear los picos de un clip por su fuente. */
export const claveDePicos = (clip) => `${clip.ruta}`;
