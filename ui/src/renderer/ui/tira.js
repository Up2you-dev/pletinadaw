import { $, $$, esc } from './dom.js';
import { ICO } from './iconos.js';
import { estado } from '../estado.js';

/**
 * La tira de dispositivos, al estilo Ableton: la cadena de la pista
 * seleccionada (o del máster) de izquierda a derecha. Cada tarjeta enseña
 * sus parámetros como deslizadores generados del descriptor que manda el
 * motor: la tira no conoce los efectos de nada, los pinta.
 */

let acciones = null;

// La suite de F1 que se puede insertar hoy; el resto vive en el rail con su fase.
const INSERTABLES = [
  { tipo: 'eqocho', nombre: 'EQ Ocho' },
  { tipo: 'compresor', nombre: 'Compresor' },
  { tipo: 'techo', nombre: 'Techo' },
  { tipo: 'medidor', nombre: 'Medidor' },
  { tipo: 'utilidad', nombre: 'Utilidad' },
];

export function montarTira(inyectadas) {
  acciones = inyectadas;
  pintarTira();
}

const objetivo = () => (estado.pistaSeleccionada === -1
  ? { nombre: 'máster', plugins: estado.master.plugins || [] }
  : { nombre: estado.pistas[estado.pistaSeleccionada]?.nombre || '…',
      plugins: estado.pistas[estado.pistaSeleccionada]?.plugins || [] });

export function pintarTira() {
  const host = $('#tira');
  const { nombre, plugins } = objetivo();
  const indicePista = estado.pistaSeleccionada;

  const tarjetas = plugins.map((plugin) => {
    const mandos = (plugin.parametros || []).map((p) => `
      <label class="mando" title="${esc(p.nombre)}: ${Number(p.valor).toFixed(2)}">
        <span>${esc(p.nombre)}</span>
        <input type="range" data-parametro="${esc(p.id)}"
               min="${p.min}" max="${p.max}" step="${(p.max - p.min) / 200}" value="${p.valor}">
      </label>
    `).join('');

    return `
      <div class="dispositivo${plugin.activo ? ' activo' : ' apagado'}" data-indice="${plugin.indice}">
        <div class="cabeza">
          <button class="led" title="${plugin.activo ? 'Apagar' : 'Encender'}"></button>
          <span class="nombre">${esc(plugin.nombre)}</span>
          <button class="cerrar" title="Quitar">${ICO.x}</button>
        </div>
        <div class="mandos">${mandos}</div>
      </div>
    `;
  }).join('');

  const menu = INSERTABLES.map((d) =>
    `<button data-tipo="${d.tipo}">${esc(d.nombre)}</button>`).join('');

  host.innerHTML = `
    <span class="titulo">${esc(nombre)}</span>
    ${tarjetas}
    <div class="insertar">
      <button class="mas" title="Insertar dispositivo">${ICO.mas}</button>
      <div class="menu" hidden>${menu}</div>
    </div>
  `;

  for (const tarjeta of $$('.tira .dispositivo')) {
    const indice = Number(tarjeta.dataset.indice);

    tarjeta.querySelector('.led').addEventListener('click', () => {
      acciones.alActivarPlugin(indicePista, indice, tarjeta.classList.contains('apagado'));
    });
    tarjeta.querySelector('.cerrar').addEventListener('click', () => {
      acciones.alQuitarPlugin(indicePista, indice);
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
  for (const opcion of menuEl.querySelectorAll('button')) {
    opcion.addEventListener('click', () => {
      menuEl.hidden = true;
      acciones.alInsertarPlugin(indicePista, opcion.dataset.tipo);
    });
  }
}
