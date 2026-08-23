/**
 * Todo el estado de la interfaz en un sitio, con suscripciones sencillas.
 * Con motor, `pistas` y compañía son una réplica del modelo que el motor
 * emite tras cada cambio: la interfaz no adivina, replica. Sin motor, la
 * maqueta enseña unas pistas de demostración para que todo se pueda recorrer.
 */

const COLORES = ['#8890FF', '#F0B357', '#FF7AA8', '#7FD4A1', '#7FC4D4', '#C9A2FF', '#FFB37F', '#A2FFB8'];

export const estado = {
  bpm: 120,
  compas: [4, 4],
  metronomo: false,
  bucle: { activo: false, inicio: 0, fin: 0 },
  proyecto: { ruta: '', nombre: 'sin guardar', modificado: false },

  reproduciendo: false,
  grabando: false,
  // Última posición confirmada por el motor y su instante local, para que el
  // cursor interpole fino entre eventos (llegan a ~15 Hz).
  segundos: 0,
  refrescadoEn: 0,
  medidores: { izq: -100, der: -100, pistas: [], lufs: null, espectro: null },
  automatizando: false,

  motor: { estado: 'maqueta', info: null, binario: null },

  // Vista del arrangement.
  pxPorSegundo: 56,
  desplazamiento: 0,
  pistaSeleccionada: -1,          // -1 = máster (la tira enseña su cadena)
  seleccion: new Set(),           // ids de clips seleccionados
  exportando: false,
  pianoRoll: null,                // id del clip MIDI abierto en el piano roll

  pistas: [],
  master: { volumenDb: 0, pan: 0, plugins: [] },
};

/** Pistas de demostración para el modo maqueta (sin motor). */
export function pistasDeMaqueta() {
  return [
    {
      indice: 0, nombre: 'Batería', color: COLORES[0], mute: false, solo: false, volumenDb: 0, pan: 0, plugins: [],
      clips: [
        { id: 'm1', nombre: 'ritmo seco', inicio: 0, duracion: 16, desfase: 0, semilla: 11 },
        { id: 'm2', nombre: 'ritmo lleno', inicio: 16, duracion: 16, desfase: 0, semilla: 12 },
      ],
    },
    {
      indice: 1, nombre: 'Bajo', color: COLORES[1], mute: false, solo: false, volumenDb: 0, pan: 0, plugins: [],
      clips: [
        { id: 'm3', nombre: 'vuelta A', inicio: 4, duracion: 12, desfase: 0, semilla: 23 },
        { id: 'm4', nombre: 'vuelta B', inicio: 18, duracion: 10, desfase: 0, semilla: 24 },
      ],
    },
    {
      indice: 2, nombre: 'Guitarras', color: COLORES[2], mute: false, solo: false, volumenDb: 0, pan: 0, plugins: [],
      clips: [{ id: 'm5', nombre: 'arpegio', inicio: 8, duracion: 8, desfase: 0, semilla: 35 }],
    },
    {
      indice: 3, nombre: 'Voz', color: COLORES[3], mute: false, solo: false, volumenDb: 0, pan: 0, plugins: [],
      clips: [{ id: 'm6', nombre: 'estrofa', inicio: 8, duracion: 10, desfase: 0, semilla: 47 }],
    },
  ];
}

/**
 * Vuelca el modelo que manda el motor sobre el estado, conservando lo que es
 * de la interfaz (colores por índice, selección de clips que sigan vivos).
 */
export function aplicarModelo(modelo) {
  const vivos = new Set();

  const pistas = (modelo.pistas || []).map((pista, i) => {
    for (const clip of pista.clips || []) vivos.add(clip.id);
    return { ...pista, color: COLORES[i % COLORES.length] };
  });

  const seleccion = new Set([...estado.seleccion].filter((id) => vivos.has(id)));

  cambiar({
    pistas,
    seleccion,
    master: modelo.master || estado.master,
    bpm: modelo.bpm ?? estado.bpm,
    metronomo: !!modelo.metronomo,
    bucle: modelo.bucle || estado.bucle,
    proyecto: modelo.proyecto || estado.proyecto,
    pistaSeleccionada: Math.min(estado.pistaSeleccionada, pistas.length - 1),
  });
}

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
