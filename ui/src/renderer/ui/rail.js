import { $, esc } from './dom.js';
import { ICO } from './iconos.js';
import { CATALOGO } from '../../shared/catalogo.js';
import { estado } from '../estado.js';

/**
 * El rail: el proyecto arriba y, debajo, el catálogo entero de la suite con
 * su fase de llegada. Enseñar el mapa es parte de la casa: lo que aún no
 * existe se ve en gris con su fase, no se esconde ni se finge.
 */

export function montarRail() {
  const host = $('#rail');

  const clips = estado.pistas.reduce((suma, pista) => suma + pista.clips.length, 0);
  const proyecto = `
    <h2>Proyecto</h2>
    <div class="entrada" title="${esc(estado.proyecto.ruta || 'proyecto temporal')}">
      ${ICO.carpeta}<span>${esc(estado.proyecto.nombre || 'sin guardar')}</span>
      <span class="detalle">${estado.proyecto.modificado ? 'sin guardar' : ''}</span>
    </div>
    <div class="entrada">${ICO.onda}<span>clips</span><span class="detalle">${clips}</span></div>
  `;

  const grupos = CATALOGO.map(({ grupo, dispositivos }) => {
    const filas = dispositivos.map((d) => {
      const icono = grupo === 'Instrumentos' ? ICO.teclado : ICO.enchufe;
      const existe = !!d.lista;
      const clase = existe ? '' : ' futura';
      const fase = existe ? '<span class="fase f0">✓</span>' : `<span class="fase">F${d.fase}</span>`;
      return `<div class="entrada${clase}" title="${esc(d.detalle)}">${icono}<span>${esc(d.nombre)}</span>${fase}</div>`;
    }).join('');
    return `<h2>${esc(grupo)}</h2>${filas}`;
  }).join('');

  host.innerHTML = proyecto + grupos;
}
