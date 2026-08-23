import { spawn } from 'node:child_process';
import { existsSync } from 'node:fs';
import path from 'node:path';

import { codificarPeticion, decodificarLinea, partirLineas } from '../shared/protocolo.js';

/**
 * El lado de acá del motor: encontrar el binario, lanzarlo, hablarle por stdio
 * y sobrevivir a su muerte. La interfaz nunca ve el proceso: ve un estado
 * (`maqueta`, `arrancando`, `conectado`, `caido`) y un flujo de eventos.
 */

/** Dónde puede estar `pletina-motor`, de más explícito a más buscado. */
function rutasCandidatas(raiz) {
  const exe = process.platform === 'win32' ? 'pletina-motor.exe' : 'pletina-motor';
  const candidatas = [];
  if (process.env.PLETINA_MOTOR) candidatas.push(process.env.PLETINA_MOTOR);
  // Empaquetada: el instalador deja el motor en resources/motor/.
  if (process.resourcesPath) candidatas.push(path.join(process.resourcesPath, 'motor', exe));
  for (const configuracion of ['Release', 'Debug', '']) {
    candidatas.push(path.join(raiz, 'motor', 'build', 'pletina-motor_artefacts', configuracion, exe));
  }
  return candidatas;
}

export function crearMotor({ raiz, alRecibirEvento, alCambiarEstado }) {
  const binario = rutasCandidatas(raiz).find((ruta) => ruta && existsSync(ruta));

  let proceso = null;
  let resto = '';
  let siguienteId = 1;
  let intentos = 0;
  let apagando = false;
  const pendientes = new Map();

  let estado = { estado: binario ? 'arrancando' : 'maqueta', binario: binario || null, info: null };

  function cambiar(cambios) {
    estado = { ...estado, ...cambios };
    alCambiarEstado(estado);
  }

  function arrancar() {
    if (!binario || apagando) return;

    const args = process.env.PLETINA_SIN_AUDIO === '1' ? ['--sin-audio'] : [];
    proceso = spawn(binario, args, { stdio: ['pipe', 'pipe', 'pipe'] });
    cambiar({ estado: 'arrancando' });

    proceso.stdout.on('data', (trozo) => {
      const partido = partirLineas(resto, trozo.toString('utf8'));
      resto = partido.resto;
      for (const linea of partido.lineas) recibir(linea);
    });

    // El diagnóstico del motor no es protocolo: se reenvía al de la interfaz.
    proceso.stderr.on('data', (trozo) => {
      if (process.env.PLETINA_DEV === '1') console.error(`[motor] ${trozo.toString('utf8').trimEnd()}`);
    });

    proceso.on('exit', (codigo) => {
      proceso = null;
      for (const { rechazar } of pendientes.values()) rechazar(new Error('el motor se ha ido'));
      pendientes.clear();
      resto = '';
      if (apagando) return;
      cambiar({ estado: 'caido', info: { codigo } });
      // Reintentos con espera creciente; a la tercera se deja en paz y se avisa.
      if (intentos < 3) {
        intentos += 1;
        setTimeout(arrancar, 500 * 2 ** intentos);
      }
    });
  }

  async function recibir(linea) {
    const mensaje = decodificarLinea(linea);

    if (mensaje.tipo === 'respuesta') {
      const pendiente = pendientes.get(mensaje.id);
      if (!pendiente) return;
      pendientes.delete(mensaje.id);
      if (mensaje.error) pendiente.rechazar(new Error(mensaje.error.mensaje || 'error del motor'));
      else pendiente.resolver(mensaje.resultado);
      return;
    }

    if (mensaje.tipo === 'evento') {
      if (mensaje.nombre === 'arrancado') {
        // Apretón de manos: con el `hola` contestado, el motor es de fiar.
        try {
          const info = await orden('hola');
          intentos = 0;
          cambiar({ estado: 'conectado', info });
        } catch {
          cambiar({ estado: 'caido', info: null });
        }
      }
      alRecibirEvento({ nombre: mensaje.nombre, datos: mensaje.datos });
      return;
    }

    if (process.env.PLETINA_DEV === '1') console.error(`[motor·ruido] ${mensaje.linea}`);
  }

  function orden(metodo, params) {
    if (!proceso) return Promise.reject(new Error('el motor no está'));
    const id = siguienteId++;
    const linea = codificarPeticion(id, metodo, params);
    return new Promise((resolver, rechazar) => {
      pendientes.set(id, { resolver, rechazar });
      proceso.stdin.write(linea);
      // Sin respuesta en 10 s, la promesa no se queda colgada para siempre.
      setTimeout(() => {
        if (pendientes.delete(id)) rechazar(new Error(`sin respuesta a ${metodo}`));
      }, 10000);
    });
  }

  function apagar() {
    apagando = true;
    if (!proceso) return;
    // Por las buenas con `salir`; si en un segundo sigue, por las malas.
    orden('salir').catch(() => {});
    const condenado = proceso;
    setTimeout(() => {
      if (condenado.exitCode === null) condenado.kill();
    }, 1000);
  }

  arrancar();

  return {
    orden,
    apagar,
    estado: () => estado,
  };
}
