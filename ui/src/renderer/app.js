import { estado, cambiar, suscribir, posicionParaPintar, aplicarModelo, pistasDeMaqueta } from './estado.js';
import { montarTransporte, pintarBoton, pintarReloj, pintarChipMotor, pintarProyecto } from './ui/transporte.js';
import { montarRail } from './ui/rail.js';
import { montarArreglo, pintar as pintarArreglo, olvidarPicos } from './ui/arreglo.js';
import { montarMesa, pintarMesa, pintarVU } from './ui/mesa.js';
import { montarTira, pintarTira } from './ui/tira.js';
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

async function exportar() {
  if (!conectado()) return soloConMotor();
  const ruta = await puente.dialogos.exportar();
  if (!ruta) return;
  cambiar({ exportando: true });
  try {
    await orden('render.exportar', { ruta });
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
  alCambiarTempo: (bpm) => {
    if (conectado()) orden('transporte.tempo', { bpm }).catch((e) => aviso(e.message));
    else cambiar({ bpm });
  },
});
montarRail();
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
});
pintarChipMotor();

/* ---------- el motor, si está ---------- */

async function sincronizar() {
  try {
    aplicarModelo(await orden('pistas.listar'));
  } catch {
    // el evento modelo llegará solo
  }
}

if (puente) {
  puente.motor.estado().then((m) => {
    cambiar({ motor: m });
    if (m.estado === 'conectado') sincronizar();
    else if (m.estado === 'maqueta') cambiar({ pistas: pistasDeMaqueta(), pistaSeleccionada: 0 });
  });

  puente.motor.alCambiarEstado((m) => {
    const antes = estado.motor.estado;
    cambiar({ motor: m });
    if (m.estado === 'conectado' && antes !== 'conectado') sincronizar();
  });

  puente.motor.alRecibirEvento(({ nombre, datos }) => {
    if (nombre === 'medidores') {
      cambiar({
        segundos: datos.segundos ?? estado.segundos,
        reproduciendo: !!datos.reproduciendo,
        refrescadoEn: performance.now(),
        medidores: {
          izq: datos.izq ?? -100,
          der: datos.der ?? -100,
          pistas: datos.pistas || [],
          lufs: datos.lufs || null,
        },
      });
    } else if (nombre === 'modelo') {
      aplicarModelo(datos);
    } else if (nombre === 'render.terminado') {
      cambiar({ exportando: false });
      aviso(datos.ok ? `Exportado: ${datos.ruta}` : 'El render ha fallado.');
    }
  });
} else {
  cambiar({ pistas: pistasDeMaqueta(), pistaSeleccionada: 0 });
}

/* ---------- repintados por suscripción ---------- */

suscribir((_estado, cambios) => {
  if ('motor' in cambios) pintarChipMotor();
  if ('reproduciendo' in cambios || 'metronomo' in cambios) pintarBoton();
  if ('proyecto' in cambios || 'bpm' in cambios || 'exportando' in cambios) pintarProyecto();
  if ('pistas' in cambios || 'master' in cambios || 'pistaSeleccionada' in cambios) {
    pintarMesa();
    pintarTira();
  }
  if ('proyecto' in cambios || 'pistas' in cambios) montarRail();
});

/* ---------- atajos ---------- */

window.addEventListener('keydown', (evento) => {
  if (evento.target.matches('input, textarea')) return;

  const ctrl = evento.ctrlKey || evento.metaKey;

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
  } else if (evento.key === 'Delete' || evento.key === 'Backspace') {
    borrarSeleccion();
  } else if (evento.key === 't' || evento.key === 'T') {
    dividirEnCursor();
  } else if (evento.key === 'm' || evento.key === 'M') {
    document.getElementById('app').classList.toggle('sin-mesa');
  } else if (evento.key === 'b' || evento.key === 'B') {
    document.getElementById('app').classList.toggle('sin-rail');
  }
});

/* ---------- bucle de pintado ---------- */

function cuadro(ahora) {
  pintarReloj(ahora);
  pintarArreglo(ahora);
  pintarVU();
  requestAnimationFrame(cuadro);
}
requestAnimationFrame(cuadro);
