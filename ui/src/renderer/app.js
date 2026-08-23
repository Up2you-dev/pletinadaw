import { estado, cambiar, suscribir, posicionParaPintar } from './estado.js';
import { montarTransporte, pintarBoton, pintarReloj, pintarChipMotor } from './ui/transporte.js';
import { montarRail } from './ui/rail.js';
import { montarArreglo, pintar as pintarArreglo } from './ui/arreglo.js';
import { montarMesa, pintarVU } from './ui/mesa.js';
import { montarTira } from './ui/tira.js';
import { aviso } from './ui/dom.js';

/**
 * Arranque de la interfaz: montar los paneles, enchufar el motor si está y
 * dejar corriendo el bucle de pintado. Con motor, tocar/parar viajan por el
 * protocolo y la posición vuelve confirmada; sin motor, un reloj local mueve
 * la maqueta para que todo se pueda recorrer igual.
 */

const puente = window.pletinadaw;
document.documentElement.dataset.platform = puente?.plataforma || 'web';

/* ---------- órdenes de transporte ---------- */

const conectado = () => estado.motor.estado === 'conectado';

async function conmutar() {
  if (conectado()) {
    try {
      const metodo = estado.reproduciendo ? 'transporte.parar' : 'transporte.tocar';
      const resultado = await puente.motor.orden(metodo);
      cambiar({
        reproduciendo: !!resultado.reproduciendo,
        segundos: resultado.segundos ?? estado.segundos,
        refrescadoEn: performance.now(),
      });
    } catch (error) {
      aviso(`El motor no responde: ${error.message}`);
    }
  } else {
    // Maqueta: el cursor corre en local, y a mucha honra.
    cambiar({
      segundos: posicionParaPintar(),
      reproduciendo: !estado.reproduciendo,
      refrescadoEn: performance.now(),
    });
  }
  pintarBoton();
}

async function irA(segundos) {
  if (conectado()) {
    try {
      const resultado = await puente.motor.orden('transporte.irA', { segundos });
      cambiar({ segundos: resultado.segundos ?? segundos, refrescadoEn: performance.now() });
      return;
    } catch (error) {
      aviso(`El motor no responde: ${error.message}`);
    }
  }
  cambiar({ segundos, refrescadoEn: performance.now() });
}

/* ---------- montar paneles ---------- */

montarTransporte({ alConmutar: conmutar, alIrA: irA });
montarRail();
montarArreglo({ alIrA: irA });
montarMesa();
montarTira();
pintarChipMotor();

/* ---------- el motor, si está ---------- */

if (puente) {
  puente.motor.estado().then((m) => cambiar({ motor: m }));
  puente.motor.alCambiarEstado((m) => cambiar({ motor: m }));

  puente.motor.alRecibirEvento(({ nombre, datos }) => {
    if (nombre === 'medidores') {
      cambiar({
        segundos: datos.segundos ?? estado.segundos,
        reproduciendo: !!datos.reproduciendo,
        refrescadoEn: performance.now(),
        medidores: { izq: datos.izq ?? -100, der: datos.der ?? -100 },
      });
    }
  });
}

suscribir((_estado, cambios) => {
  if ('motor' in cambios) pintarChipMotor();
  if ('reproduciendo' in cambios) pintarBoton();
});

/* ---------- atajos ---------- */

window.addEventListener('keydown', (evento) => {
  if (evento.target.matches('input, textarea')) return;
  if (evento.code === 'Space') { evento.preventDefault(); conmutar(); }
  else if (evento.code === 'Home') irA(0);
  else if (evento.key === 'm' || evento.key === 'M') {
    document.getElementById('app').classList.toggle('sin-mesa');
  } else if (evento.key === 'b' || evento.key === 'B') {
    document.getElementById('app').classList.toggle('sin-rail');
  }
});

/* ---------- bucle de pintado ---------- */

function cuadro(ahora) {
  pintarReloj(ahora);
  pintarArreglo(ahora);
  pintarVU();
  requestAnimationFrame(cuadro);
}
requestAnimationFrame(cuadro);
