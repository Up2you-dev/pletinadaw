/** Utilidades de DOM mínimas, herencia del reproductor. */

export const $ = (selector, root = document) => root.querySelector(selector);
export const $$ = (selector, root = document) => [...root.querySelectorAll(selector)];

const ENTITIES = { '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;', "'": '&#39;' };

/** Todo texto que no escribamos nosotros pasa por aquí antes de tocar el DOM. */
export const esc = (value) => String(value ?? '').replace(/[&<>"']/g, (c) => ENTITIES[c]);

export function aviso(mensaje) {
  const host = $('#avisos');
  const el = document.createElement('div');
  el.className = 'aviso';
  el.textContent = mensaje;
  host.appendChild(el);
  setTimeout(() => el.remove(), 3400);
}
