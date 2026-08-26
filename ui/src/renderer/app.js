import { estado, cambiar, suscribir, posicionParaPintar, aplicarModelo, pistasDeMaqueta } from './estado.js';
import { montarTransporte, pintarBoton, pintarReloj, pintarChipMotor, pintarProyecto } from './ui/transporte.js';
import { montarRail } from './ui/rail.js';
import { montarArreglo, pintar as pintarArreglo, olvidarPicos } from './ui/arreglo.js';
import { montarMesa, pintarMesa, pintarVU } from './ui/mesa.js';
import { montarTira, pintarTira } from './ui/tira.js';
import { montarPiano, pintarPiano } from './ui/piano.js';
import { montarSesion, pintarSesion } from './ui/sesion.js';
import { aviso, $ } from './ui/dom.js';

/**
 * Arranque de la interfaz: montar los paneles, enchufar el motor si está y
 * dejar corriendo el bucle de pintado. Con motor, cada gesto viaja como
 * orden y la réplica vuelve con el evento `modelo`; sin motor, la maqueta
 * permite recorrerlo todo y avisa de lo que necesita motor.
 */

const puente = window.pletinadaw;
document.documentElement.dataset.platform = puente?.plataforma || 'web';

const conectado = () => estado.motor.estado === 'conectado';

async function orden(metodo, params) {
  return puente.motor.orden(metodo, params);
}

function soloConMotor() {
  aviso('Esto necesita el motor: compílalo en motor/ o revisa el chip de estado.');
}

/* ---------- transporte ---------- */

async function conmutar() {
  if (conectado()) {
    try {
      const r = await orden(estado.reproduciendo ? 'transporte.parar' : 'transporte.tocar');
      cambiar({ reproduciendo: !!r.reproduciendo, segundos: r.segundos ?? estado.segundos, refrescadoEn: performance.now() });
    } catch (error) {
      aviso(`El motor no responde: ${error.message}`);
    }
  } else {
    cambiar({ segundos: posicionParaPintar(), reproduciendo: !estado.reproduciendo, refrescadoEn: performance.now() });
  }
  pintarBoton();
}

async function grabar(conCuenta) {
  if (!conectado()) return soloConMotor();
  if (estado.grabando) return conmutar(); // grabando, el mismo botón para = parar
  if (!estado.pistas.some((p) => p.armada)) {
    return aviso('Arma primero una pista con su botón ● de la mesa.');
  }
  try {
    const r = await orden('transporte.grabar', conCuenta ? { cuenta: true } : {});
    cambiar({ reproduciendo: !!r.reproduciendo, grabando: !!r.grabando,
              segundos: r.segundos ?? estado.segundos, refrescadoEn: performance.now() });
  } catch (error) {
    aviso(error.message);
  }
  pintarBoton();
}

async function irA(segundos) {
  if (conectado()) {
    try {
      const r = await orden('transporte.irA', { segundos });
      cambiar({ segundos: r.segundos ?? segundos, refrescadoEn: performance.now() });
      return;
    } catch (error) {
      aviso(`El motor no responde: ${error.message}`);
    }
  }
  cambiar({ segundos, refrescadoEn: performance.now() });
}

/* ---------- proyecto ---------- */

async function nuevoProyecto() {
  if (!conectado()) return soloConMotor();
  const carpeta = await puente.dialogos.nuevoProyecto();
  if (!carpeta) return;
  try {
    await orden('proyecto.nuevo', { carpeta });
    olvidarPicos();
    aviso(`Proyecto creado en ${carpeta}`);
  } catch (error) {
    aviso(error.message);
  }
}

async function abrirProyecto() {
  if (!conectado()) return soloConMotor();
  const ruta = await puente.dialogos.abrirProyecto();
  if (!ruta) return;
  try {
    await orden('proyecto.abrir', { ruta });
    olvidarPicos();
  } catch (error) {
    aviso(error.message);
  }
}

async function guardar() {
  if (!conectado()) return soloConMotor();
  try {
    if (!estado.proyecto.ruta) return nuevoProyecto();
    await orden('proyecto.guardar');
    aviso('Guardado.');
  } catch (error) {
    aviso(error.message);
  }
}

