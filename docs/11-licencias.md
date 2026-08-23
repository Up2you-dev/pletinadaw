# Licencias

## La decisión

**Pletina DAW es GPLv3** (el `LICENSE` de la raíz), y es una decisión
**firme**: el propietario decidió (agosto de 2026) que el proyecto es
público y sin ánimo de venta. No es solo la consecuencia de construir sobre
la vía gratuita de las piezas elegidas: es lo que el proyecto quiere ser —
software local, inspeccionable, sin sorpresas. De propina, en un repositorio
público GitHub Actions no cuesta minutos: el CI (incluidos los builds de
Windows del motor, los caros) sale gratis.

## Qué obliga cada pieza

| Pieza | Licencia gratuita | Implicación |
|---|---|---|
| JUCE 8 | AGPLv3 | El motor (y cualquier binario que enlace JUCE) debe publicarse bajo (A)GPLv3 con su código |
| Tracktion Engine | GPLv3 | Ídem |
| RubberBand (F3) | GPLv2+ | Compatible con GPLv3; misma obligación |
| SoundTouch | LGPL 2.1 | Sin exigencia de abrir nuestro código |
| VST3 (hosting) | Dual: GPLv3 o acuerdo Steinberg | Por la vía GPLv3, sin papeleo: ya somos GPL |
| Electron, Node | MIT y afines | Sin exigencias |
| Fuentes (Archivo, Karla, IBM Plex Mono) | OFL 1.1 | Se redistribuyen con su licencia al lado |

**ASIO es aparte y no es licencia de código abierto**: el SDK de Steinberg se
descarga tras aceptar su acuerdo y **no se puede redistribuir**. Por eso el
repo compila sin ASIO (WASAPI manda en Windows) y el soporte ASIO se activa
en compilaciones locales que tengan el SDK en disco. La documentación de F1
dirá exactamente dónde ponerlo. Distribuir binarios oficiales con ASIO
exigirá revisar el acuerdo vigente de Steinberg en ese momento.

## Qué significa GPLv3 en la práctica

- Cualquiera puede usar, estudiar, modificar y redistribuir Pletina DAW,
  también comercialmente, siempre bajo la misma licencia y con el código.
- Se puede **cobrar** por binarios, instaladores, soporte o servicios; lo que
  no se puede es cerrar el código de lo distribuido.
- Las obras de los usuarios (su música, sus proyectos) no quedan afectadas en
  absoluto: la GPL cubre el programa, no lo que el programa produce.

## La puerta comercial, cerrada

Existió como alternativa sobre el papel (licencias comerciales de JUCE,
Tracktion Engine y RubberBand para poder cerrar el código) y queda anotada
aquí solo como historia: con el proyecto declarado público y sin ánimo de
venta, no aplica. En consecuencia:

- **VST3** se aloja por la vía GPL del SDK, sin firmar el acuerdo
  propietario de Steinberg.
- **RubberBand** entra en F3 sin plan B: su GPL es la nuestra.
- No hay que vigilar umbrales de facturación ni asientos de JUCE.

## Obligaciones operativas ya asumidas

- `LICENSE` (GPLv3) en la raíz; cabecera breve de licencia en los fuentes
  del motor cuando el proyecto salga de F0.
- Las licencias de terceros viajan con el código (`LICENSE-fonts.txt` con
  las fuentes; los textos de JUCE/T.E. llegan con sus árboles descargados y
  se recogerán en un `licencias-de-terceros.md` cuando se empaquete el
  primer instalador, F6).
- Las IRs de fábrica (F2) y los samples de fábrica (F4) solo entrarán con
  licencia explícita compatible (CC0 o grabación propia), anotada junto al
  contenido.
