import { $ } from './dom.js';
import { estado, cambiar, posicionParaPintar } from '../estado.js';
import { segundosPorCompas, imanASubdivision } from '../../shared/tiempo.js';
import { recortarPicos, claveDePicos } from '../../shared/ondas.js';

/**
 * El arrangement: un solo canvas pinta cabeceras, regla, rejilla, clips con
 * su onda real y el cursor; y encima se edita: arrastrar mueve, los bordes
 * recortan, la regla busca, y soltar archivos importa. Cada gesto termina en
 * una orden al motor; el evento `modelo` que vuelve deja la réplica fina.
 */

const CABECERA_W = 150;
const REGLA_H = 30;
const BORDE_PX = 7;

let canvas, ctx, wrap;
let ancho = 0, alto = 0, dpr = 1;
let acciones = null;   // las inyecta app.js: {alIrA, alMoverClip, alRecortarClip, alSeleccionarPista, alImportar, alRenombrarPista}

const css = (variable) => getComputedStyle(document.documentElement).getPropertyValue(variable).trim();
let color = {};

function refrescarPaleta() {
  color = {
    fondo: css('--bg'), panel: css('--panel'), panel2: css('--panel-2'),
    linea: css('--line'), lineaSuave: css('--line-soft'),
    texto: css('--text'), apagado: css('--muted'), apagado2: css('--muted-2'),
    acento: css('--accent'), senal: css('--signal'),
  };
}

/* ---------- picos reales, con caché por archivo fuente ---------- */

const cachePicos = new Map();
let pedirPicosAlMotor = null; // la inyecta app.js

function picosDe(clip) {
  if (!clip.ruta) return null;
  const clave = claveDePicos(clip);
  const entrada = cachePicos.get(clave);
  if (entrada?.picos) return entrada;

  if (!entrada && pedirPicosAlMotor) {
    cachePicos.set(clave, { pidiendo: true });
    pedirPicosAlMotor(clip).then((r) => {
      if (r) cachePicos.set(clave, { porSegundo: r.porSegundo, picos: r.picos });
    }).catch(() => cachePicos.delete(clave));
  }
  return null;
}

export function olvidarPicos() {
  cachePicos.clear();
}

/* ---------- geometría ---------- */

const altoPistaActual = () => {
  const filas = Math.max(1, estado.pistas.length);
  return Math.max(56, Math.floor((alto - REGLA_H) / Math.max(filas, 4)));
};

const xDeSegundos = (s) => CABECERA_W + (s - estado.desplazamiento) * estado.pxPorSegundo;
const segundosDeX = (x) => estado.desplazamiento + (x - CABECERA_W) / estado.pxPorSegundo;

function subdivisionPorZoom() {
  const pxCompas = segundosPorCompas(estado.bpm, estado.compas[0]) * estado.pxPorSegundo;
  return pxCompas > 220 ? 16 : pxCompas > 110 ? 8 : 4;
}

function iman(segundos, sinIman) {
  if (sinIman) return Math.max(0, segundos);
  return imanASubdivision(segundos, estado.bpm, subdivisionPorZoom(), estado.compas[0]);
}

function encontrar(x, y) {
  if (y <= REGLA_H) return { zona: x < CABECERA_W ? 'esquina' : 'regla' };

  const fila = Math.floor((y - REGLA_H) / altoPistaActual());
  const pista = estado.pistas[fila];
  if (!pista) return { zona: 'vacio' };
  if (x < CABECERA_W) return { zona: 'cabecera', fila, pista };

  const s = segundosDeX(x);
  const yEnFila = (y - REGLA_H) % altoPistaActual();
  for (const clip of pista.clips) {
    if (s < clip.inicio || s > clip.inicio + clip.duracion) continue;
    const x0 = xDeSegundos(clip.inicio);
    const x1 = xDeSegundos(clip.inicio + clip.duracion);

    // Las asas de fundido viven en la franja alta del clip.
    if (yEnFila <= 20) {
      const hIzq = x0 + (clip.entradaFundido || 0) * estado.pxPorSegundo;
      const hDer = x1 - (clip.salidaFundido || 0) * estado.pxPorSegundo;
      if (Math.abs(x - hIzq) <= 7) return { zona: 'fundidoIzq', fila, pista, clip };
      if (Math.abs(x - hDer) <= 7) return { zona: 'fundidoDer', fila, pista, clip };
    }

    if (x - x0 <= BORDE_PX) return { zona: 'bordeIzq', fila, pista, clip };
    if (x1 - x <= BORDE_PX) return { zona: 'bordeDer', fila, pista, clip };
    return { zona: 'clip', fila, pista, clip };
  }
  return { zona: 'carril', fila, pista };
}