async function exportar(opciones = {}) {
  if (!conectado()) return soloConMotor();
  const ruta = await puente.dialogos.exportar();
  if (!ruta) return;
  cambiar({ exportando: true });
  try {
    await orden('render.exportar', { ruta, ...opciones });
  } catch (error) {
    aviso(`No se pudo exportar: ${error.message}`);
    cambiar({ exportando: false });
  }
}

/* ---------- clips ---------- */

async function importarRutas(rutas, fila, inicio) {
  if (!conectado()) return soloConMotor();
  let donde = inicio;
  for (const ruta of rutas) {
    try {
      const r = await orden('clip.importar', { pista: fila, ruta, inicio: donde });
      donde += r.duracion ?? 0;
    } catch (error) {
      aviso(`${ruta}: ${error.message}`);
    }
  }
}

async function importarDialogo() {
  if (!conectado()) return soloConMotor();
  const rutas = await puente.dialogos.importarAudio();
  if (!rutas.length) return;
  const fila = Math.max(0, estado.pistaSeleccionada);
  importarRutas(rutas, fila, posicionParaPintar());
}

function moverLocal(clip, inicio, filaDestino, filaOrigen) {
  // La maqueta también edita: se toca la réplica directamente.
  const pistas = estado.pistas.map((p) => ({ ...p, clips: [...p.clips] }));
  const origen = pistas[filaOrigen];
  const i = origen.clips.findIndex((c) => c.id === clip.id);
  if (i < 0) return;
  const [movido] = origen.clips.splice(i, 1);
  movido.inicio = inicio;
  (filaDestino >= 0 ? pistas[filaDestino] : origen).clips.push(movido);
  cambiar({ pistas });
}

const accionesArreglo = {
  alIrA: irA,

  alMoverClip(clip, inicio, filaDestino, filaOrigen) {
    moverLocal(clip, inicio, filaDestino, filaOrigen);
    if (conectado()) {
      const params = { id: clip.id, inicio };
      if (filaDestino >= 0) params.pista = filaDestino;
      orden('clip.mover', params).catch((e) => aviso(e.message));
    }
  },

  alRecortarClip(clip, inicio, fin, fila) {
    const pistas = estado.pistas.map((p) => ({ ...p, clips: p.clips.map((c) => ({ ...c })) }));
    const local = pistas[fila]?.clips.find((c) => c.id === clip.id);
    if (local) {
      local.desfase = (local.desfase || 0) + (inicio - local.inicio);
      local.inicio = inicio;
      local.duracion = fin - inicio;
      cambiar({ pistas });
    }
    if (conectado()) orden('clip.recortar', { id: clip.id, inicio, fin }).catch((e) => aviso(e.message));
  },

  alImportar(archivos, fila, inicio) {
    if (!conectado()) return soloConMotor();
    const rutas = archivos.map((f) => puente.rutaDe(f)).filter(Boolean);
    importarRutas(rutas, fila, inicio);
  },

  alPedirPicos(clip) {
    if (!conectado()) return Promise.resolve(null);
    return orden('clip.picos', { id: clip.id, porSegundo: 50 });
  },

  alSeleccionarPista(fila) {
    cambiar({ pistaSeleccionada: fila });
  },

  alFundidos(clip, entrada, salida) {
    if (conectado()) orden('clip.fundidos', { id: clip.id, entrada, salida }).catch((e) => aviso(e.message));
    else {
      const pistas = estado.pistas.map((p) => ({
        ...p,
        clips: p.clips.map((c) => (c.id === clip.id ? { ...c, entradaFundido: entrada, salidaFundido: salida } : c)),
      }));
      cambiar({ pistas });
    }
  },

  alBucle(activo, inicio, fin) {
    cambiar({ bucle: { activo, inicio, fin } });
    if (conectado()) orden('transporte.bucle', { activo, inicio, fin }).catch((e) => aviso(e.message));
  },

  alAutomatizar(fila, puntos) {
    // Optimista en la réplica; el motor confirma con su evento modelo.
    const pistas = estado.pistas.map((p, i) => (i === fila ? { ...p, automatizacionVolumen: puntos } : p));
    cambiar({ pistas });
    if (conectado()) orden('automatizacion.puntos', { pista: fila, parametro: 'volumen', puntos }).catch((e) => aviso(e.message));
  },

  alAbrirPianoRoll(id) {
    cambiar({ pianoRoll: id });
  },

  alCrearClipMidi(fila, inicio) {
    if (!conectado()) return soloConMotor();
    orden('clip.midi.crear', { pista: fila, inicio, compases: 1 })
      .then((r) => cambiar({ pianoRoll: r.id, seleccion: new Set([r.id]) }))
      .catch((e) => aviso(e.message));
  },

  alRenombrarPista(fila, nombreActual, caja) {
    const entrada = document.createElement('input');
    entrada.className = 'renombrar';
    entrada.value = nombreActual;
    Object.assign(entrada.style, { left: `${caja.x}px`, top: `${caja.y}px`, width: `${caja.ancho}px` });
    $('#lienzo-wrap').appendChild(entrada);
    entrada.focus();
    entrada.select();

    const terminar = (confirmar) => {
      const nombre = entrada.value.trim();
      entrada.remove();
      if (!confirmar || !nombre || nombre === nombreActual) return;
      if (conectado()) orden('pista.renombrar', { pista: fila, nombre }).catch((e) => aviso(e.message));
      else {
        const pistas = estado.pistas.map((p, i) => (i === fila ? { ...p, nombre } : p));
        cambiar({ pistas });
      }
    };
    entrada.addEventListener('keydown', (e) => {
      if (e.key === 'Enter') terminar(true);
      if (e.key === 'Escape') terminar(false);
      e.stopPropagation();
    });
    entrada.addEventListener('blur', () => terminar(true));
  },
};

