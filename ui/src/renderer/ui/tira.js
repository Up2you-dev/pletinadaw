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

export function pintarTira() {
  const host = $('#tira');
  const { nombre, plugins } = objetivo();
  const indicePista = estado.pistaSeleccionada;

  const tarjetas = plugins.map((plugin) => {
    const mandos = plugin.tipo === 'medidor'
      ? '<canvas class="espectro" width="220" height="52" title="Espectro del máster"></canvas>'
      : (plugin.parametros || []).map((p) => `
      <label class="mando" title="${esc(p.nombre)}: ${Number(p.valor).toFixed(2)}">
        <span>${esc(p.nombre)}</span>
        <input type="range" data-parametro="${esc(p.id)}"
               min="${p.min}" max="${p.max}" step="${(p.max - p.min) / 200}" value="${p.valor}">
      </label>
    `).join('');

    return `
      <div class="dispositivo${plugin.activo ? ' activo' : ' apagado'}" data-indice="${plugin.indice}" data-tipo="${esc(plugin.tipo)}">
        <div class="cabeza">
          <button class="led" title="${plugin.activo ? 'Apagar' : 'Encender'}"></button>
          <span class="nombre">${esc(plugin.nombre)}</span>
          <button class="presets" title="Presets">▾</button>
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