/* ---------- gesto en curso ---------- */

let gesto = null; // {tipo, clip, filaOrigen, filaDestino, sX, inicio0, fin0, inicioNuevo, finNuevo, movido}

function previsualizacion(clip) {
  if (gesto && gesto.clip && gesto.clip.id === clip.id) {
    return {
      inicio: gesto.inicioNuevo ?? clip.inicio,
      duracion: (gesto.finNuevo ?? clip.inicio + clip.duracion) - (gesto.inicioNuevo ?? clip.inicio),
      fila: gesto.filaDestino,
    };
  }
  return null;
}

/* ---------- montaje ---------- */

export function montarArreglo(inyectadas) {
  acciones = inyectadas;
  pedirPicosAlMotor = inyectadas.alPedirPicos;

  wrap = $('#lienzo-wrap');
  canvas = $('#arreglo');
  ctx = canvas.getContext('2d');
  refrescarPaleta();

  const observador = new ResizeObserver(() => {
    dpr = window.devicePixelRatio || 1;
    ancho = wrap.clientWidth;
    alto = wrap.clientHeight;
    canvas.width = Math.round(ancho * dpr);
    canvas.height = Math.round(alto * dpr);
    pintar(performance.now());
  });
  observador.observe(wrap);

  canvas.addEventListener('pointerdown', bajar);
  canvas.addEventListener('pointermove', mover);
  canvas.addEventListener('pointerup', soltar);
  canvas.addEventListener('pointercancel', () => { gesto = null; });
  canvas.addEventListener('dblclick', dobleClic);

  // Rueda: desplazar; con Ctrl, zoom anclado al puntero.
  canvas.addEventListener('wheel', (evento) => {
    evento.preventDefault();
    if (evento.ctrlKey || evento.metaKey) {
      const rect = canvas.getBoundingClientRect();
      const x = Math.max(CABECERA_W, evento.clientX - rect.left) - CABECERA_W;
      const ancla = estado.desplazamiento + x / estado.pxPorSegundo;
      const factor = evento.deltaY < 0 ? 1.18 : 1 / 1.18;
      const pxPorSegundo = Math.min(600, Math.max(8, estado.pxPorSegundo * factor));
      cambiar({ pxPorSegundo, desplazamiento: Math.max(0, ancla - x / pxPorSegundo) });
    } else {
      const delta = (evento.deltaY || evento.deltaX) / estado.pxPorSegundo;
      cambiar({ desplazamiento: Math.max(0, estado.desplazamiento + delta) });
    }
  }, { passive: false });

  // Soltar archivos: importa en la pista y el compás donde caen.
  canvas.addEventListener('dragover', (evento) => { evento.preventDefault(); });
  canvas.addEventListener('drop', (evento) => {
    evento.preventDefault();
    const rect = canvas.getBoundingClientRect();
    const donde = encontrar(evento.clientX - rect.left, evento.clientY - rect.top);
    const fila = donde.fila ?? 0;
    const inicio = iman(Math.max(0, segundosDeX(evento.clientX - rect.left)), evento.altKey);
    acciones.alImportar([...evento.dataTransfer.files], fila, inicio);
  });
}

