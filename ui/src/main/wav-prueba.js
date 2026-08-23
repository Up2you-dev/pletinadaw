import { writeFile } from 'node:fs/promises';

/**
 * Escribe un WAV estéreo PCM16 de prueba: un tono con envolvente rítmica y un
 * poco de ruido, para que el humo tenga algo que importar y una onda con
 * dibujo que capturar. Solo lo usa el modo humo; no toca nada del usuario.
 */
export async function escribirWavDePrueba(ruta, segundos = 2, frecuencia = 44100) {
  const n = Math.floor(segundos * frecuencia);
  const datos = Buffer.alloc(n * 4); // 2 canales × 16 bits

  let ruido = 22222;
  for (let i = 0; i < n; i += 1) {
    const t = i / frecuencia;
    // Envolvente de corcheas a 120 BPM para que la onda tenga golpes visibles.
    const golpe = Math.exp(-6 * ((t * 4) % 1));
    ruido = (ruido * 48271) % 2147483647;
    const azar = (ruido / 2147483647 - 0.5) * 0.12;
    const v = (Math.sin(2 * Math.PI * 220 * t) * 0.45 + azar) * golpe;
    const muestra = Math.max(-32768, Math.min(32767, Math.round(v * 32767)));
    datos.writeInt16LE(muestra, i * 4);
    datos.writeInt16LE(muestra, i * 4 + 2);
  }

  const cabecera = Buffer.alloc(44);
  cabecera.write('RIFF', 0);
  cabecera.writeUInt32LE(36 + datos.length, 4);
  cabecera.write('WAVE', 8);
  cabecera.write('fmt ', 12);
  cabecera.writeUInt32LE(16, 16);
  cabecera.writeUInt16LE(1, 20);            // PCM
  cabecera.writeUInt16LE(2, 22);            // canales
  cabecera.writeUInt32LE(frecuencia, 24);
  cabecera.writeUInt32LE(frecuencia * 4, 28);
  cabecera.writeUInt16LE(4, 32);
  cabecera.writeUInt16LE(16, 34);
  cabecera.write('data', 36);
  cabecera.writeUInt32LE(datos.length, 40);

  await writeFile(ruta, Buffer.concat([cabecera, datos]));
  return ruta;
}
