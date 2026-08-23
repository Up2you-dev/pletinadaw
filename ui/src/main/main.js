import path from 'node:path';
import { fileURLToPath } from 'node:url';
import { readFile, writeFile } from 'node:fs/promises';
import { app, BrowserWindow, dialog, ipcMain, nativeTheme } from 'electron';

import { registerSchemes, handleProtocols, appUrl } from './protocols.js';
import { crearMotor } from './motor.js';

/**
 * Proceso principal del DAW: la ventana, el esquema propio que sirve la
 * interfaz, y la tutela del motor. La regla de la casa sigue en pie: el
 * renderizador no tiene Node y todo lo que puede hacer está enumerado en el
 * preload.
 */

const here = path.dirname(fileURLToPath(import.meta.url));
const raizUi = path.join(here, '..', '..');
const raizRepo = path.join(raizUi, '..');

// El DAW es oscuro por defecto (ver docs/09-decisiones.md, ADR-006).
nativeTheme.themeSource = 'dark';

registerSchemes();

let ventana = null;
let motor = null;

function crearVentana() {
  ventana = new BrowserWindow({
    width: 1440,
    height: 900,
    minWidth: 960,
    minHeight: 620,
    show: false,
    backgroundColor: '#101018',
    icon: path.join(raizUi, 'build', 'icons', '512x512.png'),
    // La barra de título la dibuja la aplicación; el sistema superpone sus
    // botones en Windows y los semáforos en macOS, como en el reproductor.
    titleBarStyle: 'hidden',
    titleBarOverlay: process.platform === 'win32'
      ? { color: '#101018', symbolColor: '#ECEAF4', height: 46 }
      : undefined,
    webPreferences: {
      preload: path.join(here, '..', 'preload', 'preload.mjs'),
      contextIsolation: true,
      nodeIntegration: false,
      sandbox: false,
    },
  });

  ventana.once('ready-to-show', () => ventana.show());
  ventana.on('closed', () => { ventana = null; });
  // PLETINA_VISTA=sesion abre en Session View (lo usa el humo para capturarla).
  ventana.loadURL(appUrl() + (process.env.PLETINA_VISTA === 'sesion' ? '?vista=sesion' : ''));
}