async function borrarSeleccion() {
  if (!estado.seleccion.size) return;
  if (!conectado()) return soloConMotor();
  for (const id of [...estado.seleccion]) {
    try {
      await orden('clip.borrar', { id });
    } catch (error) {
      aviso(error.message);
    }
  }
}

async function dividirEnCursor() {
  if (!conectado()) return soloConMotor();
  const s = posicionParaPintar();
  const objetivos = [];
  for (const pista of estado.pistas)
    for (const clip of pista.clips)
      if (estado.seleccion.has(clip.id) && s > clip.inicio && s < clip.inicio + clip.duracion)
        objetivos.push(clip.id);

  if (!objetivos.length) return aviso('Selecciona un clip que pase por el cursor para dividirlo (T).');
  for (const id of objetivos) {
    try {
      await orden('clip.dividir', { id, segundos: s });
    } catch (error) {
      aviso(error.message);
    }
  }
}

/* ---------- montar paneles ---------- */

montarTransporte({
  alConmutar: conmutar,
  alGrabar: grabar,
  alIrA: irA,
  alNuevoProyecto: nuevoProyecto,
  alAbrirProyecto: abrirProyecto,
  alGuardar: guardar,
  alImportarDialogo: importarDialogo,
  alExportar: exportar,
  alMetronomo: () => {
    if (!conectado()) return soloConMotor();
    orden('transporte.metronomo', { activo: !estado.metronomo }).catch((e) => aviso(e.message));
  },
  alCiclo: () => {
    const { bucle } = estado;
    if (bucle.fin - bucle.inicio < 0.01) return aviso('Dibuja primero el bucle arrastrando por la mitad alta de la regla.');
    accionesArreglo.alBucle(!bucle.activo, bucle.inicio, bucle.fin);
  },
  alCambiarTempo: (bpm) => {
    if (conectado()) orden('transporte.tempo', { bpm }).catch((e) => aviso(e.message));
    else cambiar({ bpm });
  },
  alVistaSesion: () => {
    const abrir = !estado.vistaSesion;
    cambiar({ vistaSesion: abrir });
    // La primera vez, cuatro escenas de cortesía: una rejilla vacía no invita.
    if (abrir && !estado.escenas && conectado()) {
      orden('sesion.escenas', { numero: 4 }).catch((e) => aviso(e.message));
    }
  },
  alTema: () => conmutarTema(),
  alAyuda: () => conmutarAyuda(),
});
montarRail({
  alElegirCarpetaSonidos: async () => {
    if (!puente) return null;
    return puente.dialogos.carpetaSonidos();
  },
  alImportarSonido: (ruta) => {
    if (!conectado()) return soloConMotor();
    const fila = estado.pistaSeleccionada >= 0 ? estado.pistaSeleccionada : 0;
    orden('clip.importar', { pista: fila, ruta, inicio: posicionParaPintar() })
      .catch((e) => aviso(e.message));
  },
  alPrevia: async (ruta) => {
    if (!conectado()) return soloConMotor();
    try {
      await orden('previa.tocar', { ruta });
    } catch (error) {
      aviso(error.message);
    }
  },
  alPararPrevia: () => {
    if (conectado()) orden('previa.parar').catch(() => {});
  },
});
montarArreglo(accionesArreglo);
montarMesa({
  alSeleccionarPista: (fila) => cambiar({ pistaSeleccionada: fila }),
  alMezclar: (fila, params) => {
    if (conectado()) orden('pista.mezcla', { pista: fila, ...params }).catch(() => {});
  },
  alCrearPista: () => {
    if (!conectado()) return soloConMotor();
    orden('pista.crear').catch((e) => aviso(e.message));
  },
  alEnviar: (fila, bus, nivelDb) => {
    if (!conectado()) return soloConMotor();
    orden('pista.envio', { pista: fila, bus, nivelDb: nivelDb <= -59.5 ? -100 : nivelDb }).catch((e) => aviso(e.message));
  },
  alCongelar: (fila, activo) => {
    if (!conectado()) return soloConMotor();
    orden('pista.congelar', { pista: fila, activo }).catch((e) => aviso(e.message));
  },
  alArmar: (fila, activo) => {
    if (!conectado()) return soloConMotor();
    orden('pista.armar', { pista: fila, activo }).catch((e) => aviso(e.message));
  },
  alCrearGrupo: (pistas) => {
    if (!conectado()) return soloConMotor();
    orden('grupo.crear', { pistas }).catch((e) => aviso(e.message));
  },
  alMeterEnGrupo: (pista, grupo) => {
    if (!conectado()) return soloConMotor();
    orden('grupo.meter', { pista, grupo }).catch((e) => aviso(e.message));
  },
  alSacarDeGrupo: (pista) => {
    if (!conectado()) return soloConMotor();
    orden('grupo.sacar', { pista }).catch((e) => aviso(e.message));
  },
  alDeshacerGrupo: (grupo) => {
    if (!conectado()) return soloConMotor();
    orden('grupo.deshacer', { grupo }).catch((e) => aviso(e.message));
  },
});
montarTira({
  alInsertarPlugin: (pista, tipo) => {
    if (!conectado()) return soloConMotor();
    orden('plugin.insertar', { pista, tipo }).catch((e) => aviso(e.message));
  },
  alQuitarPlugin: (pista, indice) => {
    if (!conectado()) return soloConMotor();
    orden('plugin.quitar', { pista, indice }).catch((e) => aviso(e.message));
  },
  alParametro: (pista, indice, parametro, valor) => {
    if (conectado()) orden('plugin.parametro', { pista, indice, parametro, valor }).catch(() => {});
  },
  alActivarPlugin: (pista, indice, activo) => {
    if (!conectado()) return soloConMotor();
    orden('plugin.activar', { pista, indice, activo }).catch((e) => aviso(e.message));
  },
  alListarPresets: async (tipo) => {
    if (!conectado()) return [];
    try {
      return (await orden('plugin.presets', { tipo })).presets || [];
    } catch {
      return [];
    }
  },
  alCargarPreset: (pista, indice, nombre) => {
    if (conectado()) orden('plugin.preset.cargar', { pista, indice, nombre }).catch((e) => aviso(e.message));
  },
  alGuardarPreset: (pista, indice, nombre) => {
    if (conectado()) orden('plugin.preset.guardar', { pista, indice, nombre })
      .then(() => aviso(`Preset «${nombre}» guardado.`))
      .catch((e) => aviso(e.message));
  },
  alWarp: (id, params) => {
    if (!conectado()) return soloConMotor();
    orden('clip.warp', { id, ...params }).catch((e) => aviso(e.message));
  },
  alAbrirPianoRoll: (id) => cambiar({ pianoRoll: id }),
  alLateral: (pista, indice, fuente) => {
    if (!conectado()) return soloConMotor();
    orden('plugin.lateral', { pista, indice, fuente }).catch((e) => aviso(e.message));
  },
  alEscanearVst: async () => {
    if (!conectado()) return soloConMotor();
    aviso('Buscando VST3 en las carpetas del sistema…');
    try {
      const r = await orden('vst.escanear');
      cambiar({ vst: (await orden('vst.lista')).plugins || [] });
      aviso(`VST3: ${r.total} en el catálogo (${r.nuevos} nuevos, ${r.vetados} vetados).`);
    } catch (error) {
      aviso(error.message);
    }
  },
  alCrearRack: (pista) => {
    if (!conectado()) return soloConMotor();
    orden('rack.crear', { pista }).catch((e) => aviso(e.message));
  },
  alDeshacerRack: (pista, indice) => {
    if (!conectado()) return soloConMotor();
    orden('rack.deshacer', { pista, indice }).catch((e) => aviso(e.message));
  },
  alMacro: (pista, indice, macro, valor) => {
    if (conectado()) orden('rack.macro', { pista, indice, macro, valor }).catch(() => {});
  },
  alNombrarMacro: (pista, indice, macro, nombre) => {
    if (!conectado()) return soloConMotor();
    orden('rack.macro', { pista, indice, macro, nombre }).catch((e) => aviso(e.message));
  },
  alAsignarMacro: (pista, indice, macro, plugin, parametro, quitar) => {
    if (!conectado()) return soloConMotor();
    const params = { pista, indice, macro, plugin, parametro };
    if (quitar) params.quitar = true;
    orden('rack.asignar', params).catch((e) => aviso(e.message));
  },
});
montarSesion({
  alLanzarSesion: (pista, escena) => {
    if (!conectado()) return soloConMotor();
    const params = { escena };
    if (pista >= 0) params.pista = pista;
    orden('sesion.lanzar', params).catch((e) => aviso(e.message));
  },
  alPararSesion: (pista) => {
    if (!conectado()) return soloConMotor();
    orden('sesion.parar', pista >= 0 ? { pista } : {}).catch((e) => aviso(e.message));
  },
  alPonerEnSesion: (pista, escena, desdeClip) => {
    if (!conectado()) return soloConMotor();
    orden('sesion.poner', { pista, escena, desdeClip }).catch((e) => aviso(e.message));
  },
  alEscenas: (numero) => {
    if (!conectado()) return soloConMotor();
    orden('sesion.escenas', { numero }).catch((e) => aviso(e.message));
  },
  alCuantizacionSesion: (nombre) => {
    if (conectado()) orden('sesion.cuantizacion', { nombre }).catch((e) => aviso(e.message));
  },
});
montarPiano({
  alNotasMidi: (id, notas) => {
    // Optimista sobre la réplica: el piano roll no parpadea mientras editas.
    const pistas = estado.pistas.map((p) => ({
      ...p,
      clips: (p.clips || []).map((c) => (c.id === id ? { ...c, notas } : c)),
    }));
    cambiar({ pistas });
    if (conectado()) orden('clip.midi.notas', { id, notas }).catch((e) => aviso(e.message));
  },
  alCuantizar: (id, division) => {
    if (!conectado()) return soloConMotor();
    orden('clip.midi.cuantizar', { id, division }).catch((e) => aviso(e.message));
  },
});
pintarChipMotor();

