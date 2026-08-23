/*  Pletina DAW · motor
    Copyright (C) 2026 Up2you — GPLv3: ver el LICENSE de la raíz.

    Cuarta ola (F4): los instrumentos. Tres generadores propios que reciben
    MIDI del clip o de la entrada: Bruma (sinte sustractivo), Cinta (sampler
    de una muestra) y Pads (caja de ritmos de 16 pads sintetizados).
    Comparten la base PluginSuite de la tercera ola: tabla de parámetros,
    persistencia y automatización vienen de serie.
*/

#pragma once

#include "efectos3.h"

/** Base de los instrumentos: como un clásico, pero anunciándose como sinte
    para que el grafo le dé MIDI y lo procese aunque no lleguen clips de audio. */
template <typename Derivado>
class InstrumentoSuite : public PluginSuite<Derivado>
{
public:
    using PluginSuite<Derivado>::PluginSuite;

    bool isSynth() override                        { return true; }
    bool takesMidiInput() override                 { return true; }
    bool producesAudioWhenNoAudioInput() override  { return true; }
};

/* ================================================================= Bruma */

class BrumaPlugin : public InstrumentoSuite<BrumaPlugin>
{
public:
    static const char* xmlTypeName;
    static const char* NOMBRE;
    static const std::vector<EspecParametro> PARAMETROS;
    static const char* getPluginName() { return NOMBRE; }
    using InstrumentoSuite::InstrumentoSuite;

    void initialise (const te::PluginInitialisationInfo&) override;
    void applyToBuffer (const te::PluginRenderContext&) override;

private:
    static constexpr int VOCES = 8;
    struct Voz
    {
        bool viva = false, soltada = false;
        int nota = -1;
        float velocidad = 1.0f;
        double fase1 = 0.0, fase2 = 0.0;
        float envolvente = 0.0f;
        float pasoBajo = 0.0f, pasoBanda = 0.0f;   // filtro SVF
        int edad = 0;
    };
    Voz voces[VOCES];
    double fs = 48000.0;
    int reloj = 0;
};

/* ================================================================= Cinta */

class CintaPlugin : public InstrumentoSuite<CintaPlugin>
{
public:
    static const char* xmlTypeName;
    static const char* NOMBRE;
    static const std::vector<EspecParametro> PARAMETROS;
    static const char* getPluginName() { return NOMBRE; }

    explicit CintaPlugin (te::PluginCreationInfo);
    ~CintaPlugin() override;

    void initialise (const te::PluginInitialisationInfo&) override;
    void applyToBuffer (const te::PluginRenderContext&) override;
    void restorePluginStateFromValueTree (const juce::ValueTree&) override;

    /** Carga un WAV/MP3/FLAC como muestra (ruta vacía = la de fábrica). */
    void cargarMuestra (const juce::String& ruta);

    juce::CachedValue<juce::String> rutaMuestra;

private:
    void asegurarMuestra();

    static constexpr int VOCES = 12;
    struct Voz
    {
        bool viva = false, soltada = false;
        int nota = -1;
        float velocidad = 1.0f;
        double posicion = 0.0;
        float envolvente = 0.0f;
        int edad = 0;
    };
    Voz voces[VOCES];
    juce::AudioBuffer<float> muestra;   // se sustituye solo desde el hilo de mensajes con el lock
    double fsMuestra = 48000.0;
    juce::String rutaCargada { "\x01sin-cargar" };
    juce::CriticalSection candado;
    double fs = 48000.0;
};

/* ================================================================== Pads */

class PadsPlugin : public InstrumentoSuite<PadsPlugin>
{
public:
    static const char* xmlTypeName;
    static const char* NOMBRE;
    static const std::vector<EspecParametro> PARAMETROS;
    static const char* getPluginName() { return NOMBRE; }
    using InstrumentoSuite::InstrumentoSuite;

    void initialise (const te::PluginInitialisationInfo&) override;
    void applyToBuffer (const te::PluginRenderContext&) override;

    static constexpr int PRIMER_PAD = 36;   // C1: la fila clásica de las cajas de ritmos
    static constexpr int PADS = 16;

private:
    struct Golpe
    {
        bool vivo = false;
        int pad = 0;
        float velocidad = 1.0f;
        double fase = 0.0, fase2 = 0.0;
        float envolvente = 1.0f;
        int muestras = 0;
        juce::Random ruido;
    };
    Golpe golpes[PADS];                     // un golpe vivo por pad
    double fs = 48000.0;
};

/** Registra los tres instrumentos. La llama registrarEfectos. */
void registrarInstrumentos (te::Engine& engine);