function bajar(evento) {
  const rect = canvas.getBoundingClientRect();
  const x = evento.clientX - rect.left;
  const y = evento.clientY - rect.top;
  const donde = encontrar(x, y);

  if (donde.zona === 'regla') {
    // La mitad alta de la regla dibuja el bucle; la baja, busca.
    if (y <= REGLA_H * 0.5) {
      const inicio = iman(Math.max(0, segundosDeX(x)), evento.altKey);
      gesto = { tipo: 'bucle', bucleInicio: inicio, bucleFin: inicio };
    } else {
      gesto = { tipo: 'regla' };
      acciones.alIrA(iman(Math.max(0, segundosDeX(x)), evento.altKey));
    }
    canvas.setPointerCapture(evento.pointerId);
    return;
  }

  if (donde.zona === 'cabecera') {
    cambiar({ pistaSeleccionada: donde.fila });
    return;
  }

  if (donde.zona === 'carril' || donde.zona === 'vacio') {
    cambiar({ seleccion: new Set(), pistaSeleccionada: donde.fila ?? estado.pistaSeleccionada });
    return;
  }

  if (donde.zona === 'fundidoIzq' || donde.zona === 'fundidoDer') {
    const { clip, fila } = donde;
    gesto = {
      tipo: donde.zona,
      clip,
      filaOrigen: fila,
      filaDestino: fila,
      sX: x,
      fundidoEntrada: clip.entradaFundido || 0,
      fundidoSalida: clip.salidaFundido || 0,
      movido: false,
    };
    canvas.setPointerCapture(evento.pointerId);
    return;
  }

  if (donde.zona === 'clip' || donde.zona === 'bordeIzq' || donde.zona === 'bordeDer') {
    const { clip, fila } = donde;
    const seleccion = evento.shiftKey ? new Set(estado.seleccion) : new Set();
    seleccion.add(clip.id);
    cambiar({ seleccion, pistaSeleccionada: fila });

    gesto = {
      tipo: donde.zona === 'clip' ? 'mover' : donde.zona,
      clip,
      filaOrigen: fila,
      filaDestino: fila,
      sX: x,
      inicio0: clip.inicio,
      fin0: clip.inicio + clip.duracion,
      inicioNuevo: clip.inicio,
      finNuevo: clip.inicio + clip.duracion,
      movido: false,
    };
    canvas.setPointerCapture(evento.pointerId);
  }
}

function mover(evento) {
  const rect = canvas.getBoundingClientRect();
  const x = evento.clientX - rect.left;
  const y = evento.clientY - rect.top;

  if (!gesto) {
    const donde = encontrar(x, y);
    canvas.style.cursor = donde.zona === 'bordeIzq' || donde.zona === 'bordeDer' ? 'ew-resize'
                       : donde.zona === 'fundidoIzq' || donde.zona === 'fundidoDer' ? 'col-resize'
                       : donde.zona === 'clip' ? 'grab'
                       : donde.zona === 'regla' ? 'text' : 'default';
    return;
  }

  if (gesto.tipo === 'regla') {
    acciones.alIrA(iman(Math.max(0, segundosDeX(x)), evento.altKey));
    return;
  }

  if (gesto.tipo === 'bucle') {
    gesto.bucleFin = iman(Math.max(0, segundosDeX(x)), evento.altKey);
    return;
  }

  const dxSegundos = (x - gesto.sX) / estado.pxPorSegundo;
  gesto.movido = gesto.movido || Math.abs(x - gesto.sX) > 3;

  if (gesto.tipo === 'fundidoIzq') {
    gesto.fundidoEntrada = Math.max(0, Math.min(gesto.clip.duracion,
      (gesto.clip.entradaFundido || 0) + dxSegundos));
    return;
  }
  if (gesto.tipo === 'fundidoDer') {
    gesto.fundidoSalida = Math.max(0, Math.min(gesto.clip.duracion,
      (gesto.clip.salidaFundido || 0) - dxSegundos));
    return;
  }

  if (gesto.tipo === 'mover') {
    gesto.inicioNuevo = iman(Math.max(0, gesto.inicio0 + dxSegundos), evento.altKey);
    gesto.finNuevo = gesto.inicioNuevo + (gesto.fin0 - gesto.inicio0);
    const fila = Math.floor((y - REGLA_H) / altoPistaActual());
    if (fila >= 0 && fila < estado.pistas.length) gesto.filaDestino = fila;
  } else if (gesto.tipo === 'bordeIzq') {
    gesto.inicioNuevo = Math.min(iman(Math.max(0, gesto.inicio0 + dxSegundos), evento.altKey), gesto.fin0 - 0.05);
  } else if (gesto.tipo === 'bordeDer') {
    gesto.finNuevo = Math.max(iman(gesto.fin0 + dxSegundos, evento.altKey), gesto.inicio0 + 0.05);
  }
}

