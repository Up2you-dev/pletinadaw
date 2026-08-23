import { $ } from './dom.js';
import { estado, cambiar } from '../estado.js';

/**
 * El piano roll: un panel flotante sobre el arrangement para el clip MIDI
 * abierto (doble clic en el clip). Clic pinta una nota, arrastrarla la mueve
 * (vertical transpone), su borde derecho la alarga, Alt+clic o botón derecho
 * la borra, y la rueda sobre una nota cambia su velocidad. Cada gesto manda
 * la lista entera de notas al motor: una transacción, un deshacer.
 */

const NOTA_MIN = 24, NOTA_MAX = 107;          // C1..B7
const ALTO_FILA = 12, ANCHO_TECLADO = 44;
const NEGRAS = new Set([1, 3, 6, 8, 10]);
const NOMBRES = ['C', 'C#', 'D', 'D#', 'E', 'F', 'F#', 'G', 'G#', 'A', 'A#', 'B'];

let acciones = null;
let lienzo = null, ctx = null;
let gesto = null;                              // { tipo, indice, notas, x0, beat0, nota0 }
let rejilla = 0.25;                            // en beats: semicorchea

export function montarPiano(inyectadas) {
  acciones = inyectadas;
}

const clipAbierto = () => {
  for (const pista of estado.pistas)
    for (const clip of pista.clips || [])
      if (clip.id === estado.pianoRoll) return clip;
  return null;
};

const beatsDelClip = (clip) => Math.max(1, Math.round(clip.duracion * (estado.bpm / 60)));
const pxPorBeat = (clip) => Math.max(24, Math.min(120, 720 / beatsDelClip(clip)));

export function pintarPiano() {
  const host = $('#piano-roll');
  const clip = estado.pianoRoll === null ? null : clipAbierto();

  if (!clip) {
    host.hidden = true;
    host.innerHTML = '';
    lienzo = null;
    if (estado.pianoRoll !== null) cambiar({ pianoRoll: null });
    return;
  }

  const filas = NOTA_MAX - NOTA_MIN + 1;
  const beats = beatsDelClip(clip);
  const anchoCanvas = ANCHO_TECLADO + beats * pxPorBeat(clip);
  const altoCanvas = filas * ALTO_FILA;

  if (host.hidden || !lienzo) {
    host.hidden = false;
    host.innerHTML = `
      <div class="piano-caja">
        <div class="piano-cabeza">
          <span class="titulo">${clip.nombre ? clip.nombre : 'Clip MIDI'} · notas</span>
          <label>rejilla
            <select class="rejilla">
              <option value="1">1 beat</option>
              <option value="0.5">1/2</option>
              <option value="0.25" selected>1/4</option>
              <option value="0.125">1/8</option>
            </select>
          </label>
          <label>cuantizar
            <select class="cuantizar">
              <option value="">(no)</option>
              <option value="1 beat">1 beat</option>
              <option value="1/2 beat">1/2 beat</option>
              <option value="1/4 beat">1/4 beat</option>
              <option value="1/8 beat">1/8 beat</option>
            </select>
          </label>
          <button class="cerrar" title="Cerrar (Esc)">✕</button>
        </div>
        <div class="piano-scroll">
          <canvas width="${anchoCanvas}" height="${altoCanvas}"></canvas>
        </div>
      </div>`;

    lienzo = host.querySelector('canvas');
    ctx = lienzo.getContext('2d');

    host.querySelector('.cerrar').addEventListener('click', () => cambiar({ pianoRoll: null }));
    host.querySelector('.rejilla').addEventListener('change', (e) => { rejilla = Number(e.target.value); });
    const cuantizar = host.querySelector('.cuantizar');
    cuantizar.value = clip.cuantizacion && cuantizar.querySelector(`option[value="${clip.cuantizacion}"]`)
      ? clip.cuantizacion : '';
    cuantizar.addEventListener('change', () => {
      if (cuantizar.value) acciones.alCuantizar(clip.id, cuantizar.value);
    });

    lienzo.addEventListener('pointerdown', bajar);
    lienzo.addEventListener('pointermove', mover);
    lienzo.addEventListener('pointerup', soltar);
    lienzo.addEventListener('contextmenu', (e) => e.preventDefault());
    lienzo.addEventListener('wheel', rueda, { passive: false });

    // Centrar la vista donde están las notas (o en C4 si aún no hay).
    const scroll = host.querySelector('.piano-scroll');
    const centro = (clip.notas || []).length
      ? (clip.notas.reduce((s, n) => s + n.nota, 0) / clip.notas.length) : 60;
    scroll.scrollTop = yDeNota(centro) - scroll.clientHeight / 2;
  }

  if (lienzo.width !== anchoCanvas) lienzo.width = anchoCanvas;
  dibujar(clip);
}

const yDeNota = (nota) => (NOTA_MAX - nota) * ALTO_FILA;
const notaDeY = (y) => NOTA_MAX - Math.floor(y / ALTO_FILA);
const xDeBeat = (clip, beat) => ANCHO_TECLADO + beat * pxPorBeat(clip);
const beatDeX = (clip, x) => (x - ANCHO_TECLADO) / pxPorBeat(clip);

