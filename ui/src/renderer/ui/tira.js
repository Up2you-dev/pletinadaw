import { $, $$, esc } from './dom.js';
import { ICO } from './iconos.js';
import { estado } from '../estado.js';
import { INSERTABLES } from '../../shared/catalogo.js';

/**
 * La tira de dispositivos, al estilo Ableton: la cadena de la pista
 * seleccionada (o del máster) de izquierda a derecha. Cada tarjeta enseña
 * sus parámetros como deslizadores generados del descriptor que manda el
 * motor: la tira no conoce los efectos de nada, los pinta. Lo insertable
 * sale del catálogo compartido: una sola fuente de verdad.
 */

let acciones = null;

export function montarTira(inyectadas) {
  acciones = inyectadas;
  pintarTira();
}

const objetivo = () => (estado.pistaSeleccionada === -1
  ? { nombre: 'máster', plugins: estado.master.plugins || [] }
  : { nombre: estado.pistas[estado.pistaSeleccionada]?.nombre || '…',
      plugins: estado.pistas[estado.pistaSeleccionada]?.plugins || [] });

/** El clip seleccionado, si es exactamente uno: el inspector es suyo. */
const clipSolo = () => {
  if (estado.seleccion.size !== 1) return null;
  const [id] = estado.seleccion;
  for (const pista of estado.pistas)
    for (const clip of pista.clips || [])
      if (clip.id === id) return clip;
  return null;
};

