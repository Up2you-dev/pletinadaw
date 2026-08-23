/*  Pletina DAW · motor
    Copyright (C) 2026 Up2you — GPLv3: ver el LICENSE de la raíz.

    La suite propia, primera ola (F1): Utilidad, Compresor, Techo, EQ Ocho
    (cuatro bandas de momento) y Medidor. Son plugins internos del motor
    (tracktion::Plugin) con parámetros automatizables; la interfaz los pinta
    a partir del descriptor que viaja en el modelo, sin conocerlos de nada.

    El listón de calidad completo (sobremuestreo, vectores EBU, renders
    dorados) es de F2: aquí está la base sonando bien y medida con cabeza.
*/

#pragma once

#include <tracktion_engine/tracktion_engine.h>
#include <juce_dsp/juce_dsp.h>

namespace te = tracktion;

/** Registra todos los efectos de la suite en el motor. Llamar antes de crear ningún Edit. */
void registrarEfectos (te::Engine&);

/** ¿Es uno de los plugins que el motor pone de serie (mezclador, no cadena)? */
bool esPluginDeSerie (const te::Plugin&);

/** Descriptor de un parámetro para la interfaz. */
juce::var describirParametros (te::Plugin&);

//==============================================================================
class UtilidadPlugin : public te::Plugin
{
public:
    static const char* getPluginName() { return "Utilidad"; }
    static const char* xmlTypeName;

    UtilidadPlugin (te::PluginCreationInfo);
    ~UtilidadPlugin() override;

    juce::String getName() const override { return getPluginName(); }
    juce::String getPluginType() override { return xmlTypeName; }
    juce::String getSelectableDescription() override { return getName(); }

    void initialise (const te::PluginInitialisationInfo&) override;
    void deinitialise() override {}
    void applyToBuffer (const te::PluginRenderContext&) override;
    void restorePluginStateFromValueTree (const juce::ValueTree&) override;

    juce::CachedValue<float> ganancia, anchura, fase, mono;
    te::AutomatableParameter::Ptr pGanancia, pAnchura, pFase, pMono;

private:
    float gananciaSuavizada = 1.0f;
};

//==============================================================================
class CompresorPlugin : public te::Plugin
{
public:
    static const char* getPluginName() { return "Compresor"; }
    static const char* xmlTypeName;

    CompresorPlugin (te::PluginCreationInfo);
    ~CompresorPlugin() override;

    juce::String getName() const override { return getPluginName(); }
    juce::String getPluginType() override { return xmlTypeName; }
    juce::String getSelectableDescription() override { return getName(); }

    void initialise (const te::PluginInitialisationInfo&) override;
    void deinitialise() override {}
    void applyToBuffer (const te::PluginRenderContext&) override;
    void restorePluginStateFromValueTree (const juce::ValueTree&) override;

    juce::CachedValue<float> umbral, ratio, ataque, relajacion, ganancia;
    te::AutomatableParameter::Ptr pUmbral, pRatio, pAtaque, pRelajacion, pGanancia;

private:
    double frecuencia = 48000.0;
    float envolvente = 0.0f;    // seguidor de nivel (lineal)
    float reduccionDb = 0.0f;   // reducción aplicada, en dB (positivo)
};

//==============================================================================
class TechoPlugin : public te::Plugin
{
public:
    static const char* getPluginName() { return "Techo"; }
    static const char* xmlTypeName;

    TechoPlugin (te::PluginCreationInfo);
    ~TechoPlugin() override;

    juce::String getName() const override { return getPluginName(); }
    juce::String getPluginType() override { return xmlTypeName; }
    juce::String getSelectableDescription() override { return getName(); }

    void initialise (const te::PluginInitialisationInfo&) override;
    void deinitialise() override {}
    void applyToBuffer (const te::PluginRenderContext&) override;
    void restorePluginStateFromValueTree (const juce::ValueTree&) override;