function dibujar(clip) {
  const estilo = getComputedStyle(document.documentElement);
  const v = (nombre) => estilo.getPropertyValue(nombre).trim();
  const beats = beatsDelClip(clip);
  const compas = estado.compas[0];

  ctx.fillStyle = v('--panel');
  ctx.fillRect(0, 0, lienzo.width, lienzo.height);

  for (let nota = NOTA_MIN; nota <= NOTA_MAX; nota += 1) {
    const y = yDeNota(nota);
    if (NEGRAS.has(nota % 12)) {
      ctx.fillStyle = v('--panel-2');
      ctx.fillRect(ANCHO_TECLADO, y, lienzo.width, ALTO_FILA);
    }
    // El teclado del margen.
    ctx.fillStyle = NEGRAS.has(nota % 12) ? v('--panel-3') : v('--text');
    ctx.fillRect(0, y + 0.5, ANCHO_TECLADO - 6, ALTO_FILA - 1);
    if (nota % 12 === 0) {
      ctx.fillStyle = v('--muted');
      ctx.font = '9px Karla, sans-serif';
      ctx.fillText(`C${nota / 12 - 1}`, ANCHO_TECLADO - 26, y + ALTO_FILA - 3);
      ctx.strokeStyle = v('--line');
      ctx.beginPath(); ctx.moveTo(ANCHO_TECLADO, y + ALTO_FILA + 0.5); ctx.lineTo(lienzo.width, y + ALTO_FILA + 0.5); ctx.stroke();
    }
  }

  for (let b = 0; b <= beats; b += 1) {
    const x = xDeBeat(clip, b);
    ctx.strokeStyle = b % compas === 0 ? v('--line') : v('--line-soft');
    ctx.beginPath(); ctx.moveTo(x + 0.5, 0); ctx.lineTo(x + 0.5, lienzo.height); ctx.stroke();
  }

  const notas = (gesto ? gesto.notas : clip.notas) || [];
  notas.forEach((n, i) => {
    const x = xDeBeat(clip, n.inicio);
    const w = Math.max(3, n.duracion * pxPorBeat(clip) - 1);
    const y = yDeNota(n.nota);
    ctx.fillStyle = v('--accent');
    ctx.globalAlpha = 0.45 + 0.55 * ((n.velocidad ?? 100) / 127);
    ctx.fillRect(x, y + 1, w, ALTO_FILA - 2);
    ctx.globalAlpha = 1;
    if (gesto && gesto.indice === i) {
      ctx.strokeStyle = v('--text');
      ctx.strokeRect(x + 0.5, y + 1.5, w - 1, ALTO_FILA - 3);
    }
  });
}

function notaEn(clip, x, y) {
  const notas = clip.notas || [];
  for (let i = notas.length - 1; i >= 0; i -= 1) {
    const n = notas[i];
    const nx = xDeBeat(clip, n.inicio);
    const nw = Math.max(3, n.duracion * pxPorBeat(clip) - 1);
    if (y >= yDeNota(n.nota) && y < yDeNota(n.nota) + ALTO_FILA && x >= nx && x <= nx + nw) {
      return { indice: i, borde: x > nx + nw - 5 };
    }
  }
  return null;
}

const alRedondeo = (beat) => Math.round(beat / rejilla) * rejilla;

function bajar(evento) {
  const clip = clipAbierto();
  if (!clip) return;
  const rect = lienzo.getBoundingClientRect();
  const x = evento.clientX - rect.left;
  const y = evento.clientY - rect.top;
  if (x < ANCHO_TECLADO) return;

  const notas = (clip.notas || []).map((n) => ({ ...n }));
  const dado = notaEn(clip, x, y);

  if (dado && (evento.altKey || evento.button === 2)) {
    notas.splice(dado.indice, 1);
    acciones.alNotasMidi(clip.id, notas);
    return;
  }

  if (dado) {
    gesto = { tipo: dado.borde ? 'borde' : 'mover', indice: dado.indice, notas,
              beat0: beatDeX(clip, x), nota0: notaDeY(y),
              origen: { ...notas[dado.indice] } };
  } else {
    const inicio = Math.max(0, alRedondeo(beatDeX(clip, x) - rejilla / 2));
    notas.push({ nota: notaDeY(y), inicio, duracion: rejilla, velocidad: 100 });
    gesto = { tipo: 'borde', indice: notas.length - 1, notas,
              beat0: beatDeX(clip, x), nota0: notaDeY(y),
              origen: { ...notas[notas.length - 1] } };
  }
  lienzo.setPointerCapture(evento.pointerId);
  dibujar(clip);
}

function mover(evento) {
  if (!gesto) return;
  const clip = clipAbierto();
  if (!clip) return;
  const rect = lienzo.getBoundingClientRect();
  const beat = beatDeX(clip, evento.clientX - rect.left);
  const nota = notaDeY(evento.clientY - rect.top);
  const n = gesto.notas[gesto.indice];

  if (gesto.tipo === 'mover') {
    n.inicio = Math.max(0, alRedondeo(gesto.origen.inicio + (beat - gesto.beat0)));
    n.nota = Math.max(NOTA_MIN, Math.min(NOTA_MAX, gesto.origen.nota + (nota - gesto.nota0)));
  } else {
    n.duracion = Math.max(rejilla, alRedondeo(beat - n.inicio));
  }
  dibujar(clip);
}

function soltar() {
  if (!gesto) return;
  const clip = clipAbierto();
  const { notas } = gesto;
  gesto = null;
  if (clip) acciones.alNotasMidi(clip.id, notas);
}

function rueda(evento) {
  const clip = clipAbierto();
  if (!clip) return;
  const rect = lienzo.getBoundingClientRect();
  const dado = notaEn(clip, evento.clientX - rect.left, evento.clientY - rect.top);
  if (!dado) return;
  evento.preventDefault();
  const notas = (clip.notas || []).map((n) => ({ ...n }));
  const n = notas[dado.indice];
  n.velocidad = Math.max(1, Math.min(127, (n.velocidad ?? 100) - Math.sign(evento.deltaY) * 8));
  acciones.alNotasMidi(clip.id, notas);
}

export function nombreDeNota(nota) {
  return `${NOMBRES[nota % 12]}${Math.floor(nota / 12) - 1}`;
}
