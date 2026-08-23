# Roadmap

Seis fases después del esqueleto. Cada fase termina en algo **usable y
demostrable** —nada queda "al 80 %"—, y su lista de "hecho" es la
definición, no una intención. El orden responde a una decisión explícita del
proyecto: **la suite propia y el mastering van pronto** (F2), antes que la
grabación, el MIDI y los VST3.

La "v1.0" es la release que cierra F6. Las fases no son sprints con fecha:
son puertas que se cruzan cuando su lista está en verde.

## F0 · Esqueleto — (esta fase) ✅

La casa con los cimientos y la primera nota.

- [x] Monorepo `ui/` + `motor/` + `docs/` + CI.
- [x] Documentación de planificación completa (estos docs).
- [x] UI Electron que abre y se recorre: transporte, rail, arrangement en
      canvas con pistas de demostración, mesa plegable, tira de dispositivos.
      Tema oscuro de familia, fuentes y branding heredados.
- [x] Motor que compila (JUCE + Tracktion Engine fijados), habla el
      protocolo por stdio, carga un WAV, reproduce, y pasa su autoprueba
      sin tarjeta de sonido (`--prueba`).
- [x] Puente: la UI lanza el motor, hace `hola`, tocar/parar mueve el
      transporte y los VU del máster se mueven con los medidores.
- [x] Tests: lógica pura de la UI, contrato del protocolo entre lados, y
      autoprueba del motor en CI (Linux y Windows).

## F1 · Editar y sonar — núcleo construido, rematando

De maqueta a editor de verdad: la primera vez que alguien monta un tema.

- [x] Importar WAV/MP3/FLAC/OGG (diálogo y arrastrar); el audio se copia a
      `media/` del proyecto.
- [x] Arrangement real: mover (también entre pistas), recortar por los
      bordes, dividir (T), borrar, imán a rejilla con zoom adaptativo,
      selección (con Mayús para acumular).
- [x] Duplicar (Ctrl+D) y fundidos de entrada/salida con asas arrastrables.
- [ ] Pendiente de arrangement: crossfade automático al solapar, loop de
      clip, selección por goma.
- [x] Formas de onda reales por picos calculados por el motor (por archivo
      fuente, con caché en la interfaz).
- [x] Pistas: crear, borrar, renombrar (doble clic), color por índice,
      mute/solo. Pendiente: reordenar, armado (con la grabación, F4).
- [x] Mesa: fader, pan, VU por pista, máster con VU y sonoridad.
      Pendiente: sends y retornos.
- [x] Cadenas de inserción por pista y máster con la tira operativa
      (insertar, quitar, bypass, mandos generados del descriptor).
      Pendiente: reordenar arrastrando.
- [x] **Primeros efectos de la suite: EQ Ocho (4 bandas), Compresor, Techo
      (lookahead) y Medidor (LUFS M/S/I según BS.1770 + pico)**. Pendiente:
      el analizador de espectro del EQ (F2).
- [x] Utilidad (ganancia/fase/anchura/mono).
- [x] Transporte: bucle dibujable en la mitad alta de la regla con botón de
      ciclo, metrónomo, tempo.
- [x] Proyecto: carpeta con media/, guardar/abrir y autoguardado cada dos
      minutos con la escritura segura de T.E.
- [x] Deshacer/rehacer (Ctrl+Z / Ctrl+Y) por transacciones.
- [x] Exportar WAV del máster. Pendiente: MP3 (necesita LAME) y stems (F2).
- [x] Atajos: espacio, Inicio, T, Supr, Ctrl+Z/Y/S, M, B, zoom con Ctrl+rueda.

**Hecho cuando**: se montan 8 stems en un arreglo, se mezclan con EQ y
compresión, se exporta a −14 LUFS comprobados por el Medidor, y el proyecto
reabre idéntico. La autoprueba del motor (`--prueba`) ya cubre el ciclo
entero en CI: proyecto → importar → dividir/mover/recortar → deshacer →
suite en el máster → sonar → exportar → guardar → reabrir.

## F2 · Mastering y mezcla a fondo — prioridad declarada · CONSTRUIDA

La suite se hace adulta. Es la fase que define el carácter del producto.

- [x] Reverbs y compañía: **Placa** (FDN de 8 líneas), **Sala** (FDN con
      reflexiones tempranas y tamaño), **Convolución** (juce::dsp, IRs
      sintéticas de fábrica seleccionables desde el menú de presets, y las
      IRs WAV que dejes en la carpeta de IRs), **Delay** sincronizado al
      tempo con ping-pong, **Puerta** con histéresis y retención.
- [x] Cadena de mastering completa: **Multibanda** (cruces Linkwitz-Riley de
      4.º orden que suman plano), **Anchura** (imager M/S por bandas),
      **Chispa** (exciter), **Óxido** (cinta con pre/de-énfasis y wow),
      **Techo con detección de pico verdadero** (interpolador ×4 BS.1770),
      **Dither** TPDF con noise shaping.
- [x] **Medidor completo**: LUFS M/S/I, **LRA** (EBU Tech 3342), **pico
      verdadero** ×4, **correlación** y **espectro** de 24 bandas en la
      tarjeta. (El vectorscopio queda representado por la correlación
      numérica; su dibujo XY es un pendiente estético de F6. La validación
      contra los vectores oficiales EBU sigue pendiente como tal, pero la
      exportación normalizada se verifica en CI clavando el objetivo:
      −16.000 LUFS medidos sobre el archivo.)
