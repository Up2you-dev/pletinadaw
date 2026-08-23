# La suite de efectos

Es la pieza por la que este DAW existe: **efectos propios, con el mastering
al frente**, construidos como plugins internos del motor. Nada de esta suite
depende de terceros comerciales; el hosting VST3 (F5) es un añadido, no una
excusa.

Los nombres en clave son de la familia "cinta" y se decidirán en frío antes
de F2; aquí van como propuesta. La referencia clásica indica el carácter que
se persigue, no una promesa de clonado circuit-exact: se modela el
comportamiento que hizo célebre a cada aparato (curvas, tiempos, saturación),
con oído y con medidas.

## El listón de calidad (contrato, no aspiración)

Todo efecto de la suite, antes de darse por hecho, cumple:

1. **Render dorado.** Un render offline determinista por preset de fábrica,
   comparado a tolerancia fija en CI. Un cambio de sonido no pasa
   desapercibido jamás.
2. **Sin zipper.** Parámetros suavizados; barrer cualquier mando durante el
   render dorado de estrés no mete escalones audibles (se verifica sobre el
   espectro del residuo).
3. **No linealidades con sobremuestreo.** Saturación, clippers y limitador
   procesan sobremuestreados (mínimo ×4) con filtros de reconstrucción
   decentes; el aliasing por encima de −80 dB respecto a la señal es un bug.
4. **Medición conforme.** LUFS momentáneo/corto/integrado según **EBU R128 /
   ITU-R BS.1770** y true-peak según BS.1770-4, **validados contra los
   vectores de prueba oficiales de la EBU** en el CI. El medidor no opina:
   mide.
5. **PDC declarada.** Todo efecto declara su latencia exacta (lookahead,
   fases lineales, sobremuestreo) y el grafo la compensa; un test lo
   comprueba alineando impulsos.
6. **Estado estable.** Guardar y reabrir un proyecto reproduce bit a bit el
   mismo render.

## Catálogo

Estado a agosto de 2026: **todo lo marcado F1, F2 o F3 está construido y se
inserta desde la tira**; la columna Fase dice cuándo llegó cada uno. Los
matices que faltan de cada pieza (el analizador y el M/S del EQ Ocho, los
estilos de release del Techo, los taps dibujables del Multitap…) y el
estado del listón de calidad están dichos, sin maquillaje, en el
[roadmap](08-roadmap.md). Del listón: el **humo de la suite entera** ya
corre en CI (`--prueba-efectos`: los 38 tipos renderizan y ninguno puede
salir mudo, desbocado ni con NaN) y la sonoridad se verifica de punta a
punta (−16 LUFS medidos sobre el archivo exportado); los renders dorados
con tolerancia fina por preset y la validación EBU con vectores oficiales
siguen pendientes.

### Ecualizadores

| Nombre | Referencia / carácter | Fase |
|---|---|---|
| **EQ Ocho** | Paramétrico de 8 bandas con analizador de espectro integrado, estilo Pro-Q: campana/shelf/corte/notch, Q libre, M/S por banda (F2) | F1 (4 bandas) → F2 |
| **EQ Dinámico** | Cada banda comprime o expande su rango — la herramienta de mastering moderna | F2 |
| **Válvulas** | Pultec EQP-1A: el truco de realzar y atenuar graves a la vez, agudos sedosos | F3 |
| **Consola** | Canal británico estilo SSL: cuatro bandas musicales + filtros, rápido y sin pensar | F3 |
| **Peine** | Gráfico de 31 bandas para afinar salas y resacas | F3 |
| **Balancín** | Tilt EQ de un mando: más oscuro ↔ más brillante | F2 |

### Dinámica

| Nombre | Referencia / carácter | Fase |
|---|---|---|
| **Compresor** | VCA limpio y didáctico con vista de transferencia y de ganancia | F1 |
| **Pegamento** | Bus VCA estilo SSL G: el "glue" de la mezcla, ataque lento, auto-release | F2 |
| **Remache** | FET estilo 1176: ataque en microsegundos, la agresividad clásica, modo "all buttons" | F3 |
| **Ópto** | LA-2A: dos mandos, la voz por delante, release en dos etapas | F3 |
| **Lámpara** | Vari-mu: compresión de programa, densidad de máster antiguo | F3 |
| **Multibanda** | 3–5 bandas con crossover de fase corregida, para mezcla y mastering | F2 |
| **Puerta** | Gate/expander con sidechain filtrable e histéresis | F2 |
| **De-eser** | Detección por banda, split o wideband | F2 |

