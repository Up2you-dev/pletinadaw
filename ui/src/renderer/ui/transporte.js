import { $ } from './dom.js';
import { ICO } from './iconos.js';
import { estado, cambiar, posicionParaPintar } from '../estado.js';
import { formatoMusical, formatoReloj, bpmValido } from '../../shared/tiempo.js';

/**
 * La barra de transporte: tocar/parar, el reloj doble (musical y de pared),
 * el tempo y el chip del motor. Manda órdenes si el motor está; si no, mueve
 * un reloj local para que la maqueta se pueda recorrer igual.
 */

let relojMusical, relojSegundos, botonTocar;

export function montarTransporte({ alConmutar, alIrA }) {
  const host = $('#transporte');
  host.innerHTML = `
    <button class="btn btn-icono" id="al-principio" title="Ir al principio (Inicio)">${ICO.alPrincipio}</button>
    <button class="btn btn-icono btn-primary" id="tocar" title="Tocar / parar (espacio)">${ICO.tocar}</button>
    <div class="reloj" title="compás.pulso.dieciseisavo · minutos:segundos">
      <span class="musical" id="reloj-musical">1.1.1</span>
      <span class="segundero" id="reloj-segundos">0:00.0</span>
    </div>
    <label class="campo-bpm" title="Tempo del proyecto">
      <input id="bpm" inputmode="decimal" value="${estado.bpm}">
      <span>BPM</span>
      <span class="compas">${estado.compas[0]}/${estado.compas[1]}</span>
    </label>
  `;

  relojMusical = $('#reloj-musical');
  relojSegundos = $('#reloj-segundos');
  botonTocar = $('#tocar');

  botonTocar.addEventListener('click', alConmutar);
  $('#al-principio').addEventListener('click', () => alIrA(0));

  const bpm = $('#bpm');
  bpm.addEventListener('change', () => {
    const valor = bpmValido(bpm.value, estado.bpm);
    bpm.value = valor;
    cambiar({ bpm: valor });
  });

  pintarBoton();
}

export function pintarBoton() {
  botonTocar.innerHTML = estado.reproduciendo ? ICO.parar : ICO.tocar;
  botonTocar.title = estado.reproduciendo ? 'Parar (espacio)' : 'Tocar (espacio)';
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
