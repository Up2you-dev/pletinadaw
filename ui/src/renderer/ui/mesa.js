import { $, $$, esc } from './dom.js';
import { estado } from '../estado.js';

/**
 * La mesa: un canal por pista y el máster a la derecha. Faders, pan, mute y
 * solo mandan órdenes de mezcla; los VU se mueven con los medidores reales y
 * su caída balística se pone aquí (el motor manda picos crudos). El máster
 * enseña además la sonoridad LUFS cuando hay un Medidor en su cadena.
 */

let acciones = null;
const caida = new Map(); // clave → dB suavizado del VU
let origenGrupo = null;  // gesto de agrupar en dos clics: la pista marcada

export function montarMesa(inyectadas) {
  acciones = inyectadas;
  pintarMesa();
}

export function pintarMesa() {
  const host = $('#mesa');

  const envioFila = (pista) => pista.retorno ? '<div class="envios ret">retorno</div>' : `
      <div class="envios">
        <label title="Envío al bus A (Retorno A)">A<input class="envio" data-bus="0" type="range" min="-60" max="0" step="0.5"
          value="${Math.max(-60, pista.envios?.[0] ?? -60)}"></label>
        <label title="Envío al bus B (Retorno B)">B<input class="envio" data-bus="1" type="range" min="-60" max="0" step="0.5"
          value="${Math.max(-60, pista.envios?.[1] ?? -60)}"></label>
      </div>`;

  const canal = (pista, i) => `
    <div class="canal${i === estado.pistaSeleccionada ? ' elegido' : ''}${pista.retorno ? ' retorno' : ''}${pista.grupo >= 0 ? ' agrupado' : ''}${i === origenGrupo ? ' origen' : ''}" data-pista="${i}">
      <div class="cinta-color" style="background:${esc(pista.color)}"></div>
      <div class="cuerpo-fader">
        <input class="fader" type="range" min="-60" max="6" value="${pista.volumenDb ?? 0}" step="0.1"
               title="${esc(pista.nombre)}: ${(pista.volumenDb ?? 0).toFixed(1)} dB">
        <div class="vu" data-vu="${i}"><i style="height:0%"></i></div>
      </div>
      <input class="pan" type="range" min="-1" max="1" value="${pista.pan ?? 0}" step="0.05" title="Pan">
      ${envioFila(pista)}
      <div class="db">${(pista.volumenDb ?? 0) <= -60 ? '−inf' : (pista.volumenDb ?? 0).toFixed(1)}</div>
      <div class="botones">
        <button class="m" aria-pressed="${pista.mute ? 'true' : 'false'}" title="Silenciar">M</button>
        <button class="s" aria-pressed="${pista.solo ? 'true' : 'false'}" title="Solo">S</button>
        <button class="c" aria-pressed="${pista.congelada ? 'true' : 'false'}" title="Congelar pista">❄</button>
        <button class="r" aria-pressed="${pista.armada ? 'true' : 'false'}" title="Armar para grabar">●</button>
        <button class="g" aria-pressed="${pista.grupo >= 0 ? 'true' : i === origenGrupo ? 'true' : 'false'}"
                title="${pista.grupo >= 0 ? 'Sacar del grupo' : 'Agrupar: marca esta pista y pulsa ⌸ en la otra'}">⌸</button>
      </div>
      <div class="nombre">${esc(pista.nombre)}</div>
    </div>
  `;

  // La tira de un grupo aparece delante de su primer miembro: fader, pan,
  // M/S y VU del bus, y la ✕ que lo disuelve. Con una pista marcada para
  // agrupar, un toque en la tira del grupo la mete dentro.
  const tiraGrupo = (g) => `
    <div class="canal grupo${estado.pistaSeleccionada === -2 - g.indice ? ' elegido' : ''}" data-pista="${-2 - g.indice}" data-grupo="${g.indice}">
      <div class="cinta-color"></div>
      <div class="cuerpo-fader">
        <input class="fader" type="range" min="-60" max="6" value="${g.volumenDb ?? 0}" step="0.1"
               title="${esc(g.nombre)}: ${(g.volumenDb ?? 0).toFixed(1)} dB">
        <div class="vu" data-vu="g${g.indice}"><i style="height:0%"></i></div>
      </div>
      <input class="pan" type="range" min="-1" max="1" value="${g.pan ?? 0}" step="0.05" title="Pan">
      <div class="envios ret">grupo</div>
      <div class="db">${(g.volumenDb ?? 0) <= -60 ? '−inf' : (g.volumenDb ?? 0).toFixed(1)}</div>
      <div class="botones">
        <button class="m" aria-pressed="${g.mute ? 'true' : 'false'}" title="Silenciar el grupo">M</button>
        <button class="s" aria-pressed="${g.solo ? 'true' : 'false'}" title="Solo del grupo">S</button>
        <button class="x" title="Deshacer el grupo (las pistas quedan sueltas)">✕</button>
      </div>
      <div class="nombre">⌸ ${esc(g.nombre)}</div>
    </div>
  `;

  const grupos = estado.grupos || [];
  const emitidos = new Set();
  const canales = [];
  estado.pistas.forEach((pista, i) => {
    if (pista.grupo >= 0 && !emitidos.has(pista.grupo) && grupos[pista.grupo]) {
      emitidos.add(pista.grupo);
      canales.push(tiraGrupo(grupos[pista.grupo]));
    }
    canales.push(canal(pista, i));
  });

  host.innerHTML = `${canales.join('')}
    <button class="canal anadir" id="anadir-pista" title="Añadir pista">+</button>
    <div class="canal maestro${estado.pistaSeleccionada === -1 ? ' elegido' : ''}" data-pista="-1">
      <div class="cinta-color" style="background:var(--accent)"></div>
      <div class="cuerpo-fader">
        <input class="fader" type="range" min="-60" max="6" value="${estado.master.volumenDb ?? 0}" step="0.1" title="Máster">
        <div class="vu" data-vu="izq"><i style="height:0%"></i></div>
        <div class="vu" data-vu="der"><i style="height:0%"></i></div>
      </div>
      <div class="lufs" id="lufs">LUFS —</div>
      <div class="db" id="db-maestro">−inf</div>
      <div class="botones"></div>
      <div class="nombre">Máster</div>
    </div>
  `;

  for (const canalEl of $$('.mesa .canal[data-pista]')) {
    const indice = Number(canalEl.dataset.pista);

    canalEl.addEventListener('pointerdown', (evento) => {
      if (evento.target.matches('input, button')) return;
      // Con una pista marcada para agrupar, tocar la tira de un grupo la mete.
      if (indice <= -2 && origenGrupo !== null && estado.pistas[origenGrupo]?.grupo === -1) {
        const marcada = origenGrupo;
        origenGrupo = null;
        acciones.alMeterEnGrupo(marcada, -2 - indice);
        return;
      }
      acciones.alSeleccionarPista(indice);
    });

    const fader = canalEl.querySelector('.fader');
    fader?.addEventListener('input', () => {
      const db = Number(fader.value);
      canalEl.querySelector('.db').textContent = db <= -60 ? '−inf' : db.toFixed(1);
      acciones.alMezclar(indice, { volumenDb: db });
    });

    const pan = canalEl.querySelector('.pan');
    pan?.addEventListener('input', () => acciones.alMezclar(indice, { pan: Number(pan.value) }));

    canalEl.querySelector('.m')?.addEventListener('click', (evento) => {
      const activo = evento.currentTarget.getAttribute('aria-pressed') !== 'true';
      evento.currentTarget.setAttribute('aria-pressed', String(activo));
      acciones.alMezclar(indice, { mute: activo });
    });
    canalEl.querySelector('.s')?.addEventListener('click', (evento) => {
      const activo = evento.currentTarget.getAttribute('aria-pressed') !== 'true';
      evento.currentTarget.setAttribute('aria-pressed', String(activo));
      acciones.alMezclar(indice, { solo: activo });
    });
    canalEl.querySelector('.c')?.addEventListener('click', (evento) => {
      acciones.alCongelar(indice, evento.currentTarget.getAttribute('aria-pressed') !== 'true');
    });
    canalEl.querySelector('.r')?.addEventListener('click', (evento) => {
      acciones.alArmar(indice, evento.currentTarget.getAttribute('aria-pressed') !== 'true');
    });
    canalEl.querySelector('.g')?.addEventListener('click', () => {
      const pista = estado.pistas[indice];
      if (!pista) return;
      if (pista.grupo >= 0) { acciones.alSacarDeGrupo(indice); return; }
      if (origenGrupo === indice) { origenGrupo = null; pintarMesa(); return; }
      if (origenGrupo === null || estado.pistas[origenGrupo]?.grupo !== -1) {
        origenGrupo = indice;
        pintarMesa();
        return;
      }
      const marcada = origenGrupo;
      origenGrupo = null;
      acciones.alCrearGrupo([marcada, indice]);
    });
    canalEl.querySelector('.x')?.addEventListener('click', () => {
      acciones.alDeshacerGrupo(Number(canalEl.dataset.grupo));
    });

    for (const envio of canalEl.querySelectorAll('.envio')) {
      envio.addEventListener('change', () => {
        acciones.alEnviar(indice, Number(envio.dataset.bus), Number(envio.value));
      });
    }
  }

  $('#anadir-pista')?.addEventListener('click', () => acciones.alCrearPista());
}

