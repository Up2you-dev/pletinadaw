/**
 * El protocolo UI⇄motor, visto desde este lado: líneas JSON (NDJSON) por el
 * stdio del proceso `pletina-motor`. El formato completo está en
 * docs/04-protocolo.md.
 *
 * La lista de métodos vive dos veces —aquí y en motor/src/protocolo.h— y el
 * test de contrato (test/contrato.test.js) comprueba que son idénticas:
 * añadir un método en un solo lado rompe la build, a propósito.
 */

export const METODOS = [
  'hola',
  'dispositivos.listar',
  'dispositivos.tono',
  'vst.carpetas',
  'vst.escanear',
  'vst.lista',
  'previa.tocar',
  'previa.parar',
  'proyecto.nuevo',
  'proyecto.abrir',
  'proyecto.guardar',
  'pistas.listar',
  'pista.crear',
  'pista.borrar',
  'pista.renombrar',
  'pista.mezcla',
  'pista.envio',
  'pista.congelar',
  'pista.armar',
  'clip.importar',
  'clip.mover',
  'clip.recortar',
  'clip.dividir',
  'clip.duplicar',
  'clip.fundidos',
  'clip.borrar',
  'clip.picos',
  'clip.warp',
  'clip.midi.crear',
  'clip.midi.notas',
  'clip.midi.cuantizar',
  'sesion.escenas',
  'sesion.poner',
  'sesion.lanzar',
  'sesion.parar',
  'sesion.cuantizacion',
  'plugin.insertar',
  'plugin.quitar',
  'plugin.parametro',
  'plugin.activar',
  'plugin.lateral',
  'plugin.presets',
  'plugin.preset.guardar',
  'plugin.preset.cargar',
  'automatizacion.puntos',
  'transporte.tocar',
  'transporte.grabar',
  'transporte.parar',
  'transporte.irA',
  'transporte.estado',
  'transporte.tempo',
  'transporte.metronomo',
  'transporte.bucle',
  'deshacer.deshacer',
  'deshacer.rehacer',
  'render.exportar',
  'salir',
];

export const EVENTOS = ['arrancado', 'medidores', 'modelo', 'render.terminado', 'prueba'];

/** Serializa una petición a línea NDJSON. El id lo pone quien lleva la cuenta. */
export function codificarPeticion(id, metodo, params) {
  if (!METODOS.includes(metodo)) throw new Error(`método fuera del protocolo: ${metodo}`);
  const mensaje = params === undefined ? { id, metodo } : { id, metodo, params };
  return `${JSON.stringify(mensaje)}\n`;
}

/**
 * Clasifica una línea recibida del motor. Devuelve `{tipo, ...}` con tipo
 * `respuesta` (con `id` y `resultado` o `error`), `evento` (con `nombre` y
 * `datos`) o `ruido` (una línea que no es del protocolo: se ignora avisando).
 */
export function decodificarLinea(linea) {
  const texto = String(linea).trim();
  if (!texto) return { tipo: 'ruido', linea: texto };

  let mensaje;
  try {
    mensaje = JSON.parse(texto);
  } catch {
    return { tipo: 'ruido', linea: texto };
  }

  if (mensaje && typeof mensaje === 'object' && 'evento' in mensaje) {
    return { tipo: 'evento', nombre: mensaje.evento, datos: mensaje.datos ?? {} };
  }
  if (mensaje && typeof mensaje === 'object' && 'id' in mensaje) {
    return { tipo: 'respuesta', id: mensaje.id, resultado: mensaje.resultado, error: mensaje.error };
  }
  return { tipo: 'ruido', linea: texto };
}

/**
 * Trocea el flujo de stdout en líneas completas. Devuelve las líneas cerradas
 * y el resto pendiente, porque un `data` puede cortar una línea por la mitad.
 */
export function partirLineas(pendiente, trozo) {
  const texto = pendiente + trozo;
  const partes = texto.split('\n');
  const resto = partes.pop();
  return { lineas: partes.filter((l) => l.trim() !== ''), resto };
}
