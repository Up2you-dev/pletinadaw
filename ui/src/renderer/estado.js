/**
 * Todo el estado de la interfaz en un sitio, con suscripciones sencillas:
 * las vistas leen de aquí, cambian con `cambiar()` y repintan al recibir el
 * aviso. Sin framework, como en el reproductor.
 */

export const estado = {
  bpm: 120,
  compas: [4, 4],

  reproduciendo: false,
  // Última posición confirmada por el motor y su instante local, para que el
  // cursor interpole fino entre eventos (llegan a ~15 Hz).
  segundos: 0,
  refrescadoEn: 0,
  medidores: { izq: -100, der: -100 },

  motor: { estado: 'maqueta', info: null, binario: null },

  // Vista del arrangement.
  pxPorSegundo: 56,
  desplazamiento: 0,
  pistaSeleccionada: 3,

  mesaAbierta: true,
  railAbierto: true,

  // Pistas de demostración del esqueleto: enseñan el dibujo del arrangement
  // hasta que en F1 lleguen las de verdad desde el motor.
  pistas: [
    {
      nombre: 'Batería', color: '#8890FF',
      clips: [
        { nombre: 'ritmo seco', inicio: 0, duracion: 16, semilla: 11 },
        { nombre: 'ritmo lleno', inicio: 16, duracion: 16, semilla: 12 },
      ],
    },
    {
      nombre: 'Bajo', color: '#F0B357',
      clips: [
        { nombre: 'vuelta A', inicio: 4, duracion: 12, semilla: 23 },
        { nombre: 'vuelta B', inicio: 18, duracion: 10, semilla: 24 },
      ],
    },
    {
      nombre: 'Guitarras', color: '#FF7AA8',
      clips: [
        { nombre: 'arpegio', inicio: 8, duracion: 8, semilla: 35 },
        { nombre: 'pared', inicio: 20, duracion: 12, semilla: 36 },
      ],
    },
    {
      nombre: 'Voz', color: '#7FD4A1',
      clips: [
        { nombre: 'estrofa', inicio: 8, duracion: 10, semilla: 47 },
        { nombre: 'estribillo', inicio: 21, duracion: 8, semilla: 48 },
      ],
    },
  ],
};

const suscriptores = new Set();

/** Aplica cambios y avisa. Los repintados deciden qué les afecta. */
export function cambiar(cambios) {
  Object.assign(estado, cambios);
  for (const fn of suscriptores) fn(estado, cambios);
}

export function suscribir(fn) {
  suscriptores.add(fn);
  return () => suscriptores.delete(fn);
}

/** Posición para pintar ahora mismo: la confirmada más lo que haya corrido. */
export function posicionParaPintar(ahora = performance.now()) {
  if (!estado.reproduciendo) return estado.segundos;
  return estado.segundos + (ahora - estado.refrescadoEn) / 1000;
}
