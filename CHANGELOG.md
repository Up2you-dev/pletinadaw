# Registro de cambios

## v0.1.0 — 2026-08-28

Primera versión etiquetada de Pletina DAW: un DAW para Windows (Electron +
motor nativo sobre Tracktion Engine) con la disciplina de que todo lo que se
afirma está verificado por una prueba automática.

### Motor

- Proyectos (carpeta con `proyecto.tracktionedit` y `media/`), pistas,
  clips de audio con recorte/división/duplicado/fundidos, deshacer/rehacer
  transaccional y autoguardado.
- Transporte con metrónomo, bucle y grabación de audio y MIDI con cuenta de
  entrada; entradas verificables sin tarjeta de sonido (modo `--sin-audio`
  con bomba interna, tono y nota de prueba inyectados).
- Warp de clips: detección de BPM, tempo automático, cambio de tono y
  time-stretching (SoundTouch), verificado con duraciones exactas.
- Clips MIDI con piano roll (notas, velocidades, cuantización no
  destructiva) y tres instrumentos propios: Bruma (sustractivo polifónico),
  Cinta (sampler con muestra de fábrica) y Pads (16 percusiones).
- Session View: escenas × ranuras con lanzamiento cuantizado.
- Grupos de pistas: carpetas de submezcla con cadena de efectos, fader,
  mute/solo y VU propios; agrupar, meter, sacar y deshacer, con la mezcla
  del grupo direccionable desde el mismo protocolo que las pistas.
- Racks con 8 macros: envolver un tramo de la cadena en un rack, asignar
  macros a parámetros de los plugins contenidos (suma sobre el valor base),
  editar los parámetros de dentro y deshacer el rack recuperando la cadena
  en línea; todo sobrevive a guardar y reabrir.
- Suite propia de 37 efectos e instrumentos, del canal a la masterización:
  EQ de 8 bandas, compresor, techo (limitador con true peak), puerta,
  utilidad, saturaciones y cintas de válvulas, reverbs (placa, muelle,
  convolución, espejismo), delays (eco, multitap), modulación (coro,
  trémolo, peine), mastering multibanda y medidor con LUFS/LRA/true
  peak/correlación/espectro/vectorscopio. Side-chain real en compresor,
  techo y puerta, también en el bus de un grupo.
- Automatización de volumen/pan y de cualquier parámetro de la suite;
  presets por plugin; congelado de pistas; envíos a dos buses de retorno.
- Hosting VST3 con escaneo en procesos hijo (lista negra de plugins que
  revientan) y catálogo persistente.
- Exportación offline a WAV del máster o por stems, con normalización de
  sonoridad opcional (LUFS objetivo).
- Audición previa independiente del proyecto y medición de carga de CPU.

### Interfaz

- Arreglo con ondas reales, rejilla imantada, edición de clips, sangría de
  grupos, reordenado de pistas arrastrando, piano roll completo y vista de
  sesión (Tab) con lanzamiento por teclado (1–8, 0).
- Mesa de mezclas con VU por pista y por grupo, envíos, armado, congelado,
  agrupado con un gesto y máster con LUFS y vectorscopio; tira de
  dispositivos con tarjetas por plugin, side-chain (◁), presets, tarjeta de
  rack con 8 mandos y asignaciones, y menú con los VST3 escaneados.
- Navegador de sonidos con búsqueda, favoritos, audición previa,
  importación y arrastre al arreglo; visita guiada para la primera vez;
  manual de usuario dentro de la app (tecla ?); indicador de CPU en la
  cabecera; tema claro/oscuro; atajos documentados.
- Recuperación tras caída del motor: réplica del proyecto y reapertura
  automática al reconectar.

### Verificación

- Autoprueba del motor sin tarjeta de sonido, con criterios cuantitativos:
  reproduce, edita, deshace, renderiza, persiste, automatiza, normaliza,
  warpea, graba audio y MIDI, suena por instrumentos, lanza en sesión,
  audiciona, hospeda un VST3 con ganancia exacta, agrupa (caída ×0.1
  exacta en el bus del grupo y vuelta al deshacer) y enracka (envoltura
  transparente, macro que mueve +9 dB clavados, persistencia y deshecho).
- Humo de la suite entera: 38 renders sin silencios, NaN ni desbordes, con
  renders dorados (RMS de referencia con tolerancia fina) por efecto.
- Prueba de carga: 100 pistas sonando con el transporte clavado al reloj.
- Prueba de hostilidad del protocolo: entradas basura (JSON roto, métodos
  falsos, índices imposibles, grupos y racks inexistentes) respondidas con
  error limpio y el motor cuerdo.
- CI en GitHub Actions (Linux y Windows) y workflow de instaladores de
  Windows (NSIS, portable y zip) con el motor empaquetado.

### Conocido y pendiente

- Los instaladores de Windows se construyen con el workflow `instaladores`
  sobre esta etiqueta; quedan colgados del release cuando GitHub Actions
  esté disponible para el repositorio.
- Pendientes documentados en el roadmap: editores nativos de VST3, racks
  anidados, presets de rack y la beta con usuarios reales.
