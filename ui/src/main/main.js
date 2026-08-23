import path from 'node:path';
import { fileURLToPath } from 'node:url';
import { writeFile } from 'node:fs/promises';
import { app, BrowserWindow, ipcMain, nativeTheme } from 'electron';

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
  ventana.loadURL(appUrl());
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

  crearVentana();

  // Modo humo: arrancar de verdad, darle al tocar si el motor está, capturar
  // la pantalla con el transporte corriendo y salir limpio. La captura es la
  // prueba: si el cursor no se ha movido, algo del puente se ha roto.
  const captura = process.env.PLETINA_SMOKE;
  if (captura) {
    ventana.webContents.once('did-finish-load', () => {
      setTimeout(() => motor.orden('transporte.tocar').catch(() => {}), 900);
      setTimeout(async () => {
        try {
          const imagen = await ventana.webContents.capturePage();
          await writeFile(captura, imagen.toPNG());
          app.exit(0);
        } catch (error) {
          console.error('humo: no se pudo capturar', error);
          app.exit(1);
        }
      }, 2400);
    });
  }
});

app.on('window-all-closed', () => {
  motor?.apagar();
  app.quit();
});

app.on('before-quit', () => motor?.apagar());
