import { $, esc } from './dom.js';
import { ICO } from './iconos.js';
import { CATALOGO } from '../../shared/catalogo.js';

/**
 * El rail: el proyecto arriba y, debajo, el catálogo entero de la suite con
 * su fase de llegada. Enseñar el mapa es parte del esqueleto: lo que aún no
 * existe se ve en gris con su fase, no se esconde ni se finge.
 */

export function montarRail() {
  const host = $('#rail');

  const proyecto = `
    <h2>Proyecto</h2>
    <div class="entrada">${ICO.carpeta}<span>demo.pletina</span><span class="detalle">esqueleto</span></div>
    <div class="entrada futura">${ICO.onda}<span>media/</span><span class="fase">F1</span></div>
  `;

  const grupos = CATALOGO.map(({ grupo, dispositivos }) => {
    const filas = dispositivos.map((d) => {
      const icono = grupo === 'Instrumentos' ? ICO.teclado : ICO.enchufe;
      const clase = d.fase === 0 ? '' : ' futura';
      const fase = d.fase === 0 ? '<span class="fase f0">F0</span>' : `<span class="fase">F${d.fase}</span>`;
      return `<div class="entrada${clase}" title="${esc(d.detalle)}">${icono}<span>${esc(d.nombre)}</span>${fase}</div>`;
    }).join('');
    return `<h2>${esc(grupo)}</h2>${filas}`;
  }).join('');

  host.innerHTML = proyecto + grupos;
}