function soltar() {
  if (!gesto) return;
  const g = gesto;
  gesto = null;
  canvas.style.cursor = 'default';

  if (g.tipo === 'bucle') {
    const inicio = Math.min(g.bucleInicio, g.bucleFin);
    const fin = Math.max(g.bucleInicio, g.bucleFin);
    if (fin - inicio > 0.01) acciones.alBucle(true, inicio, fin);
    return;
  }

  if (g.tipo === 'regla' || !g.movido || !g.clip) return;

  if (g.tipo === 'fundidoIzq' || g.tipo === 'fundidoDer') {
    acciones.alFundidos(g.clip, g.fundidoEntrada, g.fundidoSalida);
    return;
  }

  if (g.tipo === 'mover') {
    acciones.alMoverClip(g.clip, g.inicioNuevo, g.filaDestino !== g.filaOrigen ? g.filaDestino : -1, g.filaOrigen);
  } else {
    acciones.alRecortarClip(g.clip, g.inicioNuevo, g.finNuevo, g.filaOrigen);
  }
}

function dobleClic(evento) {
  const rect = canvas.getBoundingClientRect();
  const donde = encontrar(evento.clientX - rect.left, evento.clientY - rect.top);
  if (donde.zona === 'cabecera') {
    const y = REGLA_H + donde.fila * altoPistaActual();
    acciones.alRenombrarPista(donde.fila, donde.pista.nombre, { x: 8, y: y + 8, ancho: CABECERA_W - 16 });
  }
}

/* ---------- pintado ---------- */

/** Ruido determinista para las ondas de la maqueta (sin motor). */
function ruido(semilla, n) {
  const v = Math.sin(semilla * 127.1 + n * 311.7) * 43758.5453;
  return v - Math.floor(v);
}

