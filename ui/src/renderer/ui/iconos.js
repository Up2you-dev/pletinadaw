/** Iconografía en línea: un solo trazo, 24×24, sin dependencias externas. */
const svg = (body, extra = '') => `<svg viewBox="0 0 24 24" aria-hidden="true"${extra}>${body}</svg>`;

export const ICO = {
  tocar: svg('<path d="M8 5.3v13.4L19 12z" fill="currentColor" stroke="none"/>'),
  parar: svg('<rect x="6.5" y="6.5" width="11" height="11" rx="1.5" fill="currentColor" stroke="none"/>'),
  alPrincipio: svg('<path d="M6 5.5v13" stroke-width="2"/><path d="M19 6.4v11.2L9.6 12z" fill="currentColor" stroke="none"/>'),
  grabar: svg('<circle cx="12" cy="12" r="5.6" fill="currentColor" stroke="none"/>'),
  ciclo: svg('<path d="M17 3.5 19.8 6 17 8.5"/><path d="M19.5 6H7.5A3.5 3.5 0 0 0 4 9.5V11"/><path d="M7 20.5 4.2 18 7 15.5"/><path d="M4.5 18h12a3.5 3.5 0 0 0 3.5-3.5V13"/>'),
  mesa: svg('<path d="M6 20V14M6 10V4M12 20v-9M12 7V4M18 20v-4M18 12V4"/><path d="M3.5 14h5M9.5 7h5M15.5 16h5"/>'),
  carpeta: svg('<path d="M3.5 6.5A1.5 1.5 0 0 1 5 5h4l2 2.5h8A1.5 1.5 0 0 1 20.5 9v9A1.5 1.5 0 0 1 19 19.5H5A1.5 1.5 0 0 1 3.5 18z"/>'),
  onda: svg('<path d="M3 12h2l2-6 3 13 3-9 2 4h6"/>'),
  enchufe: svg('<path d="M9 4v5M15 4v5M7 9h10v3a5 5 0 0 1-10 0z"/><path d="M12 17v3"/>'),
  teclado: svg('<rect x="3.5" y="6" width="17" height="12" rx="1.5"/><path d="M8 6v7M12 6v7M16 6v7"/>'),
  mas: svg('<path d="M12 5v14M5 12h14"/>'),
  x: svg('<path d="M6 6l12 12M18 6 6 18"/>'),
  nuevo: svg('<path d="M13 3.5H6.5A1.5 1.5 0 0 0 5 5v14a1.5 1.5 0 0 0 1.5 1.5h11A1.5 1.5 0 0 0 19 19V9.5z"/><path d="M13 3.5V9.5H19"/>'),
  guardar: svg('<path d="M5 5a1.5 1.5 0 0 1 1.5-1.5H16L20.5 8v11a1.5 1.5 0 0 1-1.5 1.5H6.5A1.5 1.5 0 0 1 5 19z"/><path d="M8.5 3.5V8h7V3.5M8 20.5v-6h8v6"/>'),
  importar: svg('<path d="M12 4v10M8 10.5 12 14.5l4-4"/><path d="M4.5 16v3A1.5 1.5 0 0 0 6 20.5h12a1.5 1.5 0 0 0 1.5-1.5v-3"/>'),
  exportar: svg('<path d="M12 14V4M8 7.5 12 3.5l4 4"/><path d="M4.5 16v3A1.5 1.5 0 0 0 6 20.5h12a1.5 1.5 0 0 0 1.5-1.5v-3"/>'),
  metronomo: svg('<path d="M10 3.5h4L18 20.5H6z"/><path d="M12 15.5 16.5 6"/><circle cx="12" cy="16.8" r="1.1" fill="currentColor" stroke="none"/>'),
};
