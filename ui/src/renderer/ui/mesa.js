import { $, $$, esc } from './dom.js';
import { estado } from '../estado.js';

/**
 * La mesa: un canal por pista y el máster a la derecha. En el esqueleto los
 * faders son de mentira menos uno: el VU del máster, que se mueve con los
 * medidores reales del motor (con su caída balística puesta aquí, que el
 * motor manda picos crudos).
 */

let vuIzq, vuDer, dbIzq = -100, dbDer = -100;

export function montarMesa() {
  const host = $('#mesa');

  const canales = estado.pistas.map((pista) => `
    <div class="canal">
      <div class="cinta-color" style="background:${esc(pista.color)}"></div>
      <div class="cuerpo-fader">
        <input class="fader" type="range" min="-60" max="6" value="0" step="0.1" title="${esc(pista.nombre)}: 0.0 dB">
        <div class="vu"><i style="height:0%"></i></div>
      </div>
      <div class="db">0.0</div>
      <div class="botones">
        <button class="m" aria-pressed="false" title="Silenciar">M</button>
        <button class="s" aria-pressed="false" title="Solo">S</button>
      </div>
      <div class="nombre">${esc(pista.nombre)}</div>
    </div>
  `).join('');

  host.innerHTML = `${canales}
    <div class="canal maestro">
      <div class="cinta-color" style="background:var(--accent)"></div>
      <div class="cuerpo-fader">
        <input class="fader" type="range" min="-60" max="6" value="0" step="0.1" title="Máster: 0.0 dB">
        <div class="vu" id="vu-izq"><i style="height:0%"></i></div>
        <div class="vu" id="vu-der"><i style="height:0%"></i></div>
      </div>
      <div class="db" id="db-maestro">−inf</div>
      <div class="botones"></div>
      <div class="nombre">Máster</div>
    </div>
  `;

  vuIzq = $('#vu-izq i');
  vuDer = $('#vu-der i');

  // Los mandos de la maqueta responden al tacto aunque aún no gobiernen nada.
  for (const fader of $$('.mesa .fader')) {
    fader.addEventListener('input', () => {
      const canal = fader.closest('.canal');
      const db = Number(fader.value);
      canal.querySelector('.db').textContent = db <= -60 ? '−inf' : db.toFixed(1);
      fader.title = `${canal.querySelector('.nombre').textContent}: ${db.toFixed(1)} dB`;
    });
  }
  for (const boton of $$('.mesa .botones button')) {
    boton.addEventListener('click', () => {
      boton.setAttribute('aria-pressed', boton.getAttribute('aria-pressed') === 'true' ? 'false' : 'true');
    });
  }
}

/** dB → altura del VU, con −60 como suelo visible. */
const altura = (db) => `${Math.max(0, Math.min(100, (db + 60) / 0.66))}%`;

export function pintarVU() {
  // Caída de ~20 dB/s: los picos suben de golpe y bajan con elegancia.
  const objetivoIzq = estado.medidores.izq;
  const objetivoDer = estado.medidores.der;
  dbIzq = objetivoIzq > dbIzq ? objetivoIzq : Math.max(-100, dbIzq - 1.4);
  dbDer = objetivoDer > dbDer ? objetivoDer : Math.max(-100, dbDer - 1.4);

  vuIzq.style.height = altura(dbIzq);
  vuDer.style.height = altura(dbDer);

  const pico = Math.max(dbIzq, dbDer);
  $('#db-maestro').textContent = pico <= -80 ? '−inf' : pico.toFixed(1);
}