    double getLatencySeconds() override { return MIRADA_MS / 1000.0; }

    juce::CachedValue<float> techo, relajacion;
    te::AutomatableParameter::Ptr pTecho, pRelajacion;

    static constexpr double MIRADA_MS = 2.0; // lookahead

private:
    double frecuencia = 48000.0;
    int mirada = 96;                         // muestras de lookahead
    juce::AudioBuffer<float> retardo;        // línea de retardo circular
    int posRetardo = 0;
    float atenuacion = 1.0f;                 // ganancia aplicada (estado del release)
};

//==============================================================================
class EQOchoPlugin : public te::Plugin
{
public:
    static const char* getPluginName() { return "EQ Ocho"; }
    static const char* xmlTypeName;

    EQOchoPlugin (te::PluginCreationInfo);
    ~EQOchoPlugin() override;

    juce::String getName() const override { return getPluginName(); }
    juce::String getPluginType() override { return xmlTypeName; }
    juce::String getSelectableDescription() override { return getName(); }

    void initialise (const te::PluginInitialisationInfo&) override;
    void deinitialise() override {}
    void applyToBuffer (const te::PluginRenderContext&) override;
    void restorePluginStateFromValueTree (const juce::ValueTree&) override;

    // Cuatro bandas en F1 (grave shelf, dos campanas, agudo shelf); ocho en F2.
    static constexpr int BANDAS = 4;
    juce::CachedValue<float> frecuencias[BANDAS], ganancias[BANDAS], anchos[BANDAS];
    te::AutomatableParameter::Ptr pFrecuencias[BANDAS], pGanancias[BANDAS], pAnchos[BANDAS];

private:
    void refrescarCoeficientes (bool forzar);

    double frecuencia = 48000.0;
    juce::dsp::IIR::Filter<float> filtros[BANDAS][2]; // banda × canal
    float cacheF[BANDAS] = {}, cacheG[BANDAS] = {}, cacheQ[BANDAS] = {};
};

//==============================================================================
/** Reverb de placa: red de retardos (FDN de 8 líneas, matriz de Householder)
    con difusión de entrada y amortiguación de agudos en el lazo. El carácter
    EMT-140 fino llega con F2 completa; esta ya flota como debe. */
class PlacaPlugin : public te::Plugin
{
public:
    static const char* getPluginName() { return "Placa"; }
    static const char* xmlTypeName;

    PlacaPlugin (te::PluginCreationInfo);
    ~PlacaPlugin() override;

    juce::String getName() const override { return getPluginName(); }
    juce::String getPluginType() override { return xmlTypeName; }
    juce::String getSelectableDescription() override { return getName(); }

    void initialise (const te::PluginInitialisationInfo&) override;
    void deinitialise() override {}
    void applyToBuffer (const te::PluginRenderContext&) override;
    void restorePluginStateFromValueTree (const juce::ValueTree&) override;

    juce::CachedValue<float> predelay, decaimiento, amortiguacion, mezcla;
    te::AutomatableParameter::Ptr pPredelay, pDecaimiento, pAmortiguacion, pMezcla;

private:
    static constexpr int LINEAS = 8;

    double frecuencia = 48000.0;
    std::vector<float> lineas[LINEAS];
    int posLinea[LINEAS] = {};
    float pasoBajo[LINEAS] = {};
    std::vector<float> difusores[4];       // 2 allpass por canal de entrada
    int posDifusor[4] = {};
    std::vector<float> preLinea[2];
    int posPre = 0;
};

//==============================================================================
/** Eco digital sincronizado al tempo del proyecto, con realimentación
    filtrada y modo ping-pong. */
class DelayPlugin : public te::Plugin
{
public:
    static const char* getPluginName() { return "Delay"; }
    static const char* xmlTypeName;

    DelayPlugin (te::PluginCreationInfo);
    ~DelayPlugin() override;

