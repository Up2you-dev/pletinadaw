/*  Pletina DAW · motor
    Arranque del proceso. El hilo principal es el hilo de mensajes de JUCE y el
    dueño del motor; un hilo aparte lee stdin línea a línea y le pasa cada orden.
    stdout es SOLO del protocolo (NDJSON); todo diagnóstico va por stderr, porque
    una línea suelta en stdout rompería el canal con la UI.

    Uso:
      pletina-motor                juega con el dispositivo de audio por defecto
      pletina-motor --sin-audio    bomba interna, para CI y contenedores
      pletina-motor --prueba       autoprueba sin audio: carga, reproduce y verifica
*/

#include "motor.h"
#include "protocolo.h"

#include <iostream>
#include <mutex>

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

    for (int i = 1; i < argc; ++i)
    {
        const juce::String arg (argv[i]);

        if (arg == "--sin-audio")       opciones.sinAudio = true;
        else if (arg == "--prueba")     { opciones.sinAudio = true; prueba = true; }
        else if (arg == "--frecuencia" && i + 1 < argc)  opciones.frecuencia = juce::String (argv[++i]).getDoubleValue();
        else if (arg == "--bloque" && i + 1 < argc)      opciones.bloque = juce::String (argv[++i]).getIntValue();
    }

    int codigoSalida = 0;

    {
        Motor motor (opciones, escribirLinea);

        if (prueba)
        {
            codigoSalida = motor.autoprueba();
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
