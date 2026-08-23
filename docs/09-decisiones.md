# Decisiones de arquitectura (ADRs)

Registro corto de las decisiones que condicionan todo lo demás: qué se
decidió, por qué, y qué se descartó. Si alguna se revierte, se anota aquí
mismo con fecha y motivo.

---

## ADR-001 · Híbrido: UI web + motor nativo en proceso aparte

**Decisión.** Interfaz en Electron; audio en un binario C++ propio
(`pletina-motor`) hablando por stdio.

**Por qué.** Un DAW serio exige lo que un renderer web no puede dar (ASIO,
hosting VST3, DSP a latencia baja) y una interfaz rica itera mejor en web.
El proceso separado aísla crashes de plugins, permite editores VST3 como
ventanas nativas y deja un motor sin cabeza reutilizable (CLI, CI).

**Descartado.** Electron puro (sin VST3/ASIO: insuficiente para lo pedido);
JUCE también para la UI (velocidad de iteración y ADN de familia perdidos);
módulo nativo N-API dentro de Electron (un plugin colgado tumba la app, y
las ventanas de plugins se complican).

**Coste asumido.** Dos toolchains y un protocolo que mantener; sincronía
UI⇄motor como fuente clásica de bugs — mitigado con el contrato testeado y
la verdad del modelo en un solo lado (el motor).

---

## ADR-002 · Tracktion Engine como núcleo del motor

**Decisión.** El motor se construye sobre Tracktion Engine v3 (con JUCE 8),
versiones fijadas por commit.

**Por qué.** Edit/pistas/clips, grafo con PDC, transporte, grabación, MIDI,
warp, hosting y render ya existen, probados en Waveform. Construirlo a mano
son años que la suite de efectos —el valor diferencial declarado— no tiene.

**Descartado.** Motor desde cero (esfuerzo), otros frameworks sin modelo de
DAW (habría que escribir el 80 % igualmente).

**Coste asumido.** GPLv3 (ver ADR-003) y acoplarse a las decisiones de T.E.;
mitigado fijando versiones y tocando su API solo desde `motor/src`.

---

## ADR-003 · GPLv3 para todo el repositorio

**Decisión.** Pletina DAW se publica bajo GPLv3.

**Por qué.** Es la vía gratuita de JUCE (AGPL), Tracktion Engine (GPL) y
RubberBand (GPL). Código abierto además encaja con un DAW que promete "sin
sorpresas".

**Alternativa conocida (reversible por el propietario).** Licencias
comerciales de JUCE, Tracktion Engine y RubberBand permitirían cerrar el
código; la UI, al hablar con el motor a distancia de proceso, podría
relicenciarse aparte. Detalles y matices en [licencias](11-licencias.md).

---

## ADR-004 · Protocolo propio NDJSON por stdio

**Decisión.** JSON-RPC casero en líneas: peticiones con `id`, eventos sin él.

**Por qué.** Cero dependencias en ambos lados, depurable con un `echo`,
suficiente para órdenes y estado a 15 Hz. El volumen alto (ondas, espectros)
tiene su plan: canal binario aparte cuando llegue, sin tocar el diseño de
mensajes.

**Descartado.** gRPC/protobuf y WebSocket+msgpack (dependencias y ceremonia
antes de tiempo); OSC (pobre para peticiones con respuesta).

---

## ADR-005 · Español en todo

**Decisión.** Código, protocolo, docs, tests e interfaz en español, como el
reproductor.

**Por qué.** Es el idioma de la casa y de la familia Pletina; la coherencia
vale más que la ortodoxia del inglés en un proyecto personal. Las
dependencias quedan en su idioma, claro.

---

## ADR-006 · Oscuro por defecto, diseño de familia

**Decisión.** Tokens, tipografías y componentes heredados del reproductor,
con el tema oscuro como predeterminado del DAW (el reproductor arranca en
claro).

**Por qué.** Horas de ondas y medidores piden fondo oscuro; la familia se
reconoce igual por tipografía, radios, acentos índigo y ámbar, y el logo de
barras.

---

## ADR-007 · La suite propia va antes que los VST3

**Decisión.** F2 construye mastering y mezcla propios; el hosting VST3
espera a F5.

**Por qué.** Decisión explícita del propietario: los plugins propios —y el
mastering en particular— son vitales e identitarios. Si los VST3 llegaran
primero, la suite propia nacería como "lo que usas si no tienes nada mejor";
al revés, nace como el estándar de la casa y los VST3 como invitados.