export function pintarTira() {
  const host = $('#tira');
  const { nombre, plugins } = objetivo();
  const indicePista = estado.pistaSeleccionada;
  const clip = clipSolo();

  const inspector = clip === null ? '' : clip.tipo === 'midi' ? `
    <div class="dispositivo inspector-clip activo">
      <div class="cabeza">
        <span class="nombre" title="Clip MIDI seleccionado">${esc(clip.nombre || 'Clip MIDI')}</span>
      </div>
      <div class="mandos mandos-clip">
        <button class="abrir-notas">Abrir notas ♪</button>
        <span class="campo">${(clip.notas || []).length} notas</span>
        <span class="campo">${esc(clip.cuantizacion && clip.cuantizacion !== '(none)' ? clip.cuantizacion : 'sin cuantizar')}</span>
      </div>
    </div>
  ` : `
    <div class="dispositivo inspector-clip activo">
      <div class="cabeza">
        <span class="nombre" title="Clip seleccionado">${esc(clip.nombre || 'Clip')}</span>
      </div>
      <div class="mandos mandos-clip">
        <label class="campo" title="Tempo del material de origen: lo detecta el motor al importar y se puede corregir aquí">
          <span>BPM fuente</span>
          <input class="bpm-fuente" type="number" min="20" max="999" step="0.1"
                 value="${clip.bpmFuente > 0 ? Number(clip.bpmFuente).toFixed(1) : ''}" placeholder="?">
        </label>
        <button class="warp${clip.autoTempo ? ' encendido' : ''}"
                title="El clip sigue el tempo del proyecto (time-stretch)">Warp ${clip.autoTempo ? 'ON' : 'OFF'}</button>
        <label class="campo" title="Transposición en semitonos, sin cambiar la duración">
          <span>Transposición</span>
          <input class="transposicion" type="number" min="-24" max="24" step="1"
                 value="${Math.round(clip.transposicion || 0)}">
        </label>
      </div>
    </div>
  `;

  const tarjetas = plugins.map((plugin) => {
    const mandos = plugin.tipo === 'medidor'
      ? `<div class="medidor-lienzos">
           <canvas class="espectro" width="170" height="52" title="Espectro del máster"></canvas>
           <canvas class="vectorscopio" width="52" height="52" title="Vectorscopio (M/S): vertical = mono, horizontal = fuera de fase"></canvas>
         </div>`
      : (plugin.parametros || []).map((p) => `
      <label class="mando" title="${esc(p.nombre)}: ${Number(p.valor).toFixed(2)}">
        <span>${esc(p.nombre)}</span>
        <input type="range" data-parametro="${esc(p.id)}"
               min="${p.min}" max="${p.max}" step="${(p.max - p.min) / 200}" value="${p.valor}">
      </label>
    `).join('');

    const lateral = plugin.admiteLateral ? `
        <select class="lateral" title="Entrada lateral (side-chain): la pista que dispara el detector">
          <option value="-1">lateral: no</option>
          ${estado.pistas.filter((p) => !p.retorno).map((p) =>
            `<option value="${p.indice}"${plugin.lateral === p.indice ? ' selected' : ''}>◁ ${esc(p.nombre)}</option>`).join('')}
        </select>` : '';

    return `
      <div class="dispositivo${plugin.activo ? ' activo' : ' apagado'}" data-indice="${plugin.indice}" data-tipo="${esc(plugin.tipo)}">
        <div class="cabeza">
          <button class="led" title="${plugin.activo ? 'Apagar' : 'Encender'}"></button>
          <span class="nombre">${esc(plugin.nombre)}</span>
          ${lateral}
          <button class="presets" title="Presets">▾</button>
          <button class="cerrar" title="Quitar">${ICO.x}</button>
        </div>
        <div class="mandos">${mandos}</div>
      </div>
    `;
  }).join('');

  const vst = (estado.vst || []).map((p) =>
    `<button data-tipo="vst:${esc(p.id)}" title="${esc(p.fabricante || '')}">${esc(p.nombre)} ·vst</button>`).join('');
  const menu = INSERTABLES.map((d) =>
    `<button data-tipo="${d.tipo}">${esc(d.nombre)}</button>`).join('')
    + (vst ? `<div class="raya"></div>${vst}` : '')
    + '<button class="escanear-vst">Buscar VST3…</button>';

  host.innerHTML = `
    <span class="titulo">${esc(nombre)}</span>
    ${inspector}
    ${tarjetas}
    <div class="insertar">
      <button class="mas" title="Insertar dispositivo">${ICO.mas}</button>
      <div class="menu" hidden>${menu}</div>
    </div>
  `;

  if (clip !== null && clip.tipo === 'midi') {
    $('.tira .inspector-clip .abrir-notas')?.addEventListener('click', () => {
      acciones.alAbrirPianoRoll(clip.id);
    });
  } else if (clip !== null) {
    const tarjeta = $('.tira .inspector-clip');
    const bpmFuente = tarjeta.querySelector('.bpm-fuente');
    const transposicion = tarjeta.querySelector('.transposicion');

    tarjeta.querySelector('.warp').addEventListener('click', () => {
      // Para encender el warp hace falta tempo de origen: el detectado, el
      // corregido en la caja, o el del proyecto como último recurso.
      acciones.alWarp(clip.id, clip.autoTempo
        ? { autoTempo: false }
        : { autoTempo: true, bpmFuente: Number(bpmFuente.value) || clip.bpmFuente || estado.bpm });
    });
    bpmFuente.addEventListener('change', () => {
      const bpm = Number(bpmFuente.value);
      if (bpm >= 20 && bpm <= 999) acciones.alWarp(clip.id, { bpmFuente: bpm });
    });
    transposicion.addEventListener('change', () => {
      acciones.alWarp(clip.id, { transposicion: Math.max(-24, Math.min(24, Number(transposicion.value) || 0)) });
    });
  }

  for (const tarjeta of $$('.tira .dispositivo')) {
    const indice = Number(tarjeta.dataset.indice);

    tarjeta.querySelector('.led').addEventListener('click', () => {
      acciones.alActivarPlugin(indicePista, indice, tarjeta.classList.contains('apagado'));
    });
    tarjeta.querySelector('.cerrar').addEventListener('click', () => {
      acciones.alQuitarPlugin(indicePista, indice);
    });
    tarjeta.querySelector('.presets').addEventListener('click', async (evento) => {
      const previo = tarjeta.querySelector('.menu');
      if (previo) return previo.remove();

      const lista = await acciones.alListarPresets(tarjeta.dataset.tipo);
      const menu = document.createElement('div');
      menu.className = 'menu presets-menu';
      menu.innerHTML = lista.map((p) =>
        `<button data-preset="${esc(p.nombre)}">${esc(p.nombre)}${p.fabrica ? ' ·f' : ''}</button>`).join('')
        + '<button class="guardar-preset">Guardar actual…</button>';
      tarjeta.appendChild(menu);

      for (const opcion of menu.querySelectorAll('button[data-preset]')) {
        opcion.addEventListener('click', () => {
          menu.remove();
          acciones.alCargarPreset(indicePista, indice, opcion.dataset.preset);
        });
      }
      menu.querySelector('.guardar-preset').addEventListener('click', () => {
        const entrada = document.createElement('input');
        entrada.placeholder = 'nombre del preset';
        menu.replaceChildren(entrada);
        entrada.focus();
        entrada.addEventListener('keydown', (e) => {
          if (e.key === 'Enter' && entrada.value.trim()) {
            acciones.alGuardarPreset(indicePista, indice, entrada.value.trim());
            menu.remove();
          }
          if (e.key === 'Escape') menu.remove();
          e.stopPropagation();
        });
      });
      evento.stopPropagation();
    });

    tarjeta.querySelector('.lateral')?.addEventListener('change', (evento) => {
      acciones.alLateral(indicePista, indice, Number(evento.target.value));
    });

    for (const mando of tarjeta.querySelectorAll('input[data-parametro]')) {
      mando.addEventListener('input', () => {
        mando.parentElement.title = `${mando.previousElementSibling?.textContent ?? ''}: ${Number(mando.value).toFixed(2)}`;
        acciones.alParametro(indicePista, indice, mando.dataset.parametro, Number(mando.value));
      });
    }
  }

  const boton = $('.tira .insertar .mas');
  const menuEl = $('.tira .insertar .menu');
  boton.addEventListener('click', () => { menuEl.hidden = !menuEl.hidden; });
  for (const opcion of menuEl.querySelectorAll('button[data-tipo]')) {
    opcion.addEventListener('click', () => {
      menuEl.hidden = true;
      acciones.alInsertarPlugin(indicePista, opcion.dataset.tipo);
    });
  }
  menuEl.querySelector('.escanear-vst')?.addEventListener('click', () => {
    menuEl.hidden = true;
    acciones.alEscanearVst();
  });
}
