/**
 * El catálogo de la suite propia: lo que el rail enseña hoy y lo que la tira
 * de dispositivos insertará mañana. Cada entrada dice en qué fase llega
 * (docs/06-efectos.md es la referencia larga); `fase: 0` significa que ya
 * existe en el esqueleto.
 */

/** Lo que existe de verdad lleva `lista: true` (y su `tipo` del motor);
    el resto enseña la fase del roadmap en la que llegará. */

export const CATALOGO = [
  {
    grupo: 'Ecualizadores',
    dispositivos: [
      { nombre: 'EQ Ocho', detalle: 'paramétrico con analizador', fase: 1, tipo: 'eqocho', lista: true },
      { nombre: 'EQ Dinámico', detalle: 'bandas que comprimen', fase: 2, tipo: 'eqdinamico', lista: true },
      { nombre: 'Balancín', detalle: 'tilt de un mando', fase: 2, tipo: 'balancin', lista: true },
      { nombre: 'Válvulas', detalle: 'estilo Pultec', fase: 3 },
      { nombre: 'Consola', detalle: 'canal británico', fase: 3 },
      { nombre: 'Peine', detalle: 'gráfico de 31 bandas', fase: 3 },
    ],
  },
  {
    grupo: 'Dinámica',
    dispositivos: [
      { nombre: 'Compresor', detalle: 'VCA limpio', fase: 1, tipo: 'compresor', lista: true },
      { nombre: 'Pegamento', detalle: 'bus estilo SSL', fase: 2, tipo: 'pegamento', lista: true },
      { nombre: 'Multibanda', detalle: 'compresión por bandas', fase: 2, tipo: 'multibanda', lista: true },
      { nombre: 'Puerta', detalle: 'gate con sidechain', fase: 2, tipo: 'puerta', lista: true },
      { nombre: 'De-eser', detalle: 'las eses a raya', fase: 2, tipo: 'deeser', lista: true },
      { nombre: 'Remache', detalle: 'FET estilo 1176', fase: 3 },
      { nombre: 'Ópto', detalle: 'estilo LA-2A', fase: 3 },
      { nombre: 'Lámpara', detalle: 'vari-mu de máster', fase: 3 },
    ],
  },
  {
    grupo: 'Reverbs',
    dispositivos: [
      { nombre: 'Placa', detalle: 'plate densa', fase: 2, tipo: 'placa', lista: true },
      { nombre: 'Sala', detalle: 'algorítmica FDN', fase: 2, tipo: 'sala', lista: true },
      { nombre: 'Convolución', detalle: 'IRs de verdad', fase: 2 },
      { nombre: 'Muelle', detalle: 'spring con su boing', fase: 3 },
      { nombre: 'Espejismo', detalle: 'shimmer', fase: 3 },
    ],
  },
  {
    grupo: 'Delays y modulación',
    dispositivos: [
      { nombre: 'Delay', detalle: 'digital al tempo', fase: 2, tipo: 'delay', lista: true },
      { nombre: 'Eco', detalle: 'cinta estilo RE-201', fase: 3 },
      { nombre: 'Multitap', detalle: 'taps dibujables', fase: 3 },
      { nombre: 'Coro', detalle: 'y flanger y fase', fase: 3 },
      { nombre: 'Trémolo', detalle: 'y autopan', fase: 3 },
    ],
  },
  {
    grupo: 'Saturación',
    dispositivos: [
      { nombre: 'Óxido', detalle: 'cinta magnética', fase: 2, tipo: 'oxido', lista: true },
      { nombre: 'Triodo', detalle: 'válvula', fase: 3 },
      { nombre: 'Sumadora', detalle: 'consola apurada', fase: 3 },
      { nombre: 'Machacadora', detalle: 'bitcrusher lo-fi', fase: 3 },
    ],
  },
  {
    grupo: 'Mastering',
    dispositivos: [
      { nombre: 'Techo', detalle: 'limitador true-peak', fase: 1, tipo: 'techo', lista: true },
      { nombre: 'Medidor', detalle: 'LUFS, espectro, fase', fase: 1, tipo: 'medidor', lista: true },
      { nombre: 'Anchura', detalle: 'imager M/S', fase: 2, tipo: 'anchura', lista: true },
      { nombre: 'Chispa', detalle: 'exciter por bandas', fase: 2, tipo: 'chispa', lista: true },
      { nombre: 'Dither', detalle: 'TPDF con shaping', fase: 2, tipo: 'dither', lista: true },
    ],
  },
  {
    grupo: 'Utilidades',
    dispositivos: [
      { nombre: 'Utilidad', detalle: 'ganancia, fase, anchura', fase: 1, tipo: 'utilidad', lista: true },
      { nombre: 'Oscilador', detalle: 'señal de prueba', fase: 2, tipo: 'oscilador', lista: true },
      { nombre: 'Afinador', detalle: 'cromático', fase: 4 },
    ],
  },
  {
    grupo: 'Instrumentos',
    dispositivos: [
      { nombre: 'Cinta', detalle: 'sampler: arrastra y toca', fase: 4 },
      { nombre: 'Pads', detalle: '16 pads de batería', fase: 4 },
      { nombre: 'Bruma', detalle: 'sinte sustractivo', fase: 4 },
    ],
  },
];

/** Lo que ya se puede insertar hoy: las entradas del catálogo con `lista`. */
export const INSERTABLES = CATALOGO.flatMap((g) => g.dispositivos.filter((d) => d.lista));

/** La cadena de mastering de fábrica que la tira enseña sobre el máster. */
export const CADENA_MAESTRA = ['EQ Ocho', 'Compresor', 'Techo', 'Medidor'];

/** Búsqueda por nombre en todo el catálogo. */
export function buscarDispositivo(nombre) {
  for (const { grupo, dispositivos } of CATALOGO) {
    const encontrado = dispositivos.find((d) => d.nombre === nombre);
    if (encontrado) return { ...encontrado, grupo };
  }
  return null;
}
