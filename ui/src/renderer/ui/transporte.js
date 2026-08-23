import { $, esc } from './dom.js';
import { ICO } from './iconos.js';
import { estado, posicionParaPintar } from '../estado.js';
import { formatoMusical, formatoReloj, bpmValido } from '../../shared/tiempo.js';

/**
 * La barra de transporte: proyecto (nuevo/abrir/guardar/importar/exportar),
 * tocar/parar, el reloj doble, tempo, metrónomo y el chip del motor.
 */

let relojMusical, relojSegundos, botonTocar, botonMetronomo, botonCiclo, chipProyecto;

export function montarTransporte(acciones) {
  const host = $('#transporte');
  host.innerHTML = `
    <button class="btn btn-icono" id="b-nuevo" title="Nuevo proyecto">${ICO.nuevo}</button>
    <button class="btn btn-icono" id="b-abrir" title="Abrir proyecto">${ICO.carpeta}</button>
    <button class="btn btn-icono" id="b-guardar" title="Guardar (Ctrl+S)">${ICO.guardar}</button>
    <span class="separador"></span>
    <button class="btn" id="b-importar" title="Importar audio a la pista seleccionada">${ICO.importar}<span>Importar</span></button>
    <button class="btn" id="b-exportar" title="Exportar la mezcla a WAV">${ICO.exportar}<span>Exportar</span></button>
    <span class="separador"></span>
    <button class="btn btn-icono" id="al-principio" title="Ir al principio (Inicio)">${ICO.alPrincipio}</button>
    <button class="btn btn-icono btn-primary" id="tocar" title="Tocar / parar (espacio)">${ICO.tocar}</button>
    <button class="btn btn-icono" id="b-metronomo" aria-pressed="false" title="Metrónomo">${ICO.metronomo}</button>
    <button class="btn btn-icono" id="b-ciclo" aria-pressed="false" title="Bucle (dibújalo en la mitad alta de la regla)">${ICO.ciclo}</button>
    <div class="reloj" title="compás.pulso.dieciseisavo · minutos:segundos">
      <span class="musical" id="reloj-musical">1.1.1</span>
      <span class="segundero" id="reloj-segundos">0:00.0</span>
    </div>
    <label class="campo-bpm" title="Tempo del proyecto">
      <input id="bpm" inputmode="decimal" value="${estado.bpm}">
      <span>BPM</span>
      <span class="compas">${estado.compas[0]}/${estado.compas[1]}</span>
    </label>
    <span class="chip-proyecto" id="chip-proyecto"></span>
  `;

  relojMusical = $('#reloj-musical');
  relojSegundos = $('#reloj-segundos');
  botonTocar = $('#tocar');
  botonMetronomo = $('#b-metronomo');
  botonCiclo = $('#b-ciclo');
  chipProyecto = $('#chip-proyecto');
  botonCiclo.addEventListener('click', acciones.alCiclo);

  botonTocar.addEventListener('click', acciones.alConmutar);
  $('#al-principio').addEventListener('click', () => acciones.alIrA(0));
  $('#b-nuevo').addEventListener('click', acciones.alNuevoProyecto);
  $('#b-abrir').addEventListener('click', acciones.alAbrirProyecto);
  $('#b-guardar').addEventListener('click', acciones.alGuardar);
  $('#b-importar').addEventListener('click', acciones.alImportarDialogo);
  $('#b-exportar').addEventListener('click', acciones.alExportar);
  botonMetronomo.addEventListener('click', acciones.alMetronomo);

  const bpm = $('#bpm');
  bpm.addEventListener('change', () => {
    const valor = bpmValido(bpm.value, estado.bpm);
    bpm.value = valor;
    acciones.alCambiarTempo(valor);
  });

  pintarBoton();
  pintarProyecto();
}

export function pintarBoton() {
  botonTocar.innerHTML = estado.reproduciendo ? ICO.parar : ICO.tocar;
  botonTocar.title = estado.reproduciendo ? 'Parar (espacio)' : 'Tocar (espacio)';
  botonMetronomo.setAttribute('aria-pressed', estado.metronomo ? 'true' : 'false');
  botonCiclo.setAttribute('aria-pressed', estado.bucle.activo ? 'true' : 'false');
}

export function pintarProyecto() {
  const { proyecto, exportando } = estado;
  const nombre = esc(proyecto.nombre || 'sin guardar');
  const punto = proyecto.modificado ? ' ·' : '';
  chipProyecto.textContent = exportando ? 'exportando…' : `${nombre}${punto}`;
  chipProyecto.title = proyecto.ruta || 'Proyecto temporal: usa Nuevo proyecto para darle carpeta';

  const bpm = $('#bpm');
  if (bpm && document.activeElement !== bpm) bpm.value = estado.bpm;
}

/** Llamado desde el bucle de pintado: el reloj corre aunque los eventos lleguen a 15 Hz. */
export function pintarReloj(ahora) {
  const segundos = posicionParaPintar(ahora);
  relojMusical.textContent = formatoMusical(segundos, estado.bpm, estado.compas[0]);
  relojSegundos.textContent = formatoReloj(segundos);
}

const TEXTO_ESTADO = {
  maqueta: 'modo maqueta · sin motor',
  arrancando: 'motor arrancando…',
  caido: 'motor caído',
};

export function pintarChipMotor() {
  const chip = $('#chip-motor');
  const { motor } = estado;
  chip.className = `chip-motor ${motor.estado}`;
  if (motor.estado === 'conectado') {
    const info = motor.info || {};
    chip.querySelector('.texto').textContent =
      `motor ${info.version || '?'} · ${info.audio === 'sin-audio' ? 'sin audio' : 'audio'}`;
  } else {
    chip.querySelector('.texto').textContent = TEXTO_ESTADO[motor.estado] || motor.estado;
  }
}
