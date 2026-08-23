import { $, esc } from './dom.js';
import { ICO } from './iconos.js';
import { CATALOGO } from '../../shared/catalogo.js';
import { estado } from '../estado.js';

/**
 * El rail: el proyecto arriba, el navegador de sonidos (una carpeta elegida,
 * sus audios, clic importa en el cursor), y debajo el catálogo entero de la
 * suite con su fase de llegada. Enseñar el mapa es parte de la casa: lo que
 * aún no existe se ve en gris con su fase, no se esconde ni se finge.
 */

let acciones = null;
let sonidos = { carpeta: null, archivos: [] };
let rutaSonando = null;

export function montarRail(inyectadas) {
  if (inyectadas) acciones = inyectadas;
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

  const filasSonidos = sonidos.archivos.slice(0, 200).map((archivo) => `
    <div class="entrada sonido${archivo.ruta === rutaSonando ? ' sonando' : ''}" data-ruta="${esc(archivo.ruta)}"
         title="clic: escuchar / parar · doble clic: importar en el cursor — ${esc(archivo.ruta)}">
      ${ICO.onda}<span>${esc(archivo.nombre)}</span>
      ${archivo.ruta === rutaSonando ? '<span class="detalle">▶</span>' : ''}
    </div>`).join('');
  const navegador = `
    <h2>Sonidos</h2>
    <div class="entrada boton-carpeta" title="Elegir la carpeta de sonidos">
      ${ICO.carpeta}<span>${sonidos.carpeta ? esc(sonidos.carpeta.split(/[\\/]/).pop()) : 'elegir carpeta…'}</span>
      ${sonidos.archivos.length ? `<span class="detalle">${sonidos.archivos.length}</span>` : ''}
    </div>
    ${filasSonidos}
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

  host.innerHTML = proyecto + navegador + grupos;

  host.querySelector('.boton-carpeta')?.addEventListener('click', async () => {
    const resultado = await acciones?.alElegirCarpetaSonidos?.();
    if (resultado) {
      sonidos = resultado;
      montarRail();
    }
  });
  for (const fila of host.querySelectorAll('.sonido')) {
    // Clic: escuchar (o parar si ya suena). Doble clic: importar en el cursor.
    fila.addEventListener('click', async () => {
      const ruta = fila.dataset.ruta;
      if (rutaSonando === ruta) {
        rutaSonando = null;
        await acciones?.alPararPrevia?.();
      } else {
        rutaSonando = ruta;
        await acciones?.alPrevia?.(ruta);
      }
      montarRail();
    });
    fila.addEventListener('dblclick', () => {
      rutaSonando = null;
      acciones?.alPararPrevia?.();
      acciones?.alImportarSonido?.(fila.dataset.ruta);
    });
  }
}
