# Pletina DAW

[![CI](https://github.com/Up2you-dev/pletinadaw/actions/workflows/ci.yml/badge.svg)](https://github.com/Up2you-dev/pletinadaw/actions/workflows/ci.yml)

Estación de trabajo de audio digital para Windows, de la familia
[Pletina](https://github.com/Up2you-dev/pletina). Multipista, clips, mezcla,
grabación, MIDI, warping, y una suite propia de efectos con la cadena de
mastering como pieza central. Sin nube, sin cuentas y sin IA: tus proyectos
viven en tu disco.

**Estado: F1 — editar y sonar (núcleo construido).** Ya se importa audio
(diálogo o arrastrando), se edita en el timeline (mover, recortar, dividir,
borrar, con imán y deshacer), se mezcla (faders, pan, mute/solo, VU por
pista), se procesa con los primeros efectos de la suite (EQ Ocho, Compresor,
Techo, Utilidad y el Medidor LUFS), se guarda como carpeta de proyecto y se
exporta a WAV. Los flecos de F1 y el resto del mapa están en
[`docs/08-roadmap.md`](docs/08-roadmap.md).

## Cómo está montado

Dos procesos que se reparten el trabajo, como se explica en
[`docs/02-arquitectura.md`](docs/02-arquitectura.md):

```
ui/       la interfaz: Electron, ESM, sin framework — la parte que ves
motor/    el sonido: C++20, JUCE + Tracktion Engine — la parte que suena
docs/     la planificación entera, de la visión al roadmap
```

La interfaz dibuja y ordena; el motor reproduce, graba, procesa y aloja los
plugins. Se hablan por un protocolo de líneas JSON descrito en
[`docs/04-protocolo.md`](docs/04-protocolo.md). Si el motor no está, la
interfaz abre igual en modo maqueta y lo dice en la esquina.

## Ejecutar la interfaz

Hace falta [Node 20 o superior](https://nodejs.org). Sin módulos nativos:
`npm install` no compila nada.

```powershell
cd ui
npm install          # incluye la descarga del binario de Electron
npm start            # abre la aplicación
```

Y para el resto:

```bash
npm test             # pruebas de la lógica pura y del contrato del protocolo
npm run lint         # eslint
npm run verify       # lint + pruebas + arranque real con captura
```

## Compilar el motor

Hace falta CMake 3.22+ y un compilador de C++20 (en Windows, Visual Studio
2022 con la carga de trabajo de C++). La primera configuración descarga JUCE
y Tracktion Engine fijados a versiones exactas; tarda, y las siguientes no.

```powershell
cd motor
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

El binario `pletina-motor` acaba dentro de `build/`. Para comprobar que está
vivo sin tarjeta de sonido:

```bash
./pletina-motor --prueba     # carga un WAV generado, lo reproduce y se verifica
```

La interfaz lo busca en las rutas de compilación habituales y también donde
diga la variable de entorno `PLETINA_MOTOR`.

## Qué va a ser esto

- **Lo que un DAW debe tener** — timeline de arrangement con clips, mezclador
  con sends y máster, grabación de audio, pistas MIDI con piano roll,
  automatización, exportación con stems.
- **Lo mejor de Ableton** — vista Session con lanzador de clips, warping del
  audio al tempo del proyecto, cadenas de dispositivos a pie de pantalla,
  navegador arrastrable.
- **Suite propia de efectos, con el mastering al frente** — ecualizadores
  (paramétrico con analizador, dinámico, estilo Pultec y consola), compresores
  (FET, ópto, bus VCA, multibanda), reverbs (algorítmicas y de convolución),
  delays, saturación, y una cadena de mastering completa con limitador
  true-peak, imager M/S, dither y medición LUFS conforme a EBU R128. El
  catálogo entero, con su listón de calidad, está en
  [`docs/06-efectos.md`](docs/06-efectos.md).
- **Hosting VST3** — tus plugins de terceros dentro de Pletina DAW.
- **Instrumentos** — sampler, pads de batería y un sinte sustractivo.

## Licencia

GPLv3, y con convicción: Pletina DAW es un proyecto **público y sin ánimo
de venta**. Lo exigen JUCE y Tracktion Engine en su vía gratuita y es lo que
este DAW quiere ser; implicaciones y detalles en
[`docs/11-licencias.md`](docs/11-licencias.md).
