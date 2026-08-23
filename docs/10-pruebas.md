# Estrategia de pruebas

Herencia directa del reproductor: la lógica pura se prueba a conciencia, las
costuras entre procesos tienen tests de contrato, y hay al menos una prueba
que arranca la aplicación **de verdad**. El DAW añade una categoría nueva y
central: el DSP se prueba por render.

## En la interfaz (`ui/`)

- **Unitarias (vitest)** sobre `src/shared/`: formato y aritmética de tiempo
  musical (`tiempo.js`), codificación del protocolo (`protocolo.js`), el
  almacén de estado. Sin Electron, sin DOM: puro Node.
- **Contrato del protocolo** (`test/contrato.test.js`): lee la tabla
  `METODOS` de `motor/src/protocolo.h` y la compara con la lista de
  `src/shared/protocolo.js`. Un método añadido en un solo lado rompe la
  build. Es el patrón del reproductor (menú⇄manejadores) aplicado a la
  costura UI⇄motor.
- **Smoke** (`npm run smoke`): arranca Electron real (con `xvfb-run` si no
  hay pantalla), espera la señal de la interfaz, verifica cero errores de
  consola y guarda una captura como artefacto.

## En el motor (`motor/`)

- **Autoprueba** (`pletina-motor --prueba`): sin tarjeta de sonido, genera un
  WAV, lo carga como clip, reproduce con la bomba interna y verifica que la
  posición avanza y que hay señal en los medidores. Corre en cada push del
  CI en Linux y Windows.
- **Renders dorados (desde F1)**: cada efecto e instrumento de la suite
  renderiza offline sus presets de fábrica sobre señales fijas (seno,
  barrido, ruido rosa, un loop real) y el resultado se compara a tolerancia
  contra el render de referencia versionado. Cambiar el sonido sin querer se
  vuelve imposible; cambiarlo queriendo obliga a regenerar la referencia en
  el mismo commit, donde se ve.
- **Vectores oficiales (F2)**: el Medidor pasa los vectores de prueba EBU
  R128 / ITU-R BS.1770 (LUFS y true-peak) en CI. Medir bien no es opinable.
- **PDC (F2)**: impulsos por cadenas con latencia declarada; la salida debe
  quedar alineada a la muestra.
- **Unitarias C++** donde haya lógica no-DSP con enjundia (modelo de
  proyecto, protocolo), con el runner que ya trae el ecosistema (doctest via
  T.E. o Catch2, a decidir en F1).

## Reglas

- Ningún test depende de un dispositivo de audio real: la bomba
  (`--sin-audio`) existe exactamente para esto.
- Los renders dorados son deterministas: misma semilla, mismos hilos de
  render, mismo binario ⇒ mismos bits (la tolerancia cubre diferencias
  legítimas de FPU entre plataformas, no descuidos).
- El CI es la definición de "funciona": lo que no corre en CI no está hecho.
- Cada bug con historia (el que costó una tarde) deja un test con su nombre,
  como la prueba de arrastre del reproductor.