- [x] EQ Dinámico (3 bandas con detector propio); Balancín (tilt).
      (Las 8 bandas y el M/S por banda del EQ Ocho quedan para F6, pulido.)
- [x] Pegamento (bus con auto-release), De-eser (partidor LR + compresión
      de agudos).
- [x] Oscilador de calibración (seno, rosa, barrido).
- [x] Carril de automatización de volumen por pista (dibujar, mover,
      Alt+clic para quitar, tecla A), sobre la API general
      `automatizacion.puntos` que automatiza volumen, pan o cualquier
      parámetro de la suite. (La grabación de movimientos en vivo, con la
      grabación de F4.)
- [x] Exportar por **stems** (pista a pista en solo, con su cadena),
      exportación con **objetivo de sonoridad** (dos pasadas, verificada) y
      **FLAC**.
- [x] Presets de fábrica y de usuario en todos los efectos (menú ▾ de cada
      tarjeta; los de usuario en la carpeta de datos).
- [x] Envíos A/B por pista con retornos que se crean solos (post-fader).
- [x] Congelar pista (experimental: el congelado de T.E. es asíncrono y su
      verificación con dispositivo real queda pendiente).

**Hecho cuando**: un máster hecho solo con la suite aguanta un A/B honesto
contra la cadena comercial de referencia del probador (pendiente: requiere
oídos humanos), y el CI verifica el ciclo de sonoridad de punta a punta
(hecho: la autoprueba exporta normalizado a −16 LUFS y lo comprueba
midiendo el archivo). Renders dorados por efecto: pendiente de F6.

## F3 · Clásicos y warp

El color vintage y el tiempo elástico.

- [ ] Suite vintage: Válvulas (Pultec), Consola (canal SSL), Remache (1176),
      Ópto (LA-2A), Lámpara (vari-mu), Eco (cinta), Muelle, Espejismo,
      Coro/Flanger/Fase, Trémolo/Autopan, Triodo, Sumadora, Machacadora,
      Peine, Multitap.
- [ ] **Warp por clip**: detección de transitorios, marcadores arrastrables,
      modos tonal/percusivo/re-pitch (RubberBand integrado; SoundTouch para
      previsualizar).
- [ ] Tempo maestro: los clips warpeados siguen el tempo del proyecto;
      cambios de tempo por tramos.
- [ ] Transposición por clip en semitonos y afinación fina; detección de
      tempo (y tonalidad orientativa) al importar.

**Hecho cuando**: un loop a 87 BPM cae en un proyecto a 120 y suena en
rejilla sin artefactos evidentes, y una mezcla coloreada solo con la suite
vintage justifica sus nombres.

## F4 · Grabar y MIDI

El DAW se llena de entradas.

- [ ] Grabación de audio: selección de dispositivo/canales (WASAPI/ASIO),
      monitorización con latencia visible, count-in, punch in/out, tomas en
      capas con elección posterior.
- [ ] Pistas MIDI: dispositivos de entrada, grabación con cuantización no
      destructiva, piano roll completo (velocidad, controladores, escala,
      arrastre de acordes), edición de compases.
- [ ] Instrumentos: **Cinta, Pads y Bruma** ([instrumentos](07-instrumentos.md)).
- [ ] Metrónomo grabable, tap tempo.
- [ ] Afinador.

**Hecho cuando**: se graba una voz sobre stems con monitorización cómoda, se
programa una batería en Pads con el piano roll, y todo sobrevive al
guardar/abrir.

## F5 · VST3 y Session View

Se abre la puerta a los plugins de fuera y al directo.

- [ ] Hosting VST3: escaneo en proceso hijo con lista negra, carpeta(s) de
      plugins configurables, editores en ventanas nativas, presets, PDC.
- [ ] Session View: rejilla de clips por pista × escenas, disparo con
      cuantización de lanzamiento, grabación de sesión a arrangement.
- [ ] Grupos de pistas; racks/cadenas con macros (8 mandos por rack).
- [ ] Side-chain enrutables entre pistas (para Puerta, compresores y Techo).

**Hecho cuando**: una sesión con 4 escenas se toca en directo, un proyecto
con VST3 de terceros cierra y reabre con todo su estado, y un plugin que
cuelga en el escaneo no tumba nada.

## F6 · Pulido y release → v1.0

Lo que separa "funciona" de "se recomienda".

- [ ] Navegador de contenidos: búsqueda, previsualización con audición,
      favoritos, arrastre universal.
- [ ] Temas claro/oscuro finos, tamaños de interfaz, mapa de atajos editable.
- [ ] Rendimiento: proyectos de 100 pistas, medidor de CPU/disco por pista,
      auditoría de PDC.
- [ ] Manual en español dentro de la app; visitas guiadas de primer arranque.
- [ ] Instaladores Windows (NSIS + portable + zip) con el workflow de
      instaladores de la familia; actualizador que avisa (no que fuerza).
- [ ] Beta pública, triaje, y corte de v1.0.

**Hecho cuando**: tres productores ajenos al proyecto terminan una canción
cada uno sin abrir otro DAW y sin preguntar nada que el manual no responda.

## Después de v1.0 (ideas aparcadas, no prometidas)

Follow actions y probabilidad en Session, CLAP, Ableton Link, control MIDI
de superficie, macOS/Linux empaquetados, editor de compases irregulares,
pistas de vídeo de referencia.
