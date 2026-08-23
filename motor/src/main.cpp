/*  Pletina DAW · motor
    Copyright (C) 2026 Up2you — GPLv3: ver el LICENSE de la raíz.

    Arranque del proceso. El hilo principal es el hilo de mensajes de JUCE y el
    dueño del motor; un hilo aparte lee stdin línea a línea y le pasa cada orden.
    stdout es SOLO del protocolo (NDJSON); todo diagnóstico va por stderr, porque
    una línea suelta en stdout rompería el canal con la UI.

    Uso:
      pletina-motor                       juega con el dispositivo de audio por defecto
      pletina-motor --sin-audio           bomba interna, para CI y contenedores
      pletina-motor --prueba              autoprueba sin audio: carga, reproduce y verifica
      pletina-motor --escanear-vst3 RUTA  proceso hijo del escaneo: examina UN candidato
                                          y escribe sus descripciones XML por stdout
*/

#include "motor.h"
#include "protocolo.h"

#include <iostream>
#include <mutex>

namespace
{
    /** El hijo del escaneo: si el plugin revienta, revienta este proceso y el
        padre lo apunta en la lista negra. Por eso vive aparte del motor. */
    int escanearCandidato (const juce::String& ruta)
    {
        juce::VST3PluginFormat formato;
        juce::OwnedArray<juce::PluginDescription> descripciones;
        formato.findAllTypesForFile (descripciones, ruta);

        for (auto* descripcion : descripciones)
            if (auto xml = descripcion->createXml())
                std::cout << xml->toString (juce::XmlElement::TextFormat().singleLine()) << "\n";

        std::cout << std::flush;
        return descripciones.isEmpty() ? 1 : 0;
    }
}

namespace
{
    // Logger a stderr: lo que JUCE o Tracktion quieran contar no puede pisar el protocolo.
    struct LoggerErr final : juce::Logger
    {
        void logMessage (const juce::String& m) override { std::cerr << m << std::endl; }
    };

    std::mutex candadoSalida;

    void escribirLinea (const juce::String& linea)
    {
        if (linea.isEmpty())
            return;

        std::lock_guard<std::mutex> candado (candadoSalida);
        std::cout << linea << "\n" << std::flush;
    }
}

int main (int argc, char* argv[])
{
    juce::ScopedJuceInitialiser_GUI juceInit;

    LoggerErr logger;
    juce::Logger::setCurrentLogger (&logger);

    Motor::Opciones opciones;
    bool prueba = false;
    bool pruebaEfectos = false;
    bool pruebaCarga = false;
    bool pruebaProtocolo = false;

    for (int i = 1; i < argc; ++i)
    {
        const juce::String arg (argv[i]);

        if (arg == "--sin-audio")       opciones.sinAudio = true;
        else if (arg == "--prueba")     { opciones.sinAudio = true; prueba = true; }
        else if (arg == "--prueba-efectos") { opciones.sinAudio = true; pruebaEfectos = true; }
        else if (arg == "--prueba-carga")   { opciones.sinAudio = true; pruebaCarga = true; }
        else if (arg == "--prueba-protocolo") { opciones.sinAudio = true; pruebaProtocolo = true; }
        else if (arg == "--frecuencia" && i + 1 < argc)  opciones.frecuencia = juce::String (argv[++i]).getDoubleValue();
        else if (arg == "--bloque" && i + 1 < argc)      opciones.bloque = juce::String (argv[++i]).getIntValue();
        else if (arg == "--escanear-vst3" && i + 1 < argc)
        {
            const int codigo = escanearCandidato (juce::String::fromUTF8 (argv[++i]));
            juce::Logger::setCurrentLogger (nullptr);
            return codigo;
        }
    }

    int codigoSalida = 0;

    {
        Motor motor (opciones, escribirLinea);

        if (prueba)
        {
            codigoSalida = motor.autoprueba();
        }
        else if (pruebaEfectos)
        {
            codigoSalida = motor.pruebaEfectos();
        }
        else if (pruebaCarga)
        {
            codigoSalida = motor.pruebaCarga();
        }
        else if (pruebaProtocolo)
        {
            codigoSalida = motor.pruebaProtocolo();
        }
        else
        {
            {
                auto v = juce::var (new juce::DynamicObject());
                v.getDynamicObject()->setProperty ("version", "0.1.0");
                v.getDynamicObject()->setProperty ("audio", opciones.sinAudio ? "sin-audio" : "dispositivo");
                escribirLinea (protocolo::evento ("arrancado", v));
            }

            std::atomic<bool> apagando { false };

            // Lector de stdin: cada línea salta al hilo de mensajes, que es el
            // único que puede tocar el motor. El fin de stdin (la UI se fue)
            // apaga el proceso: un motor huérfano no debe quedarse sonando.
            std::thread lector ([&apagando, &motor]
            {
                std::string linea;

                while (std::getline (std::cin, linea))
                {
                    if (apagando.load())
                        break;

                    const juce::String copia (linea);

                    juce::MessageManager::callAsync ([&motor, copia]
                    {
                        bool salir = false;
                        escribirLinea (protocolo::procesarLinea (motor, copia, salir));

                        if (salir)
                            juce::MessageManager::getInstance()->stopDispatchLoop();
                    });
                }

                if (! apagando.load())
                    juce::MessageManager::callAsync ([] { juce::MessageManager::getInstance()->stopDispatchLoop(); });
            });

            juce::MessageManager::getInstance()->runDispatchLoop();
            apagando = true;

            // El lector suele estar bloqueado en getline sin nada más que hacer:
            // se le suelta, el proceso muere y el sistema lo recoge.
            lector.detach();
        }
    }

    juce::Logger::setCurrentLogger (nullptr);
    return codigoSalida;
}
