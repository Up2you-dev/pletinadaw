# Manual de Pletina DAW

El manual corto y honesto: lo que hay hoy y cómo se usa. La ayuda rápida de
atajos vive dentro de la aplicación, en la tecla `?`.

## Primeros pasos

1. **Nuevo proyecto** (icono de página, arriba a la izquierda): elige carpeta
   y nombre. Un proyecto de Pletina es una **carpeta** con
   `proyecto.tracktionedit` dentro y el audio en `media/`: se copia entera,
   se guarda entera, se lleva a otro disco entera.
2. **Importa audio** con el botón Importar o **arrastrando archivos** al
   arreglo (WAV, MP3, FLAC, OGG). El audio se copia a `media/`: el proyecto
   nunca depende de archivos sueltos de por ahí.
3. **Espacio** toca y para. La regla de arriba busca; su mitad alta dibuja
   el bucle (y el botón de ciclo lo enciende).
4. **Guarda con Ctrl+S**. Además, el proyecto se autoguarda cada dos minutos
   con la escritura segura del motor.

## Editar

- **Mover**: arrastra un clip (también a otra pista). **Recortar**: tira de
  sus bordes. **Dividir**: tecla `T` sobre el cursor. **Duplicar**: Ctrl+D.
  **Borrar**: Supr. Todo tiene **deshacer** (Ctrl+Z / Ctrl+Y).
- **Fundidos**: cada clip tiene dos asas arriba; arrástralas para el fade de
  entrada y de salida.
- **Imán**: los gestos van imantados a la rejilla, que se afina sola con el
  zoom (Ctrl+rueda).
- **Warp**: selecciona un clip y mira la primera tarjeta de la tira (abajo).
  «Warp ON» hace que el clip siga el tempo del proyecto (el BPM de origen se
  detecta al importar y se puede corregir); «Transposición» cambia el tono
  en semitonos sin cambiar la duración.

## Mezclar

- La **mesa** (tecla `M` la pliega) tiene fader, pan, mute, solo, VU por
  pista, **envíos A/B** (crean su pista de retorno solos) y el canal máster
  con **LUFS** en vivo.
- La **tira de dispositivos** (abajo) enseña la cadena de la pista
  seleccionada o del máster: **+** inserta cualquier efecto de la suite (o
  un VST3 del catálogo), el LED apaga/enciende, `▾` abre los **presets** de
  fábrica y los tuyos.
- **Automatización**: tecla `A` y dibuja la curva de volumen sobre la pista
  (clic añade punto, arrastrar mueve, Alt+clic quita).
- **Congelar**: el botón ❄ del canal (experimental).

## Grabar

1. Arma la pista con su botón **●** en la mesa (asigna la entrada sola).
2. **⏺** en el transporte (o tecla `R`) graba; con **Mayús** añade una
   claqueta de un compás. El botón late en rojo mientras graba.
3. Para con Espacio: la toma queda como un clip normal, con su WAV dentro
   de la carpeta del proyecto.

## MIDI e instrumentos

- **Doble clic en el vacío** de una pista crea un clip MIDI de un compás;
  **doble clic en un clip MIDI** abre el **piano roll**: clic pinta una
  nota, arrastrar la mueve (vertical transpone), su borde derecho la
  alarga, Alt+clic la borra y la **rueda** cambia su velocidad. La
  cuantización (no destructiva) se elige en la cabecera del panel.
- Para que suene, inserta un instrumento en la cadena de esa pista:
  **Bruma** (sinte sustractivo), **Cinta** (sampler; de fábrica trae una
  cuerda, y puede cargar tu muestra) o **Pads** (16 sonidos de batería en
  C1–D#2: bombo en C1, caja en D2, hats en F#1/G1…).
- La grabación MIDI funciona igual que la de audio: arma la pista y ⏺.

## Session View

- El botón **⊞** (o Tab) abre la rejilla de **escenas × pistas**. Clic en
  una ranura vacía copia dentro el clip seleccionado del arreglo; clic en
  una llena la **lanza** (con la cuantización de lanzamiento del pie de la
  rejilla); el ▶ de la izquierda lanza la escena entera; ⏹ lo para todo.

## VST3

- En el menú **+** de la tira, «Buscar VST3…» escanea las carpetas del
  sistema. Cada candidato se abre en un **proceso aparte**: el que revienta
  queda vetado y no se lleva la aplicación por delante. Los encontrados
  aparecen en el mismo menú, con sus parámetros pintados como los de la
  suite, y viajan dentro del proyecto al guardar.

## Exportar

El botón Exportar ofrece: **WAV del máster**, **WAV a −14 LUFS** (dos
pasadas: mide, ajusta y el resultado se verifica midiendo el archivo) y
**stems** (pista a pista, cada una con su cadena). FLAC sale solo con
ponerle la extensión `.flac` a la ruta.

## Si algo no suena

- Mira el chip de estado (arriba a la derecha): «motor · audio» es lo
  normal; «sin motor» significa que el binario no está compilado
  (`motor/README` de compilación) y la interfaz queda en maqueta.
- El Medidor solo mide si está insertado en el máster.
- En Windows, la primera salida de audio usa el dispositivo por defecto del
  sistema (WASAPI). ASIO exige compilar con su SDK (ver
  [licencias](11-licencias.md)).
