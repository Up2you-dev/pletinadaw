import { $ } from './dom.js';

/**
 * La visita guiada del primer arranque: un foco recorre las zonas de la
 * ventana con dos frases por parada. Se enseña una sola vez (localStorage),
 * se salta con Esc o con su botón, y no vuelve a molestar. En el humo se
 * fuerza con ?visita=1 para capturarla.
 */

const PASOS = [
  { objetivo: '#transporte', titulo: 'El transporte',
    texto: 'Espacio toca y para; R graba en las pistas armadas (con Mayús añade claqueta). El BPM se edita aquí y la regla del arreglo mueve el cursor y dibuja el bucle.' },
  { objetivo: '#rail', titulo: 'El rail',
    texto: 'Tu proyecto, el navegador de Sonidos (busca, marca favoritos ★, un clic escucha y arrastras al arreglo) y el catálogo entero de la suite, con su fase.' },
  { objetivo: '#lienzo-wrap', titulo: 'El arreglo',
    texto: 'Arrastra audio aquí y cae en su pista y su compás. T divide bajo el cursor, doble clic en el vacío crea un clip MIDI, y la cabecera renombra (doble clic) y reordena pistas arrastrando.' },
  { objetivo: '#mesa', titulo: 'La mesa',
    texto: 'Fader, pan, M/S, envíos y VU por pista. El botón ⌸ agrupa dos pistas en un bus con cadena propia; ● arma para grabar.' },
  { objetivo: '#tira', titulo: 'La tira de dispositivos',
    texto: 'La cadena de lo que esté seleccionado: + inserta efectos, instrumentos o VST3; ☰ Rack envuelve la cadena en un rack con 8 macros asignables.' },
  { objetivo: '#b-sesion', titulo: 'La Session View',
    texto: 'Tab (o este botón) cambia a la rejilla de escenas: clips lanzados con cuantización, como en directo. Teclas 1–8 lanzan escenas y 0 lo para todo.' },
  { objetivo: '#b-ayuda', titulo: 'Y si te pierdes',
    texto: 'Aquí viven los atajos y el manual completo, en español. Esta visita no vuelve a salir sola. ¡A tocar!' },
];

let capa = null;
let paso = 0;

function terminar() {
  try { localStorage.setItem('pletina-visita', 'hecha'); } catch { /* da igual */ }
  window.removeEventListener('resize', pintarPaso);
  window.removeEventListener('keydown', alTeclear, true);
  capa?.remove();
  capa = null;
}

function alTeclear(evento) {
  if (!capa) return;
  if (evento.key === 'Escape') terminar();
  else if (evento.key === 'Enter' || evento.key === ' ') avanzar();
  else return;
  evento.preventDefault();
  evento.stopPropagation();
}

function avanzar() {
  paso += 1;
  if (paso >= PASOS.length) terminar();
  else pintarPaso();
}

function pintarPaso() {
  if (!capa) return;
  const { objetivo, titulo, texto } = PASOS[paso];
  const el = $(objetivo);
  if (!el) return avanzar();               // una zona oculta se salta sola

  const r = el.getBoundingClientRect();
  const foco = capa.querySelector('.visita-foco');
  const margen = 6;
  foco.style.left = `${r.left - margen}px`;
  foco.style.top = `${r.top - margen}px`;
  foco.style.width = `${r.width + margen * 2}px`;
  foco.style.height = `${r.height + margen * 2}px`;

  const tarjeta = capa.querySelector('.visita-tarjeta');
  tarjeta.querySelector('h3').textContent = titulo;
  tarjeta.querySelector('p').textContent = texto;
  tarjeta.querySelector('.visita-paso').textContent = `${paso + 1} / ${PASOS.length}`;
  tarjeta.querySelector('.visita-sigue').textContent = paso === PASOS.length - 1 ? 'A tocar' : 'Siguiente';

  // La tarjeta, donde quepa: debajo del foco si hay sitio, si no encima,
  // y como último recurso centrada.
  const alto = tarjeta.offsetHeight || 150;
  const ancho = tarjeta.offsetWidth || 340;
  let x = Math.max(12, Math.min(r.left, window.innerWidth - ancho - 12));
  let y = r.bottom + margen + 10;
  if (y + alto > window.innerHeight - 12) y = r.top - margen - alto - 10;
  if (y < 12) { y = Math.max(12, (window.innerHeight - alto) / 2); x = (window.innerWidth - ancho) / 2; }
  tarjeta.style.left = `${x}px`;
  tarjeta.style.top = `${y}px`;
}

export function arrancarVisita(forzar = false) {
  if (capa) return;
  try {
    if (!forzar && localStorage.getItem('pletina-visita')) return;
  } catch {
    return; // sin almacenamiento no se puede recordar: mejor no insistir nunca
  }

  paso = 0;
  capa = document.createElement('div');
  capa.className = 'visita';
  capa.innerHTML = `
    <div class="visita-foco"></div>
    <div class="visita-tarjeta" role="dialog" aria-label="Visita guiada">
      <h3></h3>
      <p></p>
      <div class="visita-pie">
        <span class="visita-paso"></span>
        <button class="visita-salta">Saltar</button>
        <button class="visita-sigue">Siguiente</button>
      </div>
    </div>
  `;
  document.body.appendChild(capa);

  capa.querySelector('.visita-sigue').addEventListener('click', avanzar);
  capa.querySelector('.visita-salta').addEventListener('click', terminar);
  capa.addEventListener('pointerdown', (evento) => {
    // Un clic fuera de la tarjeta también avanza: la visita no atrapa.
    if (!evento.target.closest('.visita-tarjeta')) avanzar();
  });
  window.addEventListener('resize', pintarPaso);
  window.addEventListener('keydown', alTeclear, true);

  pintarPaso();
}
