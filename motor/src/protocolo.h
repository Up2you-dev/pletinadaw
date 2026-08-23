/*  Pletina DAW · motor
    Copyright (C) 2026 Up2you — GPLv3: ver el LICENSE de la raíz.

    Protocolo UI⇄motor: JSON-RPC propio en NDJSON por stdio.
    Una línea = un mensaje. Peticiones {id, metodo, params}, respuestas {id, resultado}
    o {id, error:{codigo, mensaje}}, y eventos sin id {evento, datos}.
*/

#pragma once

#include <juce_core/juce_core.h>

class Motor;

namespace protocolo
{
    // Tabla única de métodos que el motor atiende. El test de contrato de la UI
    // (ui/test/contrato.test.js) lee este archivo y compara: un método por línea,
    // entre comillas, sin nada más en la línea. No cambiar el formato a la ligera.
    inline constexpr const char* METODOS[] = {
        "hola",
        "dispositivos.listar",
        "proyecto.nuevo",
        "proyecto.abrir",
        "proyecto.guardar",
        "pistas.listar",
        "pista.crear",
        "pista.borrar",
        "pista.renombrar",
        "pista.mezcla",
        "clip.importar",
        "clip.mover",
        "clip.recortar",
        "clip.dividir",
        "clip.duplicar",
        "clip.fundidos",
        "clip.borrar",
        "clip.picos",
        "plugin.insertar",
        "plugin.quitar",
        "plugin.parametro",
        "plugin.activar",
        "transporte.tocar",
        "transporte.parar",
        "transporte.irA",
        "transporte.estado",
        "transporte.tempo",
        "transporte.metronomo",
        "transporte.bucle",
        "deshacer.deshacer",
        "deshacer.rehacer",
        "render.exportar",
        "salir",
    };

    // Procesa una línea recibida por stdin y devuelve la línea de respuesta
    // (vacía si la entrada no era JSON válido y no traía id que contestar).
    // Debe llamarse desde el hilo de mensajes: el motor lo exige.
    juce::String procesarLinea (Motor&, const juce::String& linea, bool& pidieronSalir);

    // Construcción de mensajes sueltos.
    juce::String evento (const juce::String& nombre, const juce::var& datos);
    juce::String respuesta (const juce::var& id, const juce::var& resultado);
    juce::String error (const juce::var& id, int codigo, const juce::String& mensaje);
}