/* ---------- el motor, si está ---------- */

async function sincronizar() {
  try {
    aplicarModelo(await orden('pistas.listar'));
  } catch {
    // el evento modelo llegará solo
  }
  try {
    cambiar({ vst: (await orden('vst.lista')).plugins || [] });
  } catch {
    // motor viejo sin VST: la tira simplemente no los enseña
  }
}

/** El último proyecto con carpeta, para reabrirlo tras un arranque o una caída. */
const ultimoProyecto = {
  leer() {
    try { return localStorage.getItem('pletina-ultimo-proyecto') || ''; } catch { return ''; }
  },
  guardar(ruta) {
    try {
      if (ruta) localStorage.setItem('pletina-ultimo-proyecto', ruta);
    } catch { /* sin almacenamiento, sin drama */ }
  },
};

/**
 * Recuperación: si el motor vuelve (arranque o caída) y había un proyecto,
 * se reabre solo. Tras una caída manda la réplica (lo que estabas tocando);
 * en un arranque limpio, el último proyecto recordado. El autoguardado de
 * cada dos minutos hace que lo perdido sea, como mucho, eso.
 */
let recuperacionInicialHecha = false;

async function reabrirSiToca(traumatico) {
  if (!traumatico && recuperacionInicialHecha) return false;
  recuperacionInicialHecha = true;
  const ruta = estado.proyecto.ruta || (traumatico ? '' : ultimoProyecto.leer());
  if (!ruta) return false;
  try {
    aplicarModelo(await orden('proyecto.abrir', { ruta }));
    if (traumatico) aviso('El motor se ha recuperado: proyecto reabierto (autoguardado de hace 2 min como mucho).');
    return true;
  } catch {
    if (traumatico) aviso('El motor se ha recuperado, pero el proyecto no se pudo reabrir.');
    return false;
  }
}