    juce::String getName() const override { return getPluginName(); }
    juce::String getPluginType() override { return xmlTypeName; }
    juce::String getSelectableDescription() override { return getName(); }

    void initialise (const te::PluginInitialisationInfo&) override;
    void deinitialise() override {}
    void applyToBuffer (const te::PluginRenderContext&) override;
    void restorePluginStateFromValueTree (const juce::ValueTree&) override;

    juce::CachedValue<float> tiempo, realimentacion, tono, pingpong, mezcla;
    te::AutomatableParameter::Ptr pTiempo, pRealimentacion, pTono, pPingpong, pMezcla;

private:
    double frecuencia = 48000.0;
    std::vector<float> linea[2];
    int posEscritura = 0;
    float retardoSuavizado = 0.0f;         // en muestras; se desliza sin clicks
    float filtro[2] = {};
};

//==============================================================================
/** Puerta de ruido con histéresis y retención: abre rápido, aguanta lo que
    se le pida y cierra con elegancia. */
class PuertaPlugin : public te::Plugin
{
public:
    static const char* getPluginName() { return "Puerta"; }
    static const char* xmlTypeName;

    PuertaPlugin (te::PluginCreationInfo);
    ~PuertaPlugin() override;

    juce::String getName() const override { return getPluginName(); }
    juce::String getPluginType() override { return xmlTypeName; }
    juce::String getSelectableDescription() override { return getName(); }

    void initialise (const te::PluginInitialisationInfo&) override;
    void deinitialise() override {}
    void applyToBuffer (const te::PluginRenderContext&) override;
    void restorePluginStateFromValueTree (const juce::ValueTree&) override;

    juce::CachedValue<float> umbral, ataque, relajacion, retencion, rango;
    te::AutomatableParameter::Ptr pUmbral, pAtaque, pRelajacion, pRetencion, pRango;

private:
    double frecuencia = 48000.0;
    float envolvente = 0.0f;
    float apertura = 0.0f;                 // 0 cerrada · 1 abierta, suavizada
    int retenidas = 0;                     // muestras que quedan de retención
};

//==============================================================================
/** Medición del máster: pico por canal y sonoridad LUFS (momentánea, corta e
    integrada) con el prefiltro K y las puertas de ITU-R BS.1770 / EBU R128.
    La validación formal contra los vectores oficiales de la EBU es de F2. */
class MedidorPlugin : public te::Plugin
{
public:
    static const char* getPluginName() { return "Medidor"; }
    static const char* xmlTypeName;

    MedidorPlugin (te::PluginCreationInfo);
    ~MedidorPlugin() override;

    juce::String getName() const override { return getPluginName(); }
    juce::String getPluginType() override { return xmlTypeName; }
    juce::String getSelectableDescription() override { return getName(); }
    bool canBeDisabled() override { return false; }

    void initialise (const te::PluginInitialisationInfo&) override;
    void deinitialise() override {}
    void applyToBuffer (const te::PluginRenderContext&) override;
    void restorePluginStateFromValueTree (const juce::ValueTree&) override {}

    struct Lectura { float picoDb, lufsM, lufsS, lufsI; };
    Lectura leer();       // hilo de mensajes
    void reiniciar();     // borra la integrada (al arrancar reproducción)

private:
    void volcarBloque100ms (double energia);

    double frecuencia = 48000.0;
    juce::dsp::IIR::Filter<float> preK1[2], preK2[2]; // prefiltro K por canal

    juce::SpinLock candado;
    float picoLineal = 0.0f;
    // Energía media (z_i) por bloques de 100 ms; momentánea = 4, corta = 30.
    std::vector<double> bloques100;      // anillo corto para M y S
    size_t posBloque = 0;
    double energiaAcumulada = 0.0;
    int muestrasAcumuladas = 0;
    // Integrada: bloques de 400 ms (solapados al 75 %) que pasan la puerta absoluta.
    std::vector<double> bloquesIntegrada;
};