/** dB → altura del VU, con −60 como suelo visible. */
const altura = (db) => `${Math.max(0, Math.min(100, (db + 60) / 0.66))}%`;

function suavizar(clave, objetivo) {
  const previo = caida.get(clave) ?? -100;
  const valor = objetivo > previo ? objetivo : Math.max(-100, previo - 1.4);
  caida.set(clave, valor);
  return valor;
}

export function pintarVU() {
  const m = estado.medidores;

  const vuIzq = $('.mesa .vu[data-vu="izq"] i');
  const vuDer = $('.mesa .vu[data-vu="der"] i');
  if (!vuIzq) return;

  const izq = suavizar('izq', m.izq);
  const der = suavizar('der', m.der);
  vuIzq.style.height = altura(izq);
  vuDer.style.height = altura(der);

  const pico = Math.max(izq, der);
  const dbMaestro = $('#db-maestro');
  if (dbMaestro) dbMaestro.textContent = pico <= -80 ? '−inf' : pico.toFixed(1);

  const lufs = $('#lufs');
  if (lufs) {
    if (m.lufs) {
      const tp = m.lufs.picoVerdadero !== undefined && m.lufs.picoVerdadero > -90
        ? ` · TP ${m.lufs.picoVerdadero.toFixed(1)}` : '';
      lufs.textContent = `${m.lufs.i > -90 ? m.lufs.i.toFixed(1) : '—'} LUFS${tp}`;
      lufs.title = `momentánea ${m.lufs.m.toFixed(1)} · corta ${m.lufs.s.toFixed(1)} · integrada ${m.lufs.i > -90 ? m.lufs.i.toFixed(1) : '—'}`
        + ` · pico verdadero ${(m.lufs.picoVerdadero ?? m.lufs.pico).toFixed(1)} dBTP`
        + ` · LRA ${(m.lufs.lra ?? 0).toFixed(1)} LU · correlación ${(m.lufs.correlacion ?? 1).toFixed(2)}`;
    } else {
      lufs.textContent = 'LUFS —';
      lufs.title = 'Inserta el Medidor en el máster para medir LUFS';
    }
  }

  (m.pistas || []).forEach((niveles, i) => {
    const barra = $(`.mesa .vu[data-vu="${i}"] i`);
    if (barra) barra.style.height = altura(suavizar(`p${i}`, Math.max(niveles.izq, niveles.der)));
  });

  (m.grupos || []).forEach((niveles, i) => {
    const barra = $(`.mesa .vu[data-vu="g${i}"] i`);
    if (barra) barra.style.height = altura(suavizar(`g${i}`, Math.max(niveles.izq, niveles.der)));
  });

  // El espectro del Medidor, allí donde su tarjeta ponga el lienzo.
  for (const canvas of $$('.espectro')) {
    const ctx = canvas.getContext('2d');
    const estilo = getComputedStyle(document.documentElement);
    ctx.clearRect(0, 0, canvas.width, canvas.height);
    const bandas = m.espectro;
    if (!bandas?.length) continue;
    const anchoBarra = canvas.width / bandas.length;
    ctx.fillStyle = estilo.getPropertyValue('--accent').trim();
    for (let i = 0; i < bandas.length; i += 1) {
      const h = Math.max(1, ((Math.max(-80, bandas[i]) + 80) / 80) * canvas.height);
      ctx.fillRect(i * anchoBarra + 1, canvas.height - h, anchoBarra - 2, h);
    }
  }

  // El vectorscopio: nube L/R girada 45° — vertical mono, horizontal contrafase.
  for (const canvas of $$('.vectorscopio')) {
    const ctx = canvas.getContext('2d');
    const estilo = getComputedStyle(document.documentElement);
    ctx.clearRect(0, 0, canvas.width, canvas.height);
    const cx = canvas.width / 2;
    const cy = canvas.height / 2;
    ctx.strokeStyle = estilo.getPropertyValue('--line').trim();
    ctx.strokeRect(0.5, 0.5, canvas.width - 1, canvas.height - 1);
    ctx.beginPath(); ctx.moveTo(cx, 2); ctx.lineTo(cx, canvas.height - 2); ctx.stroke();
    const xy = m.xy;
    if (!xy?.length) continue;
    ctx.fillStyle = estilo.getPropertyValue('--signal').trim();
    const escala = canvas.width * 0.48;
    for (let i = 0; i < xy.length; i += 2) {
      const izq = xy[i];
      const der = xy[i + 1];
      const x = cx + ((izq - der) * 0.7071) * escala;
      const y = cy - ((izq + der) * 0.7071) * escala;
      ctx.fillRect(x - 0.8, y - 0.8, 1.6, 1.6);
    }
  }
}