if (puente) {
  puente.motor.estado().then(async (m) => {
    cambiar({ motor: m });
    if (m.estado === 'conectado') {
      await reabrirSiToca(false);
      sincronizar();
    } else if (m.estado === 'maqueta') {
      cambiar({ pistas: pistasDeMaqueta(), pistaSeleccionada: 0 });
    }
  });

  puente.motor.alCambiarEstado(async (m) => {
    const antes = estado.motor.estado;
    cambiar({ motor: m });
    if (m.estado === 'conectado' && antes !== 'conectado') {
      // Venir de 'caido' o 'arrancando' con réplica viva = caída en caliente.
      await reabrirSiToca(antes === 'caido' || (antes === 'arrancando' && !!estado.proyecto.ruta));
      sincronizar();
    }
  });

  puente.motor.alRecibirEvento(({ nombre, datos }) => {
    if (nombre === 'medidores') {
      cambiar({
        segundos: datos.segundos ?? estado.segundos,
        reproduciendo: !!datos.reproduciendo,
        grabando: !!datos.grabando,
        refrescadoEn: performance.now(),
        medidores: {
          izq: datos.izq ?? -100,
          der: datos.der ?? -100,
          pistas: datos.pistas || [],
          grupos: datos.grupos || [],
          lufs: datos.lufs || null,
          espectro: datos.espectro || null,
          xy: datos.xy || null,
          sesion: datos.sesion || null,
        },
      });
    } else if (nombre === 'modelo') {
      aplicarModelo(datos);
    } else if (nombre === 'render.terminado') {
      cambiar({ exportando: false });
      aviso(datos.ok ? `Exportado: ${datos.ruta}` : 'El render ha fallado.');
    }
  });
}