### Reverbs

| Nombre | Referencia / carácter | Fase |
|---|---|---|
| **Placa** | Plate densa estilo EMT 140: la voz que flota | F2 |
| **Sala** | Algorítmica FDN con early reflections ajustables: de habitación a catedral | F2 |
| **Convolución** | Carga de IRs (WAV) con true-stereo, recorte y damping; IRs de fábrica incluidas | F2 |
| **Muelle** | Spring de guitarra y dub, con su "boing" | F3 |
| **Espejismo** | Shimmer con pitch en la cola de la reverb | F3 |

### Delays y modulación

| Nombre | Referencia / carácter | Fase |
|---|---|---|
| **Eco** | Eco de cinta estilo RE-201: wow/flutter, saturación de la cinta, autooscilación | F3 |
| **Delay** | Digital sincronizado al tempo, ping-pong, filtros en la realimentación | F2 |
| **Multitap** | Taps dibujables sobre la rejilla | F3 |
| **Coro / Flanger / Fase** | La tríada clásica de modulación | F3 |
| **Trémolo / Autopan** | Sincronizados al tempo, formas de onda variadas | F3 |

### Saturación y color

| Nombre | Referencia / carácter | Fase |
|---|---|---|
| **Óxido** | Cinta: compresión suave de transitorios, redondeo de agudos, wow sutil | F2 |
| **Triodo** | Válvula: armónicos pares, de caricia a fuzz | F3 |
| **Sumadora** | Consola: no linealidad leve por canal, crunch al apurar | F3 |
| **Machacadora** | Bitcrusher/downsampler lo-fi | F3 |

### Mastering (el corazón de F2)

| Nombre | Qué es | Fase |
|---|---|---|
| **Techo** | Limitador true-peak con lookahead, canales enlazables, estilos de release, dither integrado a la salida | F1 (base) → F2 (true-peak + estilos) |
| **Anchura** | Imager M/S por bandas: mono abajo, ancho arriba, con correlómetro | F2 |
| **Chispa** | Exciter armónico por bandas | F2 |
| **Multibanda**, **EQ Dinámico**, **Óxido** | (ver arriba) completan la cadena | F2 |
| **Dither** | TPDF con noise shaping a 16/24 bits, en el render y como último eslabón | F2 |
| **Medidor** | LUFS M/S/I, true-peak, rango de sonoridad (LRA), espectro, correlación, vectorscopio, historia | F1 (LUFS+pico) → F2 (completo) |

La cadena de mastering de fábrica —EQ Ocho → Multibanda → Chispa → Óxido →
Anchura → Techo → Dither, con el Medidor mirando— viene montada como preset
del máster: abrir, ajustar, exportar. La exportación con objetivo de
sonoridad usa el mismo Medidor.

### Utilidades

| Nombre | Qué es | Fase |
|---|---|---|
| **Utilidad** | Ganancia, inversión de fase, anchura, mono, balance | F1 |
| **Afinador** | Cromático, para las pistas que entran | F4 |
| **Oscilador** | Generador de señal para calibrar y aprender | F2 |

## Cómo se construyen

Como plugins internos del motor (`tracktion::Plugin`), con DSP en C++
apoyado en `juce::dsp` donde tenga sentido (filtros, convolución, FFT,
sobremuestreo) y a mano donde no. Cada efecto vive en
`motor/src/efectos/<nombre>/` con su DSP, su descriptor de parámetros (que la
interfaz pinta de forma genérica: la tira de dispositivos sabe dibujar
mandos, curvas y medidores a partir del descriptor) y sus tests. Las UIs a
medida (curva del EQ, transferencia del compresor) llegan después del sonido,
nunca antes.
