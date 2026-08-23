/*  Pletina DAW · motor
    Traducción entre líneas NDJSON y órdenes del Motor. El formato está descrito
    en protocolo.h y en docs/04-protocolo.md.
*/

#include "protocolo.h"
#include "motor.h"

namespace
{
    juce::var objeto()
    {
        return juce::var (new juce::DynamicObject());
    }

    void pon (juce::var& v, const juce::Identifier& clave, const juce::var& valor)
    {
        v.getDynamicObject()->setProperty (clave, valor);
    }

    juce::String aLinea (const juce::var& v)
    {
        return juce::JSON::toString (v, true);
    }
}

namespace protocolo
{
    juce::String evento (const juce::String& nombre, const juce::var& datos)
    {
        auto v = objeto();
        pon (v, "evento", nombre);
        pon (v, "datos", datos);
        return aLinea (v);
    }

    juce::String respuesta (const juce::var& id, const juce::var& resultado)
    {
        auto v = objeto();
        pon (v, "id", id);
        pon (v, "resultado", resultado);
        return aLinea (v);
    }

    juce::String error (const juce::var& id, int codigo, const juce::String& mensaje)
    {
        auto e = objeto();
        pon (e, "codigo", codigo);
        pon (e, "mensaje", mensaje);

        auto v = objeto();
        pon (v, "id", id);
        pon (v, "error", e);
        return aLinea (v);
    }

    juce::String procesarLinea (Motor& motor, const juce::String& linea, bool& pidieronSalir)
    {
        if (linea.trim().isEmpty())
            return {};

        auto mensaje = juce::JSON::parse (linea);

        if (! mensaje.isObject())
            return error (juce::var(), -32700, "no es JSON: " + linea.substring (0, 80));

        const auto id = mensaje["id"];
        const auto metodo = mensaje["metodo"].toString();
        const auto params = mensaje["params"];

        try
        {
            if (metodo == "hola")                 return respuesta (id, motor.hola());
            if (metodo == "dispositivos.listar")  return respuesta (id, motor.listarDispositivos());
            if (metodo == "edit.nuevo")           return respuesta (id, motor.nuevoEdit());
            if (metodo == "edit.cargarAudio")     return respuesta (id, motor.cargarAudio (params["ruta"].toString()));
            if (metodo == "transporte.tocar")     return respuesta (id, motor.tocar());
            if (metodo == "transporte.parar")     return respuesta (id, motor.parar());
            if (metodo == "transporte.irA")       return respuesta (id, motor.irA ((double) params["segundos"]));
            if (metodo == "transporte.estado")    return respuesta (id, motor.estadoTransporte());

            if (metodo == "salir")
            {
                pidieronSalir = true;
                auto r = objeto();
                pon (r, "adios", true);
                return respuesta (id, r);
            }
        }
        catch (const std::exception& e)
        {
            return error (id, -32000, e.what());
        }

        return error (id, -32601, "método desconocido: " + metodo);
    }
}