export function pintar(ahora) {
  if (!ctx || ancho === 0) return;
  ctx.setTransform(dpr, 0, 0, dpr, 0, 0);

  const { pistas, pxPorSegundo, desplazamiento, bpm, compas, pistaSeleccionada, seleccion, bucle } = estado;
  const altoPista = altoPistaActual();

  ctx.fillStyle = color.fondo;
  ctx.fillRect(0, 0, ancho, alto);

  /* --- rejilla --- */
  const segCompas = segundosPorCompas(bpm, compas[0]);
  const pxCompas = segCompas * pxPorSegundo;
  const primerCompas = Math.floor(desplazamiento / segCompas);
  const pintarPulsos = pxCompas > 90;

  for (let c = primerCompas; ; c += 1) {
    const x = xDeSegundos(c * segCompas);
    if (x > ancho) break;
    if (x >= CABECERA_W) {
      ctx.strokeStyle = color.linea;
      ctx.beginPath(); ctx.moveTo(x + 0.5, REGLA_H); ctx.lineTo(x + 0.5, alto); ctx.stroke();
    }
    if (pintarPulsos) {
      for (let p = 1; p < compas[0]; p += 1) {
        const xp = x + p * (pxCompas / compas[0]);
        if (xp < CABECERA_W || xp > ancho) continue;
        ctx.strokeStyle = color.lineaSuave;
        ctx.beginPath(); ctx.moveTo(xp + 0.5, REGLA_H); ctx.lineTo(xp + 0.5, alto); ctx.stroke();
      }
    }
  }

  /* --- pistas y clips --- */
  pistas.forEach((pista, i) => {
    const y = REGLA_H + i * altoPista;

    ctx.strokeStyle = color.lineaSuave;
    ctx.beginPath(); ctx.moveTo(CABECERA_W, y + altoPista + 0.5); ctx.lineTo(ancho, y + altoPista + 0.5); ctx.stroke();

    for (const clip of pista.clips) {
      const previa = previsualizacion(clip);
      const inicio = previa ? previa.inicio : clip.inicio;
      const duracion = previa ? previa.duracion : clip.duracion;
      const filaPintada = previa ? previa.fila : i;
      const yBase = REGLA_H + filaPintada * altoPista;

      const x0 = xDeSegundos(inicio);
      const w = duracion * pxPorSegundo;
      if (x0 + w < CABECERA_W || x0 > ancho) continue;

      const yClip = yBase + 5;
      const hClip = altoPista - 10;
      const elegido = seleccion.has(clip.id);

      ctx.beginPath();
      ctx.roundRect(x0, yClip, w, hClip, 6);
      ctx.fillStyle = `${pista.color}${elegido ? '40' : '26'}`;
      ctx.fill();
      ctx.strokeStyle = elegido ? color.texto : pista.color;
      ctx.lineWidth = elegido ? 1.6 : 1;
      ctx.stroke();
      ctx.lineWidth = 1;

      ctx.save();
      ctx.clip();

      // Onda real si el motor mandó picos; de mentira (determinista) si no.
      const centro = yClip + hClip * 0.58;
      const margen = hClip * 0.34;
      const desde = Math.max(x0 + 2, CABECERA_W);
      const hasta = Math.min(x0 + w - 2, ancho);
      const columnas = Math.max(0, Math.floor((hasta - desde) / 2));

      ctx.strokeStyle = `${pista.color}B3`;
      ctx.beginPath();

      const entrada = picosDe(clip);
      if (entrada && columnas > 0) {
        const visibleDesde = (clip.desfase || 0) + (segundosDeX(desde) - inicio);
        const visibleDuracion = (hasta - desde) / pxPorSegundo;
        const cortados = recortarPicos(entrada.picos, entrada.porSegundo, visibleDesde, visibleDuracion, columnas);
        for (let k = 0; k < columnas; k += 1) {
          const a = (cortados[k] / 100) * margen;
          const x = desde + k * 2;
          ctx.moveTo(x + 0.5, centro - a);
          ctx.lineTo(x + 0.5, centro + a + 1);
        }
      } else {
        for (let x = desde; x < hasta; x += 2) {
          const n = Math.floor((x - x0) / 2);
          const amplitud = (0.25 + 0.75 * ruido(clip.semilla || 7, n)) * (0.6 + 0.4 * Math.sin(n * 0.09));
          const a = Math.abs(amplitud) * margen;
          ctx.moveTo(x + 0.5, centro - a);
          ctx.lineTo(x + 0.5, centro + a + 1);
        }
      }
      ctx.stroke();

      ctx.fillStyle = color.texto;
      ctx.font = '600 11px Karla, sans-serif';
      ctx.fillText(clip.nombre, x0 + 8, yClip + 11);

      // Fundidos: curva sombreada en cada extremo y su asa arriba.
      const enFundido = gesto && (gesto.tipo === 'fundidoIzq' || gesto.tipo === 'fundidoDer')
        && gesto.clip && gesto.clip.id === clip.id;
      const fEntrada = enFundido ? gesto.fundidoEntrada : (clip.entradaFundido || 0);
      const fSalida = enFundido ? gesto.fundidoSalida : (clip.salidaFundido || 0);
      const x1 = x0 + w;

      ctx.fillStyle = `${color.fondo}99`;
      ctx.strokeStyle = color.apagado;
      if (fEntrada > 0.005) {
        const xf = x0 + fEntrada * pxPorSegundo;
        ctx.beginPath();
        ctx.moveTo(x0, yClip);
        ctx.lineTo(xf, yClip);
        ctx.quadraticCurveTo(x0 + (xf - x0) * 0.3, yClip + hClip * 0.25, x0, yClip + hClip);
        ctx.closePath();
        ctx.fill();
        ctx.beginPath();
        ctx.moveTo(x0, yClip + hClip);
        ctx.quadraticCurveTo(x0 + (xf - x0) * 0.3, yClip + hClip * 0.25, xf, yClip);
        ctx.stroke();
      }
      if (fSalida > 0.005) {
        const xf = x1 - fSalida * pxPorSegundo;
        ctx.beginPath();
        ctx.moveTo(x1, yClip);
        ctx.lineTo(xf, yClip);
        ctx.quadraticCurveTo(x1 - (x1 - xf) * 0.3, yClip + hClip * 0.25, x1, yClip + hClip);
        ctx.closePath();
        ctx.fill();
        ctx.beginPath();
        ctx.moveTo(x1, yClip + hClip);
        ctx.quadraticCurveTo(x1 - (x1 - xf) * 0.3, yClip + hClip * 0.25, xf, yClip);
        ctx.stroke();
      }
      if (elegido || enFundido) {
        ctx.fillStyle = color.texto;
        for (const xAsa of [x0 + fEntrada * pxPorSegundo, x1 - fSalida * pxPorSegundo]) {
          ctx.beginPath();
          ctx.arc(xAsa, yClip + 6, 3.2, 0, Math.PI * 2);
          ctx.fill();
        }
      }

      ctx.restore();
    }
  });

  /* --- regla y cabeceras encima de todo --- */
  ctx.fillStyle = color.panel;
  ctx.fillRect(0, 0, ancho, REGLA_H);
  ctx.strokeStyle = color.linea;
  ctx.beginPath(); ctx.moveTo(0, REGLA_H + 0.5); ctx.lineTo(ancho, REGLA_H + 0.5); ctx.stroke();
  ctx.font = '11px "IBM Plex Mono", monospace';
  ctx.textBaseline = 'middle';
  for (let c = primerCompas; ; c += 1) {
    const x = xDeSegundos(c * segCompas);
    if (x > ancho) break;
    if (x < CABECERA_W) continue;
    ctx.strokeStyle = color.linea;
    ctx.beginPath(); ctx.moveTo(x + 0.5, REGLA_H - 8); ctx.lineTo(x + 0.5, REGLA_H); ctx.stroke();
    ctx.fillStyle = color.apagado;
    ctx.fillText(String(c + 1), x + 5, REGLA_H / 2 + 1);
  }

  /* --- bucle sobre la regla (con la previsualización del arrastre) --- */
  const bucleVisible = gesto && gesto.tipo === 'bucle'
    ? { activo: true, inicio: Math.min(gesto.bucleInicio, gesto.bucleFin), fin: Math.max(gesto.bucleInicio, gesto.bucleFin) }
    : bucle;
  if (bucleVisible.activo && bucleVisible.fin > bucleVisible.inicio) {
    const xa = Math.max(CABECERA_W, xDeSegundos(bucleVisible.inicio));
    const xb = Math.min(ancho, xDeSegundos(bucleVisible.fin));
    if (xb > xa) {
      ctx.fillStyle = `${color.acento}40`;
      ctx.fillRect(xa, 0, xb - xa, REGLA_H * 0.5);
      ctx.fillStyle = color.acento;
      ctx.fillRect(xa, REGLA_H * 0.5 - 2, xb - xa, 2);
    }
  }

  ctx.fillStyle = color.panel;
  ctx.fillRect(0, 0, CABECERA_W, alto);
  ctx.strokeStyle = color.linea;
  ctx.beginPath(); ctx.moveTo(CABECERA_W + 0.5, 0); ctx.lineTo(CABECERA_W + 0.5, alto); ctx.stroke();

  pistas.forEach((pista, i) => {
    const y = REGLA_H + i * altoPista;
    if (i === pistaSeleccionada) {
      ctx.fillStyle = color.panel2;
      ctx.fillRect(0, y, CABECERA_W, altoPista);
    }
    ctx.fillStyle = pista.color;
    ctx.fillRect(0, y, 3, altoPista);
    ctx.fillStyle = color.texto;
    ctx.font = '700 13px Archivo, sans-serif';
    ctx.fillText(pista.nombre, 14, y + 16);
    ctx.font = '11px Karla, sans-serif';
    ctx.fillStyle = color.apagado2;
    const extras = [pista.mute ? 'M' : '', pista.solo ? 'S' : ''].filter(Boolean).join(' ');
    ctx.fillText(extras || 'audio', 14, y + 33);
    ctx.strokeStyle = color.lineaSuave;
    ctx.beginPath(); ctx.moveTo(0, y + altoPista + 0.5); ctx.lineTo(CABECERA_W, y + altoPista + 0.5); ctx.stroke();
  });
  ctx.fillStyle = color.panel;
  ctx.fillRect(0, 0, CABECERA_W, REGLA_H);

  /* --- cursor de reproducción --- */
  const segundos = posicionParaPintar(ahora);
  const xCursor = xDeSegundos(segundos);
  if (xCursor >= CABECERA_W && xCursor <= ancho) {
    ctx.strokeStyle = color.senal;
    ctx.lineWidth = 1.6;
    ctx.beginPath(); ctx.moveTo(xCursor, 6); ctx.lineTo(xCursor, alto); ctx.stroke();
    ctx.lineWidth = 1;
    ctx.fillStyle = color.senal;
    ctx.beginPath();
    ctx.moveTo(xCursor - 5, 6); ctx.lineTo(xCursor + 5, 6); ctx.lineTo(xCursor, 15);
    ctx.closePath(); ctx.fill();
  }

  // El cursor que se sale por la derecha arrastra la vista consigo.
  if (estado.reproduciendo && xCursor > ancho - 40) {
    cambiar({ desplazamiento: segundos - (ancho - CABECERA_W) * 0.15 / pxPorSegundo });
  }
}
