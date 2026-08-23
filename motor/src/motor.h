/*  Pletina DAW · motor
    Copyright (C) 2026 Up2you — GPLv3: ver el LICENSE de la raíz.

    Envoltorio de Tracktion Engine: un Edit, su transporte y los medidores,
    con dos modos de audio. Con dispositivo real, JUCE abre lo que haya
    (WASAPI/ASIO en Windows, ALSA en Linux). Con --sin-audio se usa la interfaz
    hospedada de T.E. y una bomba propia que empuja bloques a ritmo de reloj:
    así el transporte avanza y los medidores miden aunque no exista tarjeta
    de sonido, que es el caso del CI y de los contenedores.
*/

#pragma once

#include <tracktion_engine/tracktion_engine.h>

#include <atomic>
#include <functional>
#include <thread>

namespace te = tracktion;

class Motor : private juce::Timer
{
public:
    struct Opciones
    {
        bool sinAudio = false;
        double frecuencia = 48000.0;
        int bloque = 512;
    };

    // emitir recibe líneas ya formateadas (eventos NDJSON) listas para stdout.
    Motor (Opciones, std::function<void (const juce::String&)> emitir);
    ~Motor() override;

    // Órdenes del protocolo. Todas se llaman desde el hilo de mensajes.
    juce::var hola() const;
    juce::var listarDispositivos();
    juce::var nuevoEdit();
    juce::var cargarAudio (const juce::String& ruta);
    juce::var tocar();
    juce::var parar();
    juce::var irA (double segundos);
    juce::var estadoTransporte() const;

    // Autoprueba para CI y contenedores: genera un WAV, lo carga, lo reproduce
    // medio segundo con la bomba y comprueba que la posición avanza y suena.
    // Devuelve 0 si todo bien; escribe el resultado como evento "prueba".
    int autoprueba();

private:
    void timerCallback() override;
    void asegurarEdit();
    void arrancarBomba();
    void pararBomba();

    Opciones opciones;
    std::function<void (const juce::String&)> emitir;

    te::Engine engine { "PletinaMotor" };
    std::unique_ptr<te::Edit> edit;

    // Medidor colgado del máster; client es la ventanilla de lectura.
    te::LevelMeterPlugin* medidorMaestro = nullptr;
    te::LevelMeasurer::Client clienteMedidor;

    // Bomba del modo sin audio.
    std::thread bomba;
    std::atomic<bool> bombaViva { false };
    std::atomic<float> picoIzq { 0.0f }, picoDer { 0.0f };

    bool reproduciendoAntes = false;

    JUCE_DECLARE_NON_COPYABLE (Motor)
};
