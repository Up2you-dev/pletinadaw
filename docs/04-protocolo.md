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

## Métodos de F0

| Método | Params | Resultado |
|---|---|---|
| `hola` | — | `nombre`, `version`, `motor`, `audio` (`dispositivo`/`sin-audio`), `capacidades[]` |
| `dispositivos.listar` | — | `actual`, `tipo`, `frecuencia`, `bloque`, `dispositivos[]` |
| `edit.nuevo` | — | `pistas` |
| `edit.cargarAudio` | `ruta` | `duracion`, `frecuencia`, `canales` |
| `transporte.tocar` | — | estado del transporte |
| `transporte.parar` | — | estado del transporte |
| `transporte.irA` | `segundos` | estado del transporte |
| `transporte.estado` | — | `reproduciendo`, `segundos` |
| `salir` | — | `adios` |

## Eventos de F0

| Evento | Cuándo | Datos |
|---|---|---|
| `arrancado` | al nacer el proceso | `version`, `audio` |
| `medidores` | ~15 Hz mientras reproduce (y una última al parar) | `segundos`, `reproduciendo`, `izq`, `der` (dBFS) |
| `prueba` | solo con `--prueba` | `ok`, `segundos`, `pico` |

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

- **F1**: `edit.*` de edición real (mover/recortar/dividir clips, pistas,
  mezclador), `proyecto.guardar/abrir`, `render.exportar`, ondas por
  pirámide de picos (`clip.picos`).
- **F2+**: `plugin.*` (insertar, parámetros, presets) para la suite propia;
  automatización.
- **Cuando el volumen lo pida**: canal binario aparte (memoria compartida o
  socket local) para medidores por pista, espectros y picos de onda; el canal
  de texto queda para órdenes. El diseño de mensajes no cambia: cambia el
  transporte.