if (new URLSearchParams(window.location.search).get('vista') === 'sesion') {
  cambiar({ vistaSesion: true });
}

if (!puente) {
  cambiar({ pistas: pistasDeMaqueta(), pistaSeleccionada: 0 });
}

/* ---------- repintados por suscripción ---------- */

suscribir((_estado, cambios) => {
  if ('motor' in cambios) pintarChipMotor();
  if ('proyecto' in cambios && estado.proyecto.ruta) ultimoProyecto.guardar(estado.proyecto.ruta);
  if ('reproduciendo' in cambios || 'metronomo' in cambios || 'grabando' in cambios) pintarBoton();
  if ('proyecto' in cambios || 'bpm' in cambios || 'exportando' in cambios) pintarProyecto();
  if ('pistas' in cambios || 'grupos' in cambios || 'master' in cambios || 'pistaSeleccionada' in cambios) {
    pintarMesa();
    pintarTira();
  } else if ('seleccion' in cambios || 'vst' in cambios) {
    pintarTira(); // el inspector de clip y el catálogo VST viven en la tira
  }
  if ('pianoRoll' in cambios || 'pistas' in cambios) pintarPiano();
  if ('vistaSesion' in cambios) pintarSesion(true);
  if ('proyecto' in cambios || 'pistas' in cambios) montarRail();
});

