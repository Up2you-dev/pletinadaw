import { $, esc, aviso } from './dom.js';
import { estado } from '../estado.js';

/**
 * La Session View: escenas × pistas, al estilo del directo. Cada celda es una
 * ranura; clic lanza su clip (con la cuantización de lanzamiento del
 * proyecto), clic en una vacía copia dentro el clip seleccionado del
 * arrangement, el ▶ de la izquierda lanza la escena entera y el ⏹ para todo.
 * Los estados (tocando/encolado) llegan vivos con los medidores.
 */

let acciones = null;
let firmaPintada = '';

export function montarSesion(inyectadas) {
  acciones = inyectadas;
}

const CUANTIZACIONES = ['None', '4 Bars', '2 Bars', '1 Bar', '1/2', '1/4', '1/8', '1/16'];

export function pintarSesion(forzar = false) {
  const host = $('#sesion');
  if (!estado.vistaSesion) { host.hidden = true; firmaPintada = ''; return; }
  host.hidden = false;

  const escenas = estado.escenas || 0;
  const enVivo = estado.medidores.sesion;

  // Solo se reconstruye si cambia algo visible: los medidores llegan a 15 Hz.
  const firma = JSON.stringify([escenas, estado.cuantizacionLanzamiento, enVivo,
    estado.pistas.map((p) => (p.ranuras || []).map((r) => r.clip || ''))]);
  if (!forzar && firma === firmaPintada) return;
  firmaPintada = firma;

  const pistas = estado.pistas.filter((p) => !p.retorno);

  const filas = [];
  for (let e = 0; e < escenas; e += 1) {
    const celdas = pistas.map((p) => {
      const ranura = (p.ranuras || [])[e] || {};
      const vivo = enVivo?.[p.indice]?.[e] || ranura.estado || '';
      if (!ranura.clip) {
        return `<button class="celda vacia" data-pista="${p.indice}" data-escena="${e}"
                        title="Clic: copiar aquí el clip seleccionado del arreglo">·</button>`;
      }
      return `<button class="celda ${esc(vivo)}" data-pista="${p.indice}" data-escena="${e}"
                      style="--tinta:${esc(p.color || 'var(--accent)')}"
                      title="${esc(ranura.nombre || '')} (${esc(vivo || 'parado')})">
                <span class="led-celda"></span>${esc(ranura.nombre || 'clip')}</button>`;
    }).join('');
    filas.push(`
      <div class="fila-escena">
        <button class="lanzar-escena" data-escena="${e}" title="Lanzar la escena ${e + 1}">▶</button>
        ${celdas}
      </div>`);
  }

  host.innerHTML = `
    <div class="sesion-caja">
      <div class="fila-escena cabecera">
        <span class="esquina">escenas</span>
        ${pistas.map((p) => `<span class="nombre-pista" style="--tinta:${esc(p.color)}">${esc(p.nombre)}</span>`).join('')}
      </div>
      ${filas.join('')}
      <div class="fila-escena pie">
        <button class="mas-escena" title="Añadir escena">+</button>
        <button class="parar-todo" title="Parar todas las ranuras">⏹ parar</button>
        <label class="cuantiza">al lanzar
          <select>${CUANTIZACIONES.map((c) =>
            `<option${c === estado.cuantizacionLanzamiento ? ' selected' : ''}>${c}</option>`).join('')}</select>
        </label>
      </div>
    </div>`;

  for (const celda of host.querySelectorAll('.celda')) {
    celda.addEventListener('click', () => {
      const pista = Number(celda.dataset.pista);
      const escena = Number(celda.dataset.escena);
      if (celda.classList.contains('vacia')) {
        const [id] = [...estado.seleccion];
        if (!id) return aviso('Selecciona antes un clip del arreglo para copiarlo a la ranura.');
        acciones.alPonerEnSesion(pista, escena, id);
      } else if (celda.classList.contains('tocando') || celda.classList.contains('encolado')) {
        acciones.alPararSesion(pista);
      } else {
        acciones.alLanzarSesion(pista, escena);
      }
    });
  }
  for (const boton of host.querySelectorAll('.lanzar-escena')) {
    boton.addEventListener('click', () => acciones.alLanzarSesion(-1, Number(boton.dataset.escena)));
  }
  host.querySelector('.mas-escena').addEventListener('click', () => acciones.alEscenas(escenas + 1));
  host.querySelector('.parar-todo').addEventListener('click', () => acciones.alPararSesion(-1));
  host.querySelector('.cuantiza select').addEventListener('change', (evento) => {
    acciones.alCuantizacionSesion(evento.target.value);
  });
}
