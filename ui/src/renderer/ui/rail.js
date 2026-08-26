import { $, esc } from './dom.js';
import { ICO } from './iconos.js';
import { CATALOGO } from '../../shared/catalogo.js';
import { filtrarSonidos } from '../../shared/sonidos.js';
import { estado } from '../estado.js';

/**
 * El rail: el proyecto arriba, el navegador de sonidos (una carpeta elegida,
 * búsqueda sin acentos, favoritos ★ arriba; clic escucha, doble clic importa
 * en el cursor y arrastrar suelta en el arreglo), y debajo el catálogo entero
 * de la suite con su fase de llegada. Enseñar el mapa es parte de la casa:
 * lo que aún no existe se ve en gris con su fase, no se esconde ni se finge.
 */

let acciones = null;
let sonidos = { carpeta: null, archivos: [] };
let rutaSonando = null;
let busqueda = '';

// Los favoritos son cosa de la interfaz (como el tema): localStorage.
const favoritos = (() => {
  try { return new Set(JSON.parse(localStorage.getItem('pletina-favoritos') || '[]')); }
  catch { return new Set(); }
})();
function guardarFavoritos() {
  try { localStorage.setItem('pletina-favoritos', JSON.stringify([...favoritos])); }
  catch { /* sin almacenamiento, sin drama */ }
}

const filaSonido = (archivo) => `
    <div class="entrada sonido${archivo.ruta === rutaSonando ? ' sonando' : ''}" draggable="true" data-ruta="${esc(archivo.ruta)}"
         title="clic: escuchar / parar · doble clic: importar en el cursor · arrastrar al arreglo — ${esc(archivo.ruta)}">
      ${ICO.onda}<span>${esc(archivo.nombre)}</span>
      ${archivo.ruta === rutaSonando ? '<span class="detalle">▶</span>' : ''}
      <span class="fav${favoritos.has(archivo.ruta) ? ' activo' : ''}" title="Favorito: siempre arriba">★</span>
    </div>`;

/** Solo la lista: así teclear en la búsqueda no repinta el rail entero. */
function pintarListaSonidos() {
  const lista = $('#rail .lista-sonidos');
  if (!lista) return;
  lista.innerHTML = filtrarSonidos(sonidos.archivos, busqueda, favoritos).slice(0, 200).map(filaSonido).join('');
  cablearSonidos(lista);
}

function cablearSonidos(contenedor) {
  for (const fila of contenedor.querySelectorAll('.sonido')) {
    const ruta = fila.dataset.ruta;

    // Clic: escuchar (o parar si ya suena). Doble clic: importar en el cursor.
    fila.addEventListener('click', async (evento) => {
      if (evento.target.classList.contains('fav')) return;
      if (rutaSonando === ruta) {
        rutaSonando = null;
        await acciones?.alPararPrevia?.();
      } else {
        rutaSonando = ruta;
        await acciones?.alPrevia?.(ruta);
      }
      pintarListaSonidos();
    });
    fila.addEventListener('dblclick', () => {
      rutaSonando = null;
      acciones?.alPararPrevia?.();
      acciones?.alImportarSonido?.(ruta);
    });
    fila.addEventListener('dragstart', (evento) => {
      evento.dataTransfer.setData('text/pletina-ruta', ruta);
      evento.dataTransfer.effectAllowed = 'copy';
    });
    fila.querySelector('.fav')?.addEventListener('click', (evento) => {
      evento.stopPropagation();
      if (favoritos.has(ruta)) favoritos.delete(ruta);
      else favoritos.add(ruta);
      guardarFavoritos();
      pintarListaSonidos();
    });
  }
}

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

  const navegador = `
    <h2>Sonidos</h2>
    <div class="entrada boton-carpeta" title="Elegir la carpeta de sonidos">
      ${ICO.carpeta}<span>${sonidos.carpeta ? esc(sonidos.carpeta.split(/[\\/]/).pop()) : 'elegir carpeta…'}</span>
      ${sonidos.archivos.length ? `<span class="detalle">${sonidos.archivos.length}</span>` : ''}
    </div>
    ${sonidos.carpeta ? `<input class="busca-sonidos" type="search" placeholder="buscar…" value="${esc(busqueda)}"
         title="Filtra por nombre, sin acentos ni mayúsculas; los ★ van siempre arriba">` : ''}
    <div class="lista-sonidos"></div>
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
  pintarListaSonidos();

  host.querySelector('.boton-carpeta')?.addEventListener('click', async () => {
    const resultado = await acciones?.alElegirCarpetaSonidos?.();
    if (resultado) {
      sonidos = resultado;
      montarRail();
    }
  });
  host.querySelector('.busca-sonidos')?.addEventListener('input', (evento) => {
    busqueda = evento.target.value;
    pintarListaSonidos();
  });
}