/* ---------- tema y ayuda ---------- */

// El tema claro existe y se elige; el oscuro es la casa. Se recuerda.
try {
  const tema = localStorage.getItem('pletina-tema');
  if (tema) document.documentElement.dataset.theme = tema;
} catch { /* sin almacenamiento, sin drama */ }

function conmutarTema() {
  const claro = document.documentElement.dataset.theme === 'light';
  const nuevo = claro ? 'dark' : 'light';
  document.documentElement.dataset.theme = nuevo;
  try { localStorage.setItem('pletina-tema', nuevo); } catch { /* da igual */ }
}

const ATAJOS = [
  ['Espacio', 'tocar / parar'],
  ['R · Mayús+R', 'grabar en las pistas armadas · con claqueta'],
  ['Inicio', 'ir al principio'],
  ['T', 'dividir el clip bajo el cursor'],
  ['Supr', 'borrar la selección'],
  ['Ctrl+Z · Ctrl+Y', 'deshacer · rehacer'],
  ['Ctrl+S', 'guardar el proyecto'],
  ['Ctrl+D', 'duplicar los clips seleccionados'],
  ['A', 'automatización de volumen (dibujar en la pista)'],
  ['Tab', 'Session View'],
  ['1–8 · 0', 'en sesión: lanzar escena · parar todo'],
  ['M · B', 'plegar la mesa · el rail'],
  ['Ctrl+rueda', 'zoom del arreglo'],
  ['Doble clic en el vacío', 'crear un clip MIDI'],
  ['Doble clic en un clip MIDI', 'abrir el piano roll'],
  ['Esc', 'cerrar el piano roll o esta ayuda'],
  ['?', 'esta ayuda'],
];

/** Markdown mínimo y seguro para el manual: títulos, listas, negrita y código. */
function mdAHtml(md) {
  const escapa = (t) => t.replace(/&/g, '&amp;').replace(/</g, '&lt;').replace(/>/g, '&gt;');
  const linea = (t) => escapa(t)
    .replace(/\*\*([^*]+)\*\*/g, '<strong>$1</strong>')
    .replace(/`([^`]+)`/g, '<code>$1</code>')
    .replace(/\[([^\]]+)\]\([^)]+\)/g, '$1');
  const bloques = [];
  let lista = null;
  for (const cruda of md.split('\n')) {
    const l = cruda.trimEnd();
    const esItem = /^\s*(?:[-*]|\d+\.)\s+/.test(l);
    if (!esItem && lista) { bloques.push(`<ul>${lista.join('')}</ul>`); lista = null; }
    if (/^#{1,3}\s/.test(l)) {
      const nivel = l.match(/^#+/)[0].length;
      bloques.push(`<h${nivel + 1}>${linea(l.replace(/^#+\s*/, ''))}</h${nivel + 1}>`);
    } else if (esItem) {
      (lista ??= []).push(`<li>${linea(l.replace(/^\s*(?:[-*]|\d+\.)\s+/, ''))}</li>`);
    } else if (l.trim()) {
      bloques.push(`<p>${linea(l)}</p>`);
    }
  }
  if (lista) bloques.push(`<ul>${lista.join('')}</ul>`);
  return bloques.join('');
}

function conmutarAyuda() {
  const previa = $('#ayuda-atajos');
  if (previa) return previa.remove();
  const capa = document.createElement('div');
  capa.id = 'ayuda-atajos';
  capa.className = 'ayuda-atajos';
  capa.innerHTML = `
    <div class="ayuda-caja">
      <div class="ayuda-cabeza">
        <span>Ayuda de Pletina DAW</span>
        <button class="ver-manual">Manual</button>
        <button class="cerrar">✕</button>
      </div>
      <dl>${ATAJOS.map(([tecla, que]) => `<dt>${tecla}</dt><dd>${que}</dd>`).join('')}</dl>
      <div class="manual" hidden></div>
    </div>`;
  capa.addEventListener('click', (e) => { if (e.target === capa || e.target.matches('.cerrar')) capa.remove(); });
  capa.querySelector('.ver-manual').addEventListener('click', async (e) => {
    const manual = capa.querySelector('.manual');
    const atajos = capa.querySelector('dl');
    if (!manual.hidden) { manual.hidden = true; atajos.hidden = false; e.target.textContent = 'Manual'; return; }
    if (!manual.innerHTML) {
      const md = puente ? await puente.manual() : null;
      manual.innerHTML = md ? mdAHtml(md) : '<p>El manual vive en docs/manual.md del repositorio.</p>';
    }
    manual.hidden = false;
    atajos.hidden = true;
    e.target.textContent = 'Atajos';
  });
  document.body.appendChild(capa);
}

