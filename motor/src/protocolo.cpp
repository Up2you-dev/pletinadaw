/*  Pletina DAW · motor
    Copyright (C) 2026 Up2you — GPLv3: ver el LICENSE de la raíz.

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

            if (metodo == "proyecto.nuevo")       return respuesta (id, motor.nuevoProyecto (params["carpeta"].toString()));
            if (metodo == "proyecto.abrir")       return respuesta (id, motor.abrirProyecto (params["ruta"].toString()));
            if (metodo == "proyecto.guardar")     return respuesta (id, motor.guardarProyecto());
            if (metodo == "pistas.listar")        return respuesta (id, motor.listarPistas());

            if (metodo == "pista.crear")          return respuesta (id, motor.crearPista());
            if (metodo == "pista.borrar")         return respuesta (id, motor.borrarPista ((int) params["pista"]));
            if (metodo == "pista.renombrar")      return respuesta (id, motor.renombrarPista ((int) params["pista"], params["nombre"].toString()));
            if (metodo == "pista.mezcla")         return respuesta (id, motor.mezclaPista (params.hasProperty ("pista") ? (int) params["pista"] : -1, params));
            if (metodo == "pista.envio")          return respuesta (id, motor.envioPista ((int) params["pista"], (int) params["bus"], (double) params["nivelDb"]));
            if (metodo == "pista.congelar")       return respuesta (id, motor.congelarPista ((int) params["pista"], (bool) params["activo"]));

            if (metodo == "clip.importar")        return respuesta (id, motor.importarClip ((int) params["pista"], params["ruta"].toString(), (double) params["inicio"]));
            if (metodo == "clip.mover")           return respuesta (id, motor.moverClip (params["id"].toString(), (double) params["inicio"], params.hasProperty ("pista") ? (int) params["pista"] : -1));
            if (metodo == "clip.recortar")        return respuesta (id, motor.recortarClip (params["id"].toString(), (double) params["inicio"], (double) params["fin"]));
            if (metodo == "clip.dividir")         return respuesta (id, motor.dividirClip (params["id"].toString(), (double) params["segundos"]));
            if (metodo == "clip.duplicar")        return respuesta (id, motor.duplicarClip (params["id"].toString()));
            if (metodo == "clip.fundidos")        return respuesta (id, motor.fundidosClip (params["id"].toString(), (double) params["entrada"], (double) params["salida"]));
            if (metodo == "clip.borrar")          return respuesta (id, motor.borrarClip (params["id"].toString()));
            if (metodo == "clip.picos")           return respuesta (id, motor.picosClip (params["id"].toString(), params.hasProperty ("porSegundo") ? (int) params["porSegundo"] : 50));
            if (metodo == "clip.warp")            return respuesta (id, motor.warpClip (params["id"].toString(), params));

            if (metodo == "dispositivos.tono")    return respuesta (id, motor.tonoDePrueba (params));
            if (metodo == "vst.carpetas")         return respuesta (id, motor.carpetasVst (params));
            if (metodo == "vst.escanear")         return respuesta (id, motor.escanearVst());
            if (metodo == "vst.lista")            return respuesta (id, motor.listaVst());
            if (metodo == "previa.tocar")         return respuesta (id, motor.tocarPrevia (params["ruta"].toString()));
            if (metodo == "previa.parar")         return respuesta (id, motor.pararPrevia());
            if (metodo == "pista.armar")          return respuesta (id, motor.armarPista ((int) params["pista"], (bool) params["activo"], params.hasProperty ("entrada") ? (int) params["entrada"] : 0, params.hasProperty ("midi") && (bool) params["midi"]));
            if (metodo == "transporte.grabar")    return respuesta (id, motor.grabar (params));

            if (metodo == "grupo.crear")          return respuesta (id, motor.crearGrupo (params));
            if (metodo == "grupo.meter")          return respuesta (id, motor.meterEnGrupo ((int) params["pista"], (int) params["grupo"]));
            if (metodo == "grupo.sacar")          return respuesta (id, motor.sacarDeGrupo ((int) params["pista"]));
            if (metodo == "grupo.deshacer")       return respuesta (id, motor.deshacerGrupo ((int) params["grupo"]));

            if (metodo == "clip.midi.crear")      return respuesta (id, motor.crearClipMidi ((int) params["pista"], (double) params["inicio"], params.hasProperty ("compases") ? (double) params["compases"] : 1.0));
            if (metodo == "clip.midi.notas")      return respuesta (id, motor.notasClipMidi (params["id"].toString(), params["notas"]));
            if (metodo == "clip.midi.cuantizar")  return respuesta (id, motor.cuantizarClipMidi (params["id"].toString(), params["division"].toString()));

            if (metodo == "sesion.escenas")       return respuesta (id, motor.escenasSesion ((int) params["numero"]));
            if (metodo == "sesion.poner")         return respuesta (id, motor.ponerEnSesion ((int) params["pista"], (int) params["escena"], params["desdeClip"].toString()));
            if (metodo == "sesion.lanzar")        return respuesta (id, motor.lanzarSesion (params.hasProperty ("pista") ? (int) params["pista"] : -1, (int) params["escena"]));
            if (metodo == "sesion.parar")         return respuesta (id, motor.pararSesion (params.hasProperty ("pista") ? (int) params["pista"] : -1));
            if (metodo == "sesion.cuantizacion")  return respuesta (id, motor.cuantizacionSesion (params["nombre"].toString()));

            if (metodo == "plugin.insertar")      return respuesta (id, motor.insertarPlugin (params.hasProperty ("pista") ? (int) params["pista"] : -1, params["tipo"].toString(), params.hasProperty ("indice") ? (int) params["indice"] : -1));
            if (metodo == "plugin.quitar")        return respuesta (id, motor.quitarPlugin (params.hasProperty ("pista") ? (int) params["pista"] : -1, (int) params["indice"]));
            if (metodo == "plugin.parametro")     return respuesta (id, motor.parametroPlugin (params.hasProperty ("pista") ? (int) params["pista"] : -1, (int) params["indice"], params["parametro"].toString(), (double) params["valor"]));
            if (metodo == "plugin.activar")       return respuesta (id, motor.activarPlugin (params.hasProperty ("pista") ? (int) params["pista"] : -1, (int) params["indice"], (bool) params["activo"]));
            if (metodo == "plugin.lateral")       return respuesta (id, motor.lateralPlugin (params.hasProperty ("pista") ? (int) params["pista"] : -1, (int) params["indice"], params.hasProperty ("fuente") ? (int) params["fuente"] : -1));
            if (metodo == "plugin.presets")       return respuesta (id, motor.listarPresets (params["tipo"].toString()));
            if (metodo == "plugin.preset.guardar") return respuesta (id, motor.guardarPreset (params.hasProperty ("pista") ? (int) params["pista"] : -1, (int) params["indice"], params["nombre"].toString()));
            if (metodo == "plugin.preset.cargar")  return respuesta (id, motor.cargarPreset (params.hasProperty ("pista") ? (int) params["pista"] : -1, (int) params["indice"], params["nombre"].toString()));

            if (metodo == "rack.crear")           return respuesta (id, motor.crearRack (params.hasProperty ("pista") ? (int) params["pista"] : -1, params.hasProperty ("desde") ? (int) params["desde"] : -1, params.hasProperty ("hasta") ? (int) params["hasta"] : -1, params["nombre"].toString()));
            if (metodo == "rack.deshacer")        return respuesta (id, motor.deshacerRack (params.hasProperty ("pista") ? (int) params["pista"] : -1, (int) params["indice"]));
            if (metodo == "rack.macro")           return respuesta (id, motor.macroRack (params.hasProperty ("pista") ? (int) params["pista"] : -1, (int) params["indice"], (int) params["macro"], params));
            if (metodo == "rack.asignar")         return respuesta (id, motor.asignarMacroRack (params.hasProperty ("pista") ? (int) params["pista"] : -1, (int) params["indice"], (int) params["macro"], params));

            if (metodo == "automatizacion.puntos") return respuesta (id, motor.puntosAutomatizacion (params));

            if (metodo == "transporte.tocar")     return respuesta (id, motor.tocar());
            if (metodo == "transporte.parar")     return respuesta (id, motor.parar());
            if (metodo == "transporte.irA")       return respuesta (id, motor.irA ((double) params["segundos"]));
            if (metodo == "transporte.estado")    return respuesta (id, motor.estadoTransporte());
            if (metodo == "transporte.tempo")     return respuesta (id, motor.tempo ((double) params["bpm"]));
            if (metodo == "transporte.metronomo") return respuesta (id, motor.metronomo ((bool) params["activo"]));
            if (metodo == "transporte.bucle")     return respuesta (id, motor.bucle ((bool) params["activo"], (double) params["inicio"], (double) params["fin"]));

            if (metodo == "deshacer.deshacer")    return respuesta (id, motor.deshacer());
            if (metodo == "deshacer.rehacer")     return respuesta (id, motor.rehacer());

            if (metodo == "render.exportar")      return respuesta (id, motor.exportar (params["ruta"].toString(),
                                                                     params.hasProperty ("stems") && (bool) params["stems"],
                                                                     params.hasProperty ("lufsObjetivo") ? (double) params["lufsObjetivo"] : -1000.0));

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
