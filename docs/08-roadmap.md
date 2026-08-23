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

## F3 · Clásicos y warp · CONSTRUIDA

El color vintage y el tiempo elástico.

- [x] Suite vintage completa, los quince: **Válvulas** (curvas Pultec con el
      truco de realzar y cortar a la vez), **Consola** (canal británico de
      cuatro bandas más paso alto), **Remache** (FET estilo 1176 con modo
      TODOS), **Ópto** (célula T4 con memoria lenta), **Lámpara** (vari-mu de
      programa), **Eco** (cinta con wow/flutter, head-bump y autooscilación),
      **Muelle** (allpasses dispersivos en serie), **Espejismo** (shimmer:
      octavador granular dentro de la FDN), **Multitap** (ecos al tempo con
      anchura), **Coro** (con modos flanger y fase), **Trémolo** (con
      autopan), **Triodo**, **Sumadora**, **Machacadora** y **Peine**
      (gráfico ISO de 31 bandas). Verificados en vivo por stdio: el Peine
      clava −12,0 dB en su banda, el Eco deja cola tras el clip y el Ópto
      reduce 13 dB. La base `PluginSuite` genera parámetros y persistencia
      de una tabla estática: un clásico nuevo es su tabla y su DSP.
- [x] **Warp por clip (autoTempo)**: el clip sigue el tempo del proyecto con
      time-stretch SoundTouch (modos `normal`/`mejor`); al cambiar el tempo,
      las posiciones se remapean a compás y el clip estira. La autoprueba lo
      verifica: un clip de 0,5 s a 120 BPM queda en 0,400 s exactos al subir
      el proyecto a 150, y sobrevive al guardar/reabrir.
- [x] Transposición por clip en semitonos (el motor acepta decimales; la
      caja de la interfaz va en enteros) y detección de tempo al importar
      (SoundTouch BPMDetect sobre el primer minuto; solo se apunta si sale
      un BPM sensato). Inspector de clip en la tira: BPM fuente corregible,
      warp y transposición.
- [ ] Pendiente de F3 (se queda para más adelante, dicho claro): marcadores
      de warp arrastrables con detección de transitorios y re-pitch por
      marcador (RubberBand sigue sin integrarse: el stretch es SoundTouch),
      cambios de tempo por tramos, afinación fina en centésimas y tonalidad
      orientativa al importar.

**Hecho cuando**: un loop a 87 BPM cae en un proyecto a 120 y suena en
rejilla sin artefactos evidentes (verificado en CI con la proporción exacta
120→150; la escucha con material real, pendiente de oídos), y una mezcla
coloreada solo con la suite vintage justifica sus nombres (los quince suenan
y persisten; el juicio de carácter, pendiente de A/B humano).

## F4 · Grabar y MIDI · CONSTRUIDA (núcleo)

El DAW se llena de entradas.

- [x] **Grabación de audio**: armar por pista (botón ● de la mesa, con la
      entrada asignada sola), grabar desde el transporte (⏺ o tecla R, con
      claqueta de un compás con Mayús), y la toma aparece como clip con su
      WAV dentro de la carpeta del proyecto. Verificada de punta a punta en
      CI: la bomba del modo sin audio tiene entradas y una señal de prueba
      (`dispositivos.tono`) que hace de micrófono, y la autoprueba graba
      0,8 s y comprueba el archivo y su pico. Pendiente: selección de
      dispositivo/canales concretos (WASAPI/ASIO con hardware real),
      monitorización con latencia visible, punch in/out, tomas en capas.
- [x] **Pistas MIDI**: clips MIDI (doble clic en el vacío los crea), piano
      roll flotante (pintar, mover, transponer, alargar, borrar con
      Alt+clic, velocidad con la rueda, rejilla elegible), cuantización no
      destructiva por clip con las divisiones del motor, y **grabación MIDI**
      verificada en CI con la nota de prueba por la entrada MIDI hospedada.
      Pendiente: dispositivos MIDI físicos con hardware real, controladores
      (CC), escalas, arrastre de acordes, edición de compases.
- [x] Instrumentos: **Cinta, Pads y Bruma** construidos en su v1 y sonando
      (la autoprueba lo verifica con la Bruma en solo; el detalle honesto de
      qué tiene cada uno y qué falta, en
      [instrumentos](07-instrumentos.md)).
- [ ] Metrónomo grabable y tap tempo (el metrónomo existe desde F1; el tap,
      pendiente).
- [ ] Afinador.

**Hecho cuando**: se graba una voz sobre stems con monitorización cómoda
(pendiente: exige hardware real), se programa una batería en Pads con el
piano roll (hecho), y todo sobrevive al guardar/abrir (verificado en CI:
notas, cuantización e instrumentos vuelven enteros).

## F5 · VST3 y Session View · CONSTRUIDA (núcleo)

Se abre la puerta a los plugins de fuera y al directo.

- [x] **Hosting VST3**: escaneo en **proceso hijo** (el propio binario con
      `--escanear-vst3`, un proceso por candidato: el que revienta o se
      cuelga va a la lista negra sin llevarse el motor), carpetas
      configurables (`vst.carpetas`, con las del sistema por defecto),
      catálogo persistente entre arranques, inserción desde el menú de la
      tira como cualquier efecto (`vst:<id>`), parámetros descritos y
      automatizables con la misma maquinaria que la suite. Verificado en CI
      con un **VST3 real compilado ex profeso** (atenúa 6 dB exactos): se
      escanea, se inserta, el proyecto se guarda y reabre con él dentro, y
      el render sale a la mitad clavada. Pendiente: la ventana nativa con el
      editor gráfico del plugin (hoy la tira pinta sus parámetros
      genéricos), presets del propio plugin.
- [x] **Session View**: rejilla escenas × pistas (⊞ o Tab), lanzar ranura o
      escena entera con **cuantización de lanzamiento** elegible (None…1/16),
      copiar clips del arreglo a las ranuras, parar por pista o todo, y los
      estados (tocando/encolado) latiendo en vivo con los medidores. La
      autoprueba lanza una ranura sin cuantizar y verifica que queda
      "tocando" con el transporte en marcha. Pendiente: grabar la sesión al
      arrangement, disparo por teclado/MIDI.
- [ ] Grupos de pistas; racks/cadenas con macros (8 mandos por rack).
- [ ] Side-chain enrutables entre pistas (para Puerta, compresores y Techo).

**Hecho cuando**: una sesión con 4 escenas se toca en directo (hecho en
contenedor; el directo con manos y oídos, pendiente de hardware), un
proyecto con VST3 de terceros cierra y reabre con todo su estado (verificado
con el VST3 de prueba; con plugins comerciales reales, pendiente de una
máquina con ellos), y un plugin que cuelga en el escaneo no tumba nada (el
proceso hijo muere solo y queda vetado; probado con el escaneo real).

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
