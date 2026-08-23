import { $, esc } from './dom.js';
import { ICO } from './iconos.js';
import { CADENA_MAESTRA, buscarDispositivo } from '../../shared/catalogo.js';

/**
 * La tira de dispositivos, al estilo Ableton: la cadena de la pista
 * seleccionada de izquierda a derecha. En el esqueleto enseña la cadena del
 * máster que F1 hará real, con cada tarjeta marcando su fase.
 */

export function montarTira() {
  const host = $('#tira');

  const tarjetas = CADENA_MAESTRA.map((nombre) => {
    const d = buscarDispositivo(nombre);
    if (!d) return '';
    const activo = d.fase === 0;
    return `
      <div class="dispositivo${activo ? ' activo' : ' futuro'}" title="${esc(d.detalle)}">
        <div class="nombre">${esc(d.nombre)}</div>
        <div class="detalle">${esc(d.detalle)}</div>
        <div class="pie"><span class="fase${activo ? ' f0' : ''}">F${d.fase}</span><span class="led"></span></div>
      </div>
    `;
  }).join('');

  host.innerHTML = `
    <span class="titulo">máster</span>
    ${tarjetas}
    <button class="mas" title="Insertar dispositivo (F1)">${ICO.mas}</button>
  `;

  $('.tira .mas').addEventListener('click', () => {
    import('./dom.js').then(({ aviso }) => aviso('Insertar dispositivos llega en F1; la cadena que ves es la de fábrica del máster.'));
  });
}
