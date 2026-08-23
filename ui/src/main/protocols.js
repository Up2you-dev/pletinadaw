import path from 'node:path';
import { readFile } from 'node:fs/promises';
import { protocol } from 'electron';

export const APP_SCHEME = 'pletinadaw';

/**
 * El esquema propio que sirve la interfaz, herencia directa del reproductor:
 * al no ser `file://`, la ventana tiene un origen real, los módulos ES cargan
 * y la CSP se puede apretar de verdad. El DAW aún no sirve media por esquema
 * (las ondas las pintará el canvas con datos del motor); cuando toque, tendrá
 * su `pletinadaw-media://` con las mismas reglas que el reproductor.
 */
export function registerSchemes() {
  protocol.registerSchemesAsPrivileged([
    {
      scheme: APP_SCHEME,
      privileges: { standard: true, secure: true, supportFetchAPI: true, codeCache: true },
    },
  ]);
}

const UI_TYPES = {
  '.html': 'text/html; charset=utf-8',
  '.js': 'text/javascript; charset=utf-8',
  '.css': 'text/css; charset=utf-8',
  '.svg': 'image/svg+xml',
  '.woff2': 'font/woff2',
  '.json': 'application/json; charset=utf-8',
  '.png': 'image/png',
  '.txt': 'text/plain; charset=utf-8',
};

const notFound = () => new Response('no existe', { status: 404 });

export function handleProtocols({ rendererDir, sharedDir }) {
  // `/shared/…` son los módulos comunes a interfaz y proceso principal; el
  // resto sale de la carpeta de la interfaz. Cualquier otra ruta no existe.
  const roots = [
    { prefix: 'shared/', dir: sharedDir },
    { prefix: '', dir: rendererDir },
  ];

  protocol.handle(APP_SCHEME, async (request) => {
    const url = new URL(request.url);
    const relative = decodeURIComponent(url.pathname).replace(/^\/+/, '') || 'index.html';
    const root = roots.find((candidate) => relative.startsWith(candidate.prefix));
    const target = path.join(root.dir, relative.slice(root.prefix.length));
    // Nada fuera de esas dos carpetas, pase lo que pase en la URL.
    if (!target.startsWith(root.dir + path.sep)) return notFound();
    try {
      const body = await readFile(target);
      const type = UI_TYPES[path.extname(target).toLowerCase()] || 'application/octet-stream';
      return new Response(body, { headers: { 'content-type': type } });
    } catch {
      return notFound();
    }
  });
}

export const appUrl = (file = 'index.html') => `${APP_SCHEME}://app/${file}`;
