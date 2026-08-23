import { contextBridge, ipcRenderer } from 'electron';

/**
 * La única puerta entre la interfaz y el sistema, como en el reproductor:
 * el renderizador no tiene Node y lo que no esté aquí, no se puede hacer.
 */
const invoke = (channel, ...args) => ipcRenderer.invoke(channel, ...args);

function subscribe(channel) {
  return (listener) => {
    const handler = (_event, payload) => listener(payload);
    ipcRenderer.on(channel, handler);
    return () => ipcRenderer.removeListener(channel, handler);
  };
}

contextBridge.exposeInMainWorld('pletinadaw', {
  plataforma: process.platform,

  motor: {
    /** Manda una orden del protocolo y devuelve su resultado (o lanza). */
    orden: (metodo, params) => invoke('motor:orden', metodo, params),
    estado: () => invoke('motor:estado'),
    alRecibirEvento: subscribe('motor:evento'),
    alCambiarEstado: subscribe('motor:estado'),
  },

  version: () => invoke('app:version'),
});
