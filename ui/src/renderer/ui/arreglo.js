import { $ } from './dom.js';
import { estado, cambiar, posicionParaPintar } from '../estado.js';
import { segundosPorCompas, segundosPorPulso } from '../../shared/tiempo.js';

/**
 * El arrangement: un solo canvas pinta cabeceras, regla, rejilla, clips con
 * su onda y el cursor. Nada de esto pasa por el DOM: a 60 fps con cientos de
 * clips es la única manera de que no se arrastre.
 */

const CABECERA_W = 150;
const REGLA_H = 30;

let canvas, ctx, wrap;
let ancho = 0, alto = 0, dpr = 1;

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

export function montarArreglo({ alIrA }) {
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

  // Clic (o arrastre) en la regla: llevar el cursor allí, con imán al pulso.
  let arrastrandoRegla = false;
  const irAlPuntero = (evento) => {
    const rect = canvas.getBoundingClientRect();
    const x = evento.clientX - rect.left;
    if (x < CABECERA_W) return;
    const segundos = Math.max(0, estado.desplazamiento + (x - CABECERA_W) / estado.pxPorSegundo);
    const pulso = segundosPorPulso(estado.bpm);
    alIrA(Math.round(segundos / pulso) * pulso);
  };
  canvas.addEventListener('pointerdown', (evento) => {
    const rect = canvas.getBoundingClientRect();
    if (evento.clientY - rect.top > REGLA_H) return;
    arrastrandoRegla = true;
    canvas.setPointerCapture(evento.pointerId);
    irAlPuntero(evento);
  });
  canvas.addEventListener('pointermove', (evento) => { if (arrastrandoRegla) irAlPuntero(evento); });
  canvas.addEventListener('pointerup', () => { arrastrandoRegla = false; });

  // Rueda: desplazar; con Ctrl, zoom anclado al puntero.
  canvas.addEventListener('wheel', (evento) => {
    evento.preventDefault();
    if (evento.ctrlKey || evento.metaKey) {
      const rect = canvas.getBoundingClientRect();
      const x = Math.max(CABECERA_W, evento.clientX - rect.left) - CABECERA_W;
      const segundoAncla = estado.desplazamiento + x / estado.pxPorSegundo;
      const factor = evento.deltaY < 0 ? 1.18 : 1 / 1.18;
      const pxPorSegundo = Math.min(600, Math.max(8, estado.pxPorSegundo * factor));
      const desplazamiento = Math.max(0, segundoAncla - x / pxPorSegundo);
      cambiar({ pxPorSegundo, desplazamiento });
    } else {
      const delta = (evento.deltaY || evento.deltaX) / estado.pxPorSegundo;
      cambiar({ desplazamiento: Math.max(0, estado.desplazamiento + delta) });
    }
  }, { passive: false });
}

/** Ruido determinista por clip: la misma "onda" en cada arranque. */
function ruido(semilla, n) {
  const v = Math.sin(semilla * 127.1 + n * 311.7) * 43758.5453;
  return v - Math.floor(v);
}

