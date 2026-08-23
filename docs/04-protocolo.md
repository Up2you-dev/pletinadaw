# El protocolo UI⇄motor

Líneas JSON por stdio (NDJSON): una línea, un mensaje. Legible con un `tee`,
depurable con un `echo`, y sin dependencias a ninguno de los dos lados. La
versión de estreno es deliberadamente pequeña; crecerá con las fases, no
antes.

## Formas de mensaje

```jsonc
// petición (UI → motor): id correlaciona la respuesta
{"id": 7, "metodo": "transporte.tocar", "params": {}}

// respuesta (motor → UI): o resultado o error, nunca los dos
{"id": 7, "resultado": {"reproduciendo": true, "segundos": 0.0}}
{"id": 7, "error": {"codigo": -32000, "mensaje": "no existe el archivo: …"}}

// evento (motor → UI): sin id, nadie lo pidió
{"evento": "medidores", "datos": {"segundos": 1.25, "reproduciendo": true, "izq": -12.4, "der": -12.8}}
```

Códigos de error al estilo JSON-RPC: `-32700` JSON ilegible, `-32601` método
desconocido, `-32000` fallo de la operación (el mensaje viene en castellano y
se puede enseñar tal cual).

## Métodos (estado F3)

| Zona | Métodos | Notas |
|---|---|---|
| saludo | `hola`, `dispositivos.listar` | `hola` anuncia `capacidades[]` y la `suite[]` disponible |
| proyecto | `proyecto.nuevo {carpeta}`, `proyecto.abrir {ruta}`, `proyecto.guardar` | el proyecto es una carpeta con `proyecto.tracktionedit` y `media/` |
| modelo | `pistas.listar` | la foto completa (ver evento `modelo`) |
| pistas | `pista.crear`, `pista.borrar`, `pista.renombrar`, `pista.mezcla {volumenDb, pan, mute, solo}`, `pista.envio {pista, bus, nivelDb}`, `pista.congelar {pista, activo}` | `pista.mezcla` no emite `modelo`: los faders disparan decenas por segundo. `pista.envio` crea el retorno del bus si no existe |
| clips | `clip.importar {pista, ruta, inicio}`, `clip.mover {id, inicio, pista?}`, `clip.recortar {id, inicio, fin}`, `clip.dividir {id, segundos}`, `clip.duplicar {id}`, `clip.fundidos {id, entrada, salida}`, `clip.borrar {id}`, `clip.picos {id, porSegundo}`, `clip.warp {id, autoTempo?, bpmFuente?, transposicion?, modo?}` | importar copia el audio a `media/` y detecta el tempo de origen (SoundTouch, primer minuto); `clip.warp` enciende el autoTempo (el clip sigue el tempo del proyecto), corrige el BPM de origen, transpone en semitonos y elige el modo de stretch (`normal`/`mejor`) |
| suite | `plugin.insertar {pista, tipo, indice}`, `plugin.quitar`, `plugin.parametro {parametro, valor}`, `plugin.activar`, `plugin.presets {tipo}`, `plugin.preset.guardar {pista, indice, nombre}`, `plugin.preset.cargar {pista, indice, nombre}` | `pista = -1` es el máster; los tipos válidos vienen en `hola.suite`; presets de fábrica y de usuario |
| automatización | `automatizacion.puntos {pista, parametro, puntos[{t, v}]}` | `volumen`, `pan` o cualquier parámetro de la suite; volumen en dB |
| transporte | `transporte.tocar/parar/irA/estado/tempo/metronomo/bucle` | cambiar el tempo remapea posiciones a compás y estira los clips con autoTempo |
| deshacer | `deshacer.deshacer`, `deshacer.rehacer` | cada orden mutadora es una transacción |
| render | `render.exportar {ruta, stems?, lufsObjetivo?}` | WAV o FLAC por extensión; `stems` exporta pista a pista; `lufsObjetivo` normaliza en dos pasadas y se verifica midiendo el archivo |
| fin | `salir` | |

## Eventos (estado F3)

| Evento | Cuándo | Datos |
|---|---|---|
| `arrancado` | al nacer el proceso | `version`, `audio` |
| `modelo` | tras cada orden que muta el proyecto | la foto completa: `proyecto`, `bpm`, `metronomo`, `bucle`, `pistas[]` (con `clips[]` —posición, fundidos, `autoTempo`, `transposicion`, `bpmFuente`—, `plugins[]` con sus `parametros[]` descriptores, `envios[]`, `retorno`, `congelada` y `automatizacionVolumen[]`), `master` |
| `medidores` | ~15 Hz mientras reproduce (y una última al parar) | `segundos`, `reproduciendo`, `izq`, `der` (dBFS), `pistas[]` (VU por pista) y, si hay Medidor en el máster, `lufs {m, s, i, lra, correlacion, pico, picoVerdadero}` y `espectro[24]` |
| `render.terminado` | al acabar una exportación | `ruta`, `ok` |
| `prueba` | solo con `--prueba` | los veredictos de la autoprueba |

La interfaz no conoce los efectos de nada: pinta los `parametros[]` que
describe el propio plugin (id, nombre, valor, min, max). Un efecto nuevo en
el motor aparece en la tira sin tocar una línea de la interfaz.

## El contrato es un test

La tabla de métodos vive **una vez en cada lado**: `motor/src/protocolo.h`
(la tabla `METODOS`, un método por línea) y `ui/src/shared/protocolo.js`. El
test `ui/test/contrato.test.js` lee el archivo C++ y comprueba que las dos
listas son idénticas: añadir un método en un solo lado rompe la build, que es
exactamente lo que tiene que pasar. Es el mismo patrón que el reproductor usa
entre menú y manejadores.

## Reglas

- **stdout del motor es sagrado**: solo protocolo. Diagnóstico, por stderr.
- La UI **no espera** a las respuestas para pintar (optimista donde sea
  seguro, confirmación donde no).
- Los eventos son *lo que ha pasado*, no *lo que se pidió*: la UI replica
  estado a partir de ellos y cualquier observador externo entendería la
  sesión solo con leerlos.
- `hola` lleva `capacidades` para que una UI nueva hable con un motor viejo
  sin romperse: lo que no esté anunciado, no se enseña.

## Lo que viene después (diseñado, no construido)

- **F4**: `grabacion.*` (armar pistas, monitorización, punch, tomas) y
  `midi.*` (notas del piano roll, cuantización, instrumentos).
- **F5**: `vst.*` (escaneo en proceso hijo, editores, estado) y `sesion.*`
  (escenas y disparo de clips).
- **Cuando el volumen lo pida**: canal binario aparte (memoria compartida o
  socket local) para medidores por pista, espectros y picos de onda; el canal
  de texto queda para órdenes. El diseño de mensajes no cambia: cambia el
  transporte.
