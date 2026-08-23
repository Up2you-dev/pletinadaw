import { deflateSync } from 'node:zlib';
import { mkdirSync, writeFileSync } from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

/**
 * Genera el icono de Pletina DAW en los formatos de los tres sistemas.
 *
 * Mismo método que el reproductor —distancias con signo, por código, sin
 * binarios opacos— y misma familia: el cuadrado índigo. La variante DAW cambia
 * las barras de ecualizador por un mini-arrangement: tres clips horizontales
 * sobre sus pistas y el cursor de reproducción ámbar cruzándolos.
 */
const here = path.dirname(fileURLToPath(import.meta.url));

/* ----------------------------------------------------------------- dibujo */

const clamp = (v, a = 0, b = 1) => Math.min(b, Math.max(a, v));
const mix = (a, b, t) => a.map((c, i) => c + (b[i] - c) * t);

/** Distancia con signo a un rectángulo redondeado, centrado en el origen. */
function sdRoundRect(px, py, halfW, halfH, radius) {
  const qx = Math.abs(px) - halfW + radius;
  const qy = Math.abs(py) - halfH + radius;
  const outside = Math.hypot(Math.max(qx, 0), Math.max(qy, 0));
  return outside + Math.min(Math.max(qx, qy), 0) - radius;
}

const FONDO_A = [58, 63, 212];
const FONDO_B = [26, 29, 120];
const AMBAR = [240, 179, 87];
const BLANCO = [255, 255, 255];

/**
 * Tres clips sobre tres pistas y el cursor encima. Los clips van a distintas
 * alturas y anchos —como un arreglo de verdad— y el cursor pasa por el medio.
 */
function render(size) {
  const data = Buffer.alloc(size * size * 4);
  const alto = 0.085;
  const clips = [
    { x: -0.10, y: -0.21, medioAncho: 0.175, color: BLANCO },
    { x: 0.06, y: 0.0, medioAncho: 0.235, color: BLANCO },
    { x: -0.045, y: 0.21, medioAncho: 0.145, color: BLANCO },
  ];
  const cursorX = 0.16;
  // Cobertura del píxel a partir de la distancia con signo (negativa = dentro).
  const cobertura = (d) => clamp(0.5 - d * size);

  for (let y = 0; y < size; y += 1) {
    for (let x = 0; x < size; x += 1) {
      const nx = (x + 0.5) / size - 0.5;
      const ny = (y + 0.5) / size - 0.5;

      const aFondo = cobertura(sdRoundRect(nx, ny, 0.5, 0.5, 0.225));
      let color = mix(FONDO_A, FONDO_B, clamp((nx + ny + 1) / 2));

      for (const clip of clips) {
        const aClip = cobertura(sdRoundRect(nx - clip.x, ny - clip.y, clip.medioAncho, alto, 0.05));
        if (aClip > 0) color = mix(color, clip.color, aClip);
      }

      const aCursor = cobertura(sdRoundRect(nx - cursorX, ny, 0.031, 0.315, 0.03));
      if (aCursor > 0) color = mix(color, AMBAR, aCursor);

      const i = (y * size + x) * 4;
      data[i] = Math.round(color[0]);
      data[i + 1] = Math.round(color[1]);
      data[i + 2] = Math.round(color[2]);
      data[i + 3] = Math.round(aFondo * 255);
    }
  }
  return data;
}

/* -------------------------------------------------------------------- PNG */

const CRC_TABLE = (() => {
  const table = new Int32Array(256);
  for (let n = 0; n < 256; n += 1) {
    let c = n;
    for (let k = 0; k < 8; k += 1) c = c & 1 ? 0xedb88320 ^ (c >>> 1) : c >>> 1;
    table[n] = c;
  }
  return table;
})();

function crc32(buffer) {
  let c = 0xffffffff;
  for (const byte of buffer) c = CRC_TABLE[(c ^ byte) & 0xff] ^ (c >>> 8);
  return (c ^ 0xffffffff) >>> 0;
}

function chunk(type, body) {
  const head = Buffer.alloc(8);
  head.writeUInt32BE(body.length, 0);
  head.write(type, 4, 4, 'ascii');
  const crc = Buffer.alloc(4);
  crc.writeUInt32BE(crc32(Buffer.concat([Buffer.from(type, 'ascii'), body])), 0);
  return Buffer.concat([head, body, crc]);
}

function png(size, rgba) {
  const ihdr = Buffer.alloc(13);
  ihdr.writeUInt32BE(size, 0);
  ihdr.writeUInt32BE(size, 4);
  ihdr[8] = 8;   // bits por canal
  ihdr[9] = 6;   // color: RGBA
  const raw = Buffer.alloc(size * (size * 4 + 1));
  for (let y = 0; y < size; y += 1) {
    raw[y * (size * 4 + 1)] = 0; // filtro: ninguno
    rgba.copy(raw, y * (size * 4 + 1) + 1, y * size * 4, (y + 1) * size * 4);
  }
  return Buffer.concat([
    Buffer.from([0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a]),
    chunk('IHDR', ihdr),
    chunk('IDAT', deflateSync(raw, { level: 9 })),
    chunk('IEND', Buffer.alloc(0)),
  ]);
}

/* ------------------------------------------------------------- ICO e ICNS */

function ico(entries) {
  const head = Buffer.alloc(6);
  head.writeUInt16LE(0, 0);
  head.writeUInt16LE(1, 2);
  head.writeUInt16LE(entries.length, 4);
  let offset = 6 + entries.length * 16;
  const dir = [];
  for (const entry of entries) {
    const e = Buffer.alloc(16);
    e[0] = entry.size >= 256 ? 0 : entry.size;
    e[1] = entry.size >= 256 ? 0 : entry.size;
    e[4] = 1;   // planos
    e.writeUInt16LE(32, 6);
    e.writeUInt32LE(entry.data.length, 8);
    e.writeUInt32LE(offset, 12);
    offset += entry.data.length;
    dir.push(e);
  }
  return Buffer.concat([head, ...dir, ...entries.map((e) => e.data)]);
}

function icns(entries) {
  const bodies = entries.map(({ type, data }) => {
    const head = Buffer.alloc(8);
    head.write(type, 0, 4, 'ascii');
    head.writeUInt32BE(data.length + 8, 4);
    return Buffer.concat([head, data]);
  });
  const total = bodies.reduce((sum, b) => sum + b.length, 0) + 8;
  const head = Buffer.alloc(8);
  head.write('icns', 0, 4, 'ascii');
  head.writeUInt32BE(total, 4);
  return Buffer.concat([head, ...bodies]);
}

/* ---------------------------------------------------------------- salida */

const sizes = [16, 32, 48, 64, 128, 256, 512, 1024];
const pngs = new Map(sizes.map((size) => [size, png(size, render(size))]));

mkdirSync(path.join(here, 'icons'), { recursive: true });
for (const size of sizes) {
  writeFileSync(path.join(here, 'icons', `${size}x${size}.png`), pngs.get(size));
}
writeFileSync(path.join(here, 'icon.png'), pngs.get(1024));
writeFileSync(path.join(here, 'icon.ico'), ico([16, 32, 48, 64, 128, 256].map((size) => ({ size, data: pngs.get(size) }))));
writeFileSync(path.join(here, 'icon.icns'), icns([
  ['icp4', 16], ['icp5', 32], ['ic07', 128], ['ic08', 256], ['ic09', 512], ['ic10', 1024],
].map(([type, size]) => ({ type, data: pngs.get(size) }))));

console.log('iconos generados:', sizes.join(', '));
