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

## F1 · Editar y sonar

De maqueta a editor de verdad: la primera vez que alguien monta un tema.

- [ ] Importar WAV/MP3/FLAC/OGG (diálogo y arrastrar); el audio se copia a
      `media/` del proyecto.
- [ ] Arrangement real: mover, recortar por los bordes, dividir, duplicar,
      fundidos de entrada/salida y crossfade, loop de clip, imán a rejilla
      con zoom adaptativo, selección múltiple.
- [ ] Formas de onda por pirámide de picos calculada por el motor.
- [ ] Pistas: crear, borrar, reordenar, renombrar, color, mute/solo/arm.
- [ ] Mesa completa: fader, pan, medidor por pista, 2 sends, retornos, y
      canal máster.
- [ ] Cadenas de inserción por pista y en el máster, con la tira de
      dispositivos operativa (insertar, quitar, reordenar, bypass).
- [ ] **Primeros efectos de la suite: EQ Ocho (4 bandas + analizador),
      Compresor, Techo (base) y Medidor (LUFS + pico)** — desde F1 se puede
      mezclar y dejar un máster básico decente.
- [ ] Utilidad (ganancia/fase/anchura).
- [ ] Transporte completo: loop de proyecto, metrónomo, tempo y compás.
- [ ] Proyecto: carpeta, guardar/abrir, autoguardado atómico, recuperación.
- [ ] Deshacer/rehacer de todo lo anterior.
- [ ] Exportar WAV y MP3 del máster.
- [ ] Atajos de edición (herramientas, zoom, navegación).

**Hecho cuando**: se montan 8 stems en un arreglo, se mezclan con EQ y
compresión, se exporta a −14 LUFS comprobados por el Medidor, y el proyecto
reabre idéntico.

## F2 · Mastering y mezcla a fondo — prioridad declarada

La suite se hace adulta. Es la fase que define el carácter del producto.

- [ ] Cadena de mastering completa: **Multibanda, Anchura (imager M/S),
      Chispa (exciter), Óxido (cinta), Techo true-peak con estilos de
      release, Dither con noise shaping**.
- [ ] **Medidor completo**: LUFS M/S/I + LRA, true-peak BS.1770-4, espectro,
      correlación, vectorscopio, historia de sonoridad. Validado contra los
      vectores EBU en CI.
- [ ] EQ Ocho a 8 bandas con M/S por banda; EQ Dinámico; Balancín.
- [ ] Pegamento (bus SSL), Puerta, De-eser.
- [ ] Reverbs Placa y Sala; **Convolución** con IRs de fábrica; Delay
      sincronizado; Oscilador de calibración.
- [ ] Carriles de automatización (volumen, pan, sends y cualquier parámetro
      de la suite), con dibujo a mano y grabación de movimientos.
- [ ] Exportar por **stems**; exportación con objetivo de sonoridad; FLAC.
- [ ] Presets de fábrica y de usuario para todos los efectos; la cadena de
      mastering de fábrica montada en el máster de cada proyecto nuevo.
- [ ] Congelar pista (render + bypass) para aliviar CPU.

**Hecho cuando**: un máster hecho solo con la suite aguanta un A/B honesto
contra la cadena comercial de referencia del probador, y el CI verifica
renders dorados y vectores EBU de todos los efectos de la fase.

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