app.whenReady().then(() => {
  handleProtocols({
    rendererDir: path.join(raizUi, 'src', 'renderer'),
    sharedDir: path.join(raizUi, 'src', 'shared'),
  });

  motor = crearMotor({
    raiz: raizRepo,
    alRecibirEvento: (evento) => ventana?.webContents.send('motor:evento', evento),
    alCambiarEstado: (estado) => ventana?.webContents.send('motor:estado', estado),
  });

  ipcMain.handle('motor:orden', (_evento, metodo, params) => motor.orden(metodo, params));
  ipcMain.handle('motor:estado', () => motor.estado());
  ipcMain.handle('app:version', () => app.getVersion());

  // El navegador de sonidos: una carpeta elegida y sus audios (primer nivel
  // y un nivel de subcarpetas; con audición y favoritos ya vendrá el fino).
  ipcMain.handle('sonidos:elegirCarpeta', async () => {
    const r = await dialog.showOpenDialog(ventana, {
      title: 'Carpeta de sonidos',
      properties: ['openDirectory'],
    });
    if (r.canceled) return null;
    const carpeta = r.filePaths[0];
    const AUDIO = new Set(['.wav', '.mp3', '.flac', '.ogg', '.aif', '.aiff']);
    const archivos = [];
    const { readdir } = await import('node:fs/promises');
    const explorar = async (dir, hondura) => {
      let entradas = [];
      try {
        entradas = await readdir(dir, { withFileTypes: true });
      } catch {
        return;
      }
      for (const entrada of entradas) {
        const ruta = path.join(dir, entrada.name);
        if (entrada.isDirectory() && hondura > 0) await explorar(ruta, hondura - 1);
        else if (entrada.isFile() && AUDIO.has(path.extname(entrada.name).toLowerCase())) {
          archivos.push({ nombre: entrada.name, ruta });
          if (archivos.length >= 500) return;
        }
      }
    };
    await explorar(carpeta, 1);
    archivos.sort((a, b) => a.nombre.localeCompare(b.nombre));
    return { carpeta, archivos };
  });

  // El manual, dentro de la app: del repo en desarrollo, de resources/ empaquetada.
  ipcMain.handle('manual:leer', async () => {
    const candidatas = [
      process.resourcesPath ? path.join(process.resourcesPath, 'manual.md') : null,
      path.join(raizRepo, 'docs', 'manual.md'),
    ].filter(Boolean);
    for (const ruta of candidatas) {
      try {
        return await readFile(ruta, 'utf8');
      } catch {
        // la siguiente candidata
      }
    }
    return null;
  });

  // Diálogos del sistema: la interfaz pide, el proceso principal pregunta.
  ipcMain.handle('dialogo:importarAudio', async () => {
    const r = await dialog.showOpenDialog(ventana, {
      title: 'Importar audio',
      properties: ['openFile', 'multiSelections'],
      filters: [{ name: 'Audio', extensions: ['wav', 'mp3', 'flac', 'ogg', 'aif', 'aiff'] }],
    });
    return r.canceled ? [] : r.filePaths;
  });

  ipcMain.handle('dialogo:nuevoProyecto', async () => {
    const r = await dialog.showSaveDialog(ventana, {
      title: 'Nuevo proyecto: elige carpeta y nombre',
      buttonLabel: 'Crear proyecto',
      nameFieldLabel: 'Nombre',
    });
    return r.canceled ? null : r.filePath;
  });

  ipcMain.handle('dialogo:abrirProyecto', async () => {
    const r = await dialog.showOpenDialog(ventana, {
      title: 'Abrir proyecto',
      properties: ['openFile'],
      filters: [{ name: 'Proyecto Pletina', extensions: ['tracktionedit'] }],
    });
    return r.canceled ? null : r.filePaths[0];
  });

  ipcMain.handle('dialogo:exportar', async () => {
    const r = await dialog.showSaveDialog(ventana, {
      title: 'Exportar mezcla a WAV',
      defaultPath: 'mezcla.wav',
      filters: [{ name: 'WAV', extensions: ['wav'] }],
    });
    return r.canceled ? null : r.filePath;
  });

  crearVentana();

  // Modo humo: arrancar de verdad y, si el motor está, montar una sesión
  // pequeña por el protocolo (importar un WAV generado, dividirlo, Medidor en
  // el máster, tocar) y capturar con las ondas reales y el transporte
  // corriendo. La captura es la prueba: si algo del puente se rompe, se ve.
  const captura = process.env.PLETINA_SMOKE;
  if (captura) {
    ventana.webContents.once('did-finish-load', () => {
      setTimeout(async () => {
        try {
          const { escribirWavDePrueba } = await import('./wav-prueba.js');
          const wav = path.join(app.getPath('temp'), 'pletinadaw-humo.wav');
          await escribirWavDePrueba(wav, 2);
          const clip = await motor.orden('clip.importar', { pista: 0, ruta: wav, inicio: 0 });
          const otro = await motor.orden('clip.importar', { pista: 1, ruta: wav, inicio: 1 });
          await motor.orden('clip.dividir', { id: clip.id, segundos: 1 });
          await motor.orden('clip.fundidos', { id: otro.id, entrada: 0.4, salida: 0.5 });
          await motor.orden('clip.warp', { id: otro.id, bpmFuente: 120, autoTempo: true, transposicion: 3 });
          await motor.orden('plugin.insertar', { pista: -1, tipo: 'valvulas' });
          await motor.orden('plugin.insertar', { pista: -1, tipo: 'multibanda', indice: 1 });
          await motor.orden('plugin.insertar', { pista: -1, tipo: 'anchura', indice: 2 });
          await motor.orden('plugin.insertar', { pista: -1, tipo: 'techo', indice: 3 });
          await motor.orden('plugin.insertar', { pista: -1, tipo: 'medidor', indice: 4 });
          await motor.orden('plugin.insertar', { pista: 1, tipo: 'placa' });
          await motor.orden('plugin.insertar', { pista: 1, tipo: 'eco', indice: 1 });
          const patron = await motor.orden('clip.midi.crear', { pista: 2, inicio: 0, compases: 2 });
          await motor.orden('clip.midi.notas', { id: patron.id, notas: [
            { nota: 36, inicio: 0, duracion: 0.4, velocidad: 110 },
            { nota: 42, inicio: 0.5, duracion: 0.2, velocidad: 80 },
            { nota: 38, inicio: 1, duracion: 0.4, velocidad: 100 },
            { nota: 42, inicio: 1.5, duracion: 0.2, velocidad: 80 },
            { nota: 36, inicio: 2, duracion: 0.4, velocidad: 110 },
            { nota: 42, inicio: 2.5, duracion: 0.2, velocidad: 80 },
            { nota: 38, inicio: 3, duracion: 0.4, velocidad: 100 },
            { nota: 39, inicio: 3.5, duracion: 0.5, velocidad: 90 },
            { nota: 36, inicio: 4, duracion: 0.4, velocidad: 110 },
            { nota: 42, inicio: 4.5, duracion: 0.2, velocidad: 80 },
            { nota: 38, inicio: 5, duracion: 0.4, velocidad: 100 },
            { nota: 48, inicio: 6, duracion: 1.6, velocidad: 95 },
          ] });
          await motor.orden('plugin.insertar', { pista: 2, tipo: 'pads' });
          await motor.orden('pista.armar', { pista: 3, activo: true });
          await motor.orden('sesion.escenas', { numero: 3 });
          await motor.orden('sesion.poner', { pista: 0, escena: 0, desdeClip: clip.id });
          await motor.orden('sesion.poner', { pista: 2, escena: 0, desdeClip: patron.id });
          await motor.orden('sesion.poner', { pista: 1, escena: 1, desdeClip: otro.id });
          await motor.orden('sesion.cuantizacion', { nombre: 'None' });
          await motor.orden('sesion.lanzar', { escena: 0 });
          await motor.orden('pista.envio', { pista: 0, bus: 0, nivelDb: -14 });
          await motor.orden('automatizacion.puntos', { pista: 0, parametro: 'volumen',
            puntos: [{ t: 0, v: 0 }, { t: 1.5, v: -14 }, { t: 3, v: -2 }] });
          await motor.orden('transporte.bucle', { activo: true, inicio: 0, fin: 3 });
          await motor.orden('transporte.tocar');
        } catch {
          // sin motor, el humo captura la maqueta: también vale
        }
      }, 900);

      setTimeout(async () => {
        try {
          const imagen = await ventana.webContents.capturePage();
          await writeFile(captura, imagen.toPNG());
          app.exit(0);
        } catch (error) {
          console.error('humo: no se pudo capturar', error);
          app.exit(1);
        }
      }, 3400);
    });
  }
});

app.on('window-all-closed', () => {
  motor?.apagar();
  app.quit();
});

app.on('before-quit', () => motor?.apagar());
