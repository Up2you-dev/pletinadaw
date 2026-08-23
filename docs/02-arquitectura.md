# Arquitectura

## Dos procesos

```
┌────────────────────────────── Electron ──────────────────────────────┐
│  main (Node)                                                         │
│  · ventana, menú, diálogos de archivo                                │
│  · lanza y supervisa el motor (spawn, reinicio, apagado)             │
│  · reparte el protocolo: órdenes ↓, respuestas y eventos ↑           │
│      ▲ contextBridge (preload: única superficie expuesta)            │
│  renderer (sin Node)                                                 │
│  · transporte, navegador, arrangement en canvas, mesa, dispositivos  │
└───────────────────────────────┬──────────────────────────────────────┘
                                │ stdio · NDJSON (docs/04-protocolo.md)
┌───────────────────────────────▼──────────────────────────────────────┐
│  pletina-motor (C++20 · JUCE + Tracktion Engine)                     │
│  · hilo de mensajes: dueño del Edit, atiende las órdenes             │
│  · hilo(s) de audio: WASAPI/ASIO/ALSA, o bomba interna sin tarjeta   │
│  · grafo con compensación de latencia, medidores, y en el futuro:    │
│    grabación, MIDI, suite de efectos, hosting VST3, render           │
└──────────────────────────────────────────────────────────────────────┘
```

## Por qué separados

- **Un plugin VST3 colgado no puede llevarse la interfaz por delante.** El
  motor puede morir y reaparecer; la interfaz lo cuenta y sigue en pie. (El
  escaneo de plugins irá aún más lejos: proceso propio, porque escanear es
  ejecutar código ajeno a ciegas.)
- **Los editores de plugins son ventanas nativas.** Las abre el motor, que es
  quien aloja el plugin; Electron no tiene que hacer malabares con HWNDs
  ajenos.
- **El motor sin cabeza vale solo.** El mismo binario que usa la interfaz
  puede renderizar un proyecto por línea de órdenes o correr la autoprueba
  del CI.
- **Cada mundo con su toolchain.** La interfaz itera al ritmo de guardar y
  recargar; el motor compila con optimización y sin prisas.

## Ciclo de vida

1. Electron `main` abre la ventana y busca `pletina-motor` (rutas de
   compilación conocidas o la variable `PLETINA_MOTOR`).
2. Si existe: `spawn`, el motor emite el evento `arrancado`, la interfaz
   manda `hola` y pinta el chip de estado con versión y modo de audio. Si
   no: **modo maqueta**, todo abre y se puede recorrer, y el chip dice que el
   motor no está.
3. Las órdenes de la interfaz viajan como peticiones con `id`; el motor
   contesta a cada una y, aparte, emite eventos (posición, medidores) sin que
   nadie se los pida.
4. Si el motor muere, `main` lo detecta, avisa a la interfaz y lo relanza con
   una espera creciente; si stdin se le cierra al motor (la interfaz se fue),
   el motor se apaga solo. Un motor huérfano sonando sin ventana no es
   aceptable.

## Hilos del motor

- **Hilo de mensajes (JUCE).** El único que toca el Edit y el modelo. Todas
  las órdenes del protocolo aterrizan aquí.
- **Hilo de audio.** El del dispositivo real (WASAPI/ASIO/ALSA) o, con
  `--sin-audio`, una *bomba* propia: un hilo que pide bloques al motor al
  ritmo que marcaría el hardware. Gracias a ella el transporte avanza y los
  medidores miden en un contenedor sin tarjeta, que es como corre el CI.
- **Lector de stdin.** Deserializa líneas y las salta al hilo de mensajes.
  stdout tiene candado: una línea de protocolo no se parte jamás.

Regla de oro heredada del ecosistema JUCE: el hilo de audio no bloquea, no
reserva memoria y no toca el modelo; se comunica por colas y atómicos.

## Estado y persistencia (diseño para F1)

- **El proyecto es una carpeta**: `proyecto.pletina` (JSON del modelo),
  `media/` con el audio importado o grabado, `congelados/` para renders de
  pistas. Copiar la carpeta es copiar el proyecto.
- **Escritura atómica** como en el reproductor: temporal + rename, nunca un
  proyecto a medias. Autoguardado cada pocos minutos y al cerrar.
- **La verdad del modelo vive en el motor** (el Edit de Tracktion). La
  interfaz mantiene una réplica ligera para pintar, alimentada por eventos;
  cada edición viaja como orden y vuelve como confirmación. Deshacer/rehacer
  los gestiona el motor, que ya sabe hacerlo por Edit.

## Rendimiento en la interfaz

- El arrangement y las formas de onda se pintan en `<canvas>` con
  `requestAnimationFrame` y escala de `devicePixelRatio`; nada de miles de
  nodos DOM.
- Los picos de onda los calcula el motor por niveles de zoom (pirámide de
  picos) y viajan una vez; el canvas solo dibuja.
- Medidores y posición llegan a ~15 Hz y se interpolan al pintar, para que el
  cursor corra fino sin ahogar el canal.
