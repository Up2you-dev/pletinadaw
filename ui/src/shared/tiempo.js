/**
 * Aritmética y formato del tiempo musical. Todo puro y probado: la regla, el
 * transporte y el imán del arrangement dependen de que esto no mienta.
 */

/** Segundos que dura un pulso (una negra) a un tempo dado. */
export const segundosPorPulso = (bpm) => 60 / bpm;

/** Segundos que dura un compás entero. */
export const segundosPorCompas = (bpm, pulsosPorCompas = 4) =>
  segundosPorPulso(bpm) * pulsosPorCompas;

/**
 * De segundos a posición musical. Compás y pulso empiezan en 1, como en
 * cualquier DAW; las semicorcheas van de 1 a 4 dentro del pulso.
 */
export function posicionMusical(segundos, bpm, pulsosPorCompas = 4) {
  const pulsosTotales = Math.max(0, segundos) / segundosPorPulso(bpm);
  const compas = Math.floor(pulsosTotales / pulsosPorCompas) + 1;
  const pulso = Math.floor(pulsosTotales % pulsosPorCompas) + 1;
  const dieciseisavo = Math.floor((pulsosTotales % 1) * 4) + 1;
  return { compas, pulso, dieciseisavo };
}

/** `5.2.3` — lo que enseña el transporte al lado del reloj. */
export function formatoMusical(segundos, bpm, pulsosPorCompas = 4) {
  const { compas, pulso, dieciseisavo } = posicionMusical(segundos, bpm, pulsosPorCompas);
  return `${compas}.${pulso}.${dieciseisavo}`;
}

/** `m:ss.d` — reloj corto para el transporte. */
export function formatoReloj(segundos) {
  const s = Math.max(0, segundos);
  const minutos = Math.floor(s / 60);
  const resto = s - minutos * 60;
  const enteros = Math.floor(resto);
  const decimas = Math.floor((resto - enteros) * 10);
  return `${minutos}:${String(enteros).padStart(2, '0')}.${decimas}`;
}

/** Principio del compás en el que cae (o el más cercano hacia atrás), en segundos. */
export function principioDeCompas(segundos, bpm, pulsosPorCompas = 4) {
  const compas = segundosPorCompas(bpm, pulsosPorCompas);
  return Math.floor(Math.max(0, segundos) / compas) * compas;
}

/** Imán: la marca de rejilla más cercana, con la rejilla en fracciones de compás. */
export function imanASubdivision(segundos, bpm, subdivisionesPorCompas = 16, pulsosPorCompas = 4) {
  const paso = segundosPorCompas(bpm, pulsosPorCompas) / subdivisionesPorCompas;
  return Math.round(Math.max(0, segundos) / paso) * paso;
}

/** Un BPM que se puede enseñar y usar: número finito recortado a [20, 999]. */
export function bpmValido(valor, porDefecto = 120) {
  const n = Number(valor);
  if (!Number.isFinite(n)) return porDefecto;
  return Math.min(999, Math.max(20, n));
}