/* ---------- atajos ---------- */

window.addEventListener('keydown', (evento) => {
  if (evento.target.matches('input, textarea, select')) return;

  const ctrl = evento.ctrlKey || evento.metaKey;

  // Con el piano roll abierto los atajos de edición del arrangement callan.
  if (estado.pianoRoll !== null) {
    if (evento.key === 'Escape') cambiar({ pianoRoll: null });
    else if (evento.code === 'Space') { evento.preventDefault(); conmutar(); }
    else if (ctrl && evento.key.toLowerCase() === 'z') {
      evento.preventDefault();
      if (conectado()) orden(evento.shiftKey ? 'deshacer.rehacer' : 'deshacer.deshacer').catch(() => {});
    }
    return;
  }

  if (ctrl && evento.key.toLowerCase() === 'z') {
    evento.preventDefault();
    if (!conectado()) return soloConMotor();
    orden(evento.shiftKey ? 'deshacer.rehacer' : 'deshacer.deshacer').catch(() => {});
  } else if (ctrl && evento.key.toLowerCase() === 'y') {
    evento.preventDefault();
    if (conectado()) orden('deshacer.rehacer').catch(() => {});
  } else if (ctrl && evento.key.toLowerCase() === 's') {
    evento.preventDefault();
    guardar();
  } else if (evento.code === 'Space') {
    evento.preventDefault();
    conmutar();
  } else if (evento.code === 'Home') {
    irA(0);
  } else if (ctrl && evento.key.toLowerCase() === 'd') {
    evento.preventDefault();
    if (!conectado()) return soloConMotor();
    for (const id of [...estado.seleccion]) orden('clip.duplicar', { id }).catch((e) => aviso(e.message));
  } else if (evento.key === 'Delete' || evento.key === 'Backspace') {
    borrarSeleccion();
  } else if (evento.key === 't' || evento.key === 'T') {
    dividirEnCursor();
  } else if (evento.key === 'r' || evento.key === 'R') {
    grabar(evento.shiftKey);
  } else if (evento.key === 'a' || evento.key === 'A') {
    cambiar({ automatizando: !estado.automatizando });
    aviso(estado.automatizando
      ? 'Automatización de volumen: clic añade punto, arrastra mueve, Alt+clic quita (A para salir).'
      : 'Automatización desactivada.');
  } else if (evento.key === 'm' || evento.key === 'M') {
    document.getElementById('app').classList.toggle('sin-mesa');
  } else if (evento.key === 'b' || evento.key === 'B') {
    document.getElementById('app').classList.toggle('sin-rail');
  } else if (evento.key === 'Tab') {
    evento.preventDefault();
    $('#b-sesion')?.click();
  } else if (estado.vistaSesion && /^[1-8]$/.test(evento.key)) {
    // En la Session View, 1..8 lanzan la escena y 0 lo para todo.
    if (!conectado()) return soloConMotor();
    orden('sesion.lanzar', { escena: Number(evento.key) - 1 }).catch((e) => aviso(e.message));
  } else if (estado.vistaSesion && evento.key === '0') {
    if (conectado()) orden('sesion.parar', {}).catch(() => {});
  } else if (evento.key === '?') {
    conmutarAyuda();
  } else if (evento.key === 'Escape') {
    $('#ayuda-atajos')?.remove();
  }
});

/* ---------- bucle de pintado ---------- */

function cuadro(ahora) {
  pintarReloj(ahora);
  pintarArreglo(ahora);
  pintarVU();
  pintarSesion();   // se guarda solo: repinta únicamente si algo visible cambió
  requestAnimationFrame(cuadro);
}
requestAnimationFrame(cuadro);
