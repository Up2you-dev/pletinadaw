# El motor

## Sobre qué se construye

**JUCE 8** (framework de audio en C++) y **Tracktion Engine v3** encima: el
motor open source que impulsa el DAW Waveform. La alternativa era escribir a
mano el modelo de edición, el grafo con compensación de latencia, el hosting
de plugins, el warp y el render — años de trabajo antes de la primera
canción. Con Tracktion Engine todo eso existe, está probado en un DAW
comercial y se licencia en GPLv3 ([licencias](11-licencias.md)).

Las versiones van **fijadas** en `motor/CMakeLists.txt`: Tracktion Engine a
su etiqueta (`v3.2.0`) y JUCE al commit exacto que señala el submódulo de esa
etiqueta, porque T.E. sigue la rama `develop` de JUCE y una release "cercana"
no garantiza compilar. Subir de versión es un cambio deliberado, no un
efecto del día.

## El modelo que regala Tracktion Engine

| Concepto | Qué nos da |
|---|---|
| `Edit` | El proyecto entero: pistas, clips, tempo, automatización, con deshacer integrado |
| `AudioTrack` / clips | Pistas de audio y MIDI, clips con recortes, fundidos, loop y **warp** |
| Grafo de reproducción | Mezcla con compensación de latencia (PDC) automática |
| `TransportControl` | Tocar, parar, localizar, loop, grabación |
| Hosting | VST3 (y LADSPA) con escaneo, presets y editores |
| Time-stretch | SoundTouch integrado; RubberBand enchufable |
| Render | Exportación offline del máster o por pistas (stems) |

Nuestro trabajo no es reinventar esto: es dárselo a la interfaz por el
[protocolo](04-protocolo.md), construir encima la
[suite de efectos propia](06-efectos.md) como plugins internos
(`tracktion::Plugin` / `juce::AudioProcessor`), y afinar los detalles que
hacen DAW: imanes, atajos, flujos.

## Audio en cada sistema

- **Windows**: WASAPI por defecto (exclusivo o compartido). **ASIO** para
  latencia seria: el SDK de Steinberg no se puede redistribuir, así que el
  binario publicado se compila con ASIO **solo** cuando el SDK está presente
  (`motor/README` de F1 documentará la ruta); sin él, WASAPI manda.
- **Linux**: ALSA (y JACK si está). Es la plataforma del CI y de los
  contenedores de desarrollo.
- **macOS**: CoreAudio, gratis con JUCE, cuando toque empaquetar para Mac.

### El modo sin tarjeta

`pletina-motor --sin-audio` no abre dispositivo: usa la interfaz de audio
hospedada de T.E. y una **bomba** —un hilo que pide bloques al grafo al ritmo
que marcaría el hardware—. El transporte avanza, los medidores miden y la
autoprueba (`--prueba`) verifica de verdad que un WAV cargado suena: genera
un seno, lo carga como clip, reproduce medio segundo y comprueba posición y
pico. Es lo que corre el CI en cada push.

## Grabación (F4)

Entradas del dispositivo → `InputDevice` de T.E. asignable a pista, con
monitorización, count-in del metrónomo y punch. El archivo cae en `media/`
del proyecto desde el primer segundo (nada de "exportar la toma": ya está en
disco).

## MIDI e instrumentos (F4)

Dispositivos MIDI del sistema (JUCE `MidiInput`) enrutados a pistas; clips
MIDI con piano roll en la interfaz; los instrumentos propios
([instrumentos](07-instrumentos.md)) son plugins internos, iguales a los
efectos a ojos del grafo.

## Hosting VST3 (F5)

- **Escaneo en proceso hijo**: escanear es ejecutar binarios ajenos; uno que
  cuelga no puede llevarse el motor. Lista negra persistente de los que
  fallan.
- **Editores**: ventanas nativas abiertas por el motor, flotantes sobre la
  interfaz.
- **PDC**: la latencia que declare cada plugin entra en la compensación del
  grafo, también en las cadenas del máster.
- VST2 queda fuera (Steinberg ya no licencia su SDK); CLAP se estudiará
  cuando el ecosistema lo pida.

## Time-stretch y tono

- **SoundTouch** ya viene activado (`TRACKTION_ENABLE_TIMESTRETCH_SOUNDTOUCH`):
  rápido, calidad de previsualización digna.
- **RubberBand** (GPL, como nosotros) se integrará en F3 como calidad alta
  para warp y transposición; los modos por clip (tonal, percusivo,
  re-pitch) se mapean a sus opciones.
- El **warp** usa los marcadores de T.E.: detectar transitorios, anclar, y
  estirar el clip al tempo del proyecto sin cambiar el tono (o cambiándolo,
  si el modo es re-pitch).

## Render y exportación (F1 en adelante)

Render offline del máster a WAV (16/24/32 bits, con dither de la suite) y
MP3/FLAC; por **stems** en F2 (una pasada por pista o bus). La exportación
con objetivo de sonoridad (p. ej. −14 LUFS) mide con el propio medidor de la
suite: el mismo código que enseña el número lo aplica.