export function pintar(ahora) {
  if (!ctx || ancho === 0) return;
  ctx.setTransform(dpr, 0, 0, dpr, 0, 0);

  const { pistas, pxPorSegundo, desplazamiento, bpm, compas, pistaSeleccionada } = estado;
  const altoPistas = alto - REGLA_H;
  const altoPista = Math.max(56, Math.floor(altoPistas / Math.max(1, pistas.length)));

  ctx.fillStyle = color.fondo;
  ctx.fillRect(0, 0, ancho, alto);

  /* --- rejilla y regla --- */
  const segCompas = segundosPorCompas(bpm, compas[0]);
  const pxCompas = segCompas * pxPorSegundo;
  const primerCompas = Math.floor(desplazamiento / segCompas);
  const pintarPulsos = pxCompas > 90;

  ctx.font = '11px "IBM Plex Mono", monospace';
  ctx.textBaseline = 'middle';

  for (let c = primerCompas; ; c += 1) {
    const x = CABECERA_W + (c * segCompas - desplazamiento) * pxPorSegundo;
    if (x > ancho) break;
    if (x >= CABECERA_W) {
      ctx.strokeStyle = color.linea;
      ctx.beginPath(); ctx.moveTo(x + 0.5, REGLA_H); ctx.lineTo(x + 0.5, alto); ctx.stroke();
      ctx.fillStyle = color.apagado;
      ctx.fillText(String(c + 1), x + 5, REGLA_H / 2 + 1);
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
      const x0 = CABECERA_W + (clip.inicio - desplazamiento) * pxPorSegundo;
      const w = clip.duracion * pxPorSegundo;
      if (x0 + w < CABECERA_W || x0 > ancho) continue;

      const yClip = y + 5;
      const hClip = altoPista - 10;
      ctx.beginPath();
      ctx.roundRect(x0, yClip, w, hClip, 6);
      ctx.fillStyle = `${pista.color}26`;
      ctx.fill();
      ctx.strokeStyle = pista.color;
      ctx.stroke();

      // La onda de mentira, determinista por semilla: F1 la sustituirá por
      // los picos reales que calcule el motor.
      ctx.save();
      ctx.clip();
      ctx.strokeStyle = `${pista.color}B3`;
      ctx.beginPath();
      const centro = yClip + hClip * 0.58;
      const margen = hClip * 0.34;
      const desde = Math.max(x0 + 2, CABECERA_W);
      for (let x = desde; x < Math.min(x0 + w - 2, ancho); x += 2) {
        const n = Math.floor((x - x0) / 2);
        const amplitud = (0.25 + 0.75 * ruido(clip.semilla, n)) * (0.6 + 0.4 * Math.sin(n * 0.09 + clip.semilla));
        const a = Math.abs(amplitud) * margen;
        ctx.moveTo(x + 0.5, centro - a);
        ctx.lineTo(x + 0.5, centro + a + 1);
      }
      ctx.stroke();
      ctx.fillStyle = color.texto;
      ctx.font = '600 11px Karla, sans-serif';
      ctx.fillText(clip.nombre, x0 + 8, yClip + 11);
      ctx.restore();
    }
  });

  /* --- fondo de regla y cabeceras encima de todo --- */
  ctx.fillStyle = color.panel;
  ctx.fillRect(0, 0, ancho, REGLA_H);
  ctx.strokeStyle = color.linea;
  ctx.beginPath(); ctx.moveTo(0, REGLA_H + 0.5); ctx.lineTo(ancho, REGLA_H + 0.5); ctx.stroke();
  ctx.font = '11px "IBM Plex Mono", monospace';
  for (let c = primerCompas; ; c += 1) {
    const x = CABECERA_W + (c * segCompas - desplazamiento) * pxPorSegundo;
    if (x > ancho) break;
    if (x < CABECERA_W) continue;
    ctx.strokeStyle = color.linea;
    ctx.beginPath(); ctx.moveTo(x + 0.5, REGLA_H - 8); ctx.lineTo(x + 0.5, REGLA_H); ctx.stroke();
    ctx.fillStyle = color.apagado;
    ctx.fillText(String(c + 1), x + 5, REGLA_H / 2 + 1);
  }

  ctx.fillStyle = color.panel;
  ctx.fillRect(0, 0, CABECERA_W, alto);
  ctx.strokeStyle = color.linea;
  ctx.beginPath(); ctx.moveTo(CABECERA_W + 0.5, 0); ctx.lineTo(CABECERA_W + 0.5, alto); ctx.stroke();

  ctx.font = '700 13px Archivo, sans-serif';
  pistas.forEach((pista, i) => {
    const y = REGLA_H + i * altoPista;
    if (i === pistaSeleccionada) {
      ctx.fillStyle = color.panel2;
      ctx.fillRect(0, y, CABECERA_W, altoPista);
    }
    ctx.fillStyle = pista.color;
    ctx.fillRect(0, y, 3, altoPista);
    ctx.fillStyle = color.texto;
    ctx.fillText(pista.nombre, 14, y + 20);
    ctx.fillStyle = color.apagado2;
    ctx.font = '11px Karla, sans-serif';
    ctx.fillText('audio', 14, y + 36);
    ctx.font = '700 13px Archivo, sans-serif';
    ctx.strokeStyle = color.lineaSuave;
    ctx.beginPath(); ctx.moveTo(0, y + altoPista + 0.5); ctx.lineTo(CABECERA_W, y + altoPista + 0.5); ctx.stroke();
  });
  ctx.fillStyle = color.panel;
  ctx.fillRect(0, 0, CABECERA_W, REGLA_H);

  /* --- cursor de reproducción --- */
  const segundos = posicionParaPintar(ahora);
  const xCursor = CABECERA_W + (segundos - desplazamiento) * pxPorSegundo;
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
