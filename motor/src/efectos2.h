/*  Pletina DAW · motor
    Copyright (C) 2026 Up2you — GPLv3: ver el LICENSE de la raíz.

    Segunda ola de la suite (F2): la cadena de mastering — Multibanda,
    Anchura, Chispa, Óxido, Dither, Oscilador — y las piezas de mezcla que
    faltaban — Sala, Pegamento, De-eser, EQ Dinámico y Balancín. El registro
    de todas vive en registrarEfectos() (efectos.cpp).
*/

#pragma once

#include "efectos.h"

//==============================================================================
/** Compresión por bandas: cruces Linkwitz-Riley de 4.º orden (suman plano)
    y un compresor enlazado por banda. La herramienta central del mastering. */
class MultibandaPlugin : public te::Plugin
{
public:
    static const char* getPluginName() { return "Multibanda"; }
    static const char* xmlTypeName;

    MultibandaPlugin (te::PluginCreationInfo);
    ~MultibandaPlugin() override;

    juce::String getName() const override { return getPluginName(); }
    juce::String getPluginType() override { return xmlTypeName; }
    juce::String getSelectableDescription() override { return getName(); }

    void initialise (const te::PluginInitialisationInfo&) override;
    void deinitialise() override {}
    void applyToBuffer (const te::PluginRenderContext&) override;
    void restorePluginStateFromValueTree (const juce::ValueTree&) override;

    juce::CachedValue<float> cruceBajo, cruceAlto, ratio, ataque, relajacion,
                             umbrales[3], ganancias[3];
    te::AutomatableParameter::Ptr pCruceBajo, pCruceAlto, pRatio, pAtaque, pRelajacion,
                                  pUmbrales[3], pGanancias[3];

private:
    double frecuencia = 48000.0;
    // Por canal: partidor bajo (LP+HP), partidor alto (LP+HP) y el allpass
    // que mantiene la banda grave en fase con las demás.
    juce::dsp::LinkwitzRileyFilter<float> lpBajo[2], hpBajo[2], lpAlto[2], hpAlto[2], apBajo[2];
    float envolventes[3] = {};
};

//==============================================================================
/** Imager M/S por bandas: mono por debajo del cruce si se pide, anchura
    independiente para graves y agudos. */
class AnchuraPlugin : public te::Plugin
{
public:
    static const char* getPluginName() { return "Anchura"; }
    static const char* xmlTypeName;

    AnchuraPlugin (te::PluginCreationInfo);
    ~AnchuraPlugin() override;

    juce::String getName() const override { return getPluginName(); }
    juce::String getPluginType() override { return xmlTypeName; }
    juce::String getSelectableDescription() override { return getName(); }

    void initialise (const te::PluginInitialisationInfo&) override;
    void deinitialise() override {}
    void applyToBuffer (const te::PluginRenderContext&) override;
    void restorePluginStateFromValueTree (const juce::ValueTree&) override;

    juce::CachedValue<float> cruce, anchoGraves, anchoAgudos;
    te::AutomatableParameter::Ptr pCruce, pAnchoGraves, pAnchoAgudos;

private:
    double frecuencia = 48000.0;
    juce::dsp::LinkwitzRileyFilter<float> lpLado, hpLado; // el canal S, partido
};

//==============================================================================
/** Exciter: armónicos nuevos solo del registro alto, mezclados por debajo. */
class ChispaPlugin : public te::Plugin
{
public:
    static const char* getPluginName() { return "Chispa"; }
    static const char* xmlTypeName;

    ChispaPlugin (te::PluginCreationInfo);
    ~ChispaPlugin() override;

    juce::String getName() const override { return getPluginName(); }
    juce::String getPluginType() override { return xmlTypeName; }
    juce::String getSelectableDescription() override { return getName(); }

    void initialise (const te::PluginInitialisationInfo&) override;
    void deinitialise() override {}
    void applyToBuffer (const te::PluginRenderContext&) override;
    void restorePluginStateFromValueTree (const juce::ValueTree&) override;

    juce::CachedValue<float> frecuencia_, empuje, cantidad;
    te::AutomatableParameter::Ptr pFrecuencia, pEmpuje, pCantidad;

private:
    double frecuencia = 48000.0;
    juce::dsp::IIR::Filter<float> pasoAlto[2];
};

//==============================================================================
/** Saturación de cinta: pre/de-énfasis, curva suave con leve asimetría,
    redondeo de agudos y un wow sutil. El color de "pasar por cinta". */
class OxidoPlugin : public te::Plugin
{
public:
    static const char* getPluginName() { return "Óxido"; }
    static const char* xmlTypeName;

    OxidoPlugin (te::PluginCreationInfo);
    ~OxidoPlugin() override;

    juce::String getName() const override { return juce::String::fromUTF8 ("\xc3\x93xido"); }
    juce::String getPluginType() override { return xmlTypeName; }
    juce::String getSelectableDescription() override { return getName(); }

    void initialise (const te::PluginInitialisationInfo&) override;
    void deinitialise() override {}
    void applyToBuffer (const te::PluginRenderContext&) override;
    void restorePluginStateFromValueTree (const juce::ValueTree&) override;

    juce::CachedValue<float> empuje, tono, wow, mezcla;
    te::AutomatableParameter::Ptr pEmpuje, pTono, pWow, pMezcla;

private:
    double frecuencia = 48000.0;
    juce::dsp::IIR::Filter<float> enfasis[2], desenfasis[2], pasoBajo[2];
    std::vector<float> lineaWow[2];
    int posWow = 0;
    double faseWow = 0.0, faseFlutter = 0.0;
};

//==============================================================================
/** Dither TPDF con noise shaping, para cerrar a 16 o 24 bits. Va el último. */
class DitherPlugin : public te::Plugin
{
public:
    static const char* getPluginName() { return "Dither"; }
    static const char* xmlTypeName;

    DitherPlugin (te::PluginCreationInfo);
    ~DitherPlugin() override;

    juce::String getName() const override { return getPluginName(); }
    juce::String getPluginType() override { return xmlTypeName; }
    juce::String getSelectableDescription() override { return getName(); }

    void initialise (const te::PluginInitialisationInfo&) override;
    void deinitialise() override {}
    void applyToBuffer (const te::PluginRenderContext&) override;
    void restorePluginStateFromValueTree (const juce::ValueTree&) override;

    juce::CachedValue<float> bits, moldeado;
    te::AutomatableParameter::Ptr pBits, pMoldeado;

private:
    juce::Random azar;
    float error[2][2] = {}; // realimentación del error, 2 etapas por canal
};

//==============================================================================
/** Oscilador de prueba: seno, ruido rosa o barrido. Para calibrar y aprender. */
class OsciladorPlugin : public te::Plugin
{
public:
    static const char* getPluginName() { return "Oscilador"; }
    static const char* xmlTypeName;

    OsciladorPlugin (te::PluginCreationInfo);
    ~OsciladorPlugin() override;

    juce::String getName() const override { return getPluginName(); }
    juce::String getPluginType() override { return xmlTypeName; }
    juce::String getSelectableDescription() override { return getName(); }

    void initialise (const te::PluginInitialisationInfo&) override;
    void deinitialise() override {}
    void applyToBuffer (const te::PluginRenderContext&) override;
    void restorePluginStateFromValueTree (const juce::ValueTree&) override;

    juce::CachedValue<float> tipo, frecuencia_, nivel;
    te::AutomatableParameter::Ptr pTipo, pFrecuencia, pNivel;

private:
    double frecuencia = 48000.0;
    double fase = 0.0, faseBarrido = 0.0;
    juce::Random azar;
    float rosa[3] = {};
};

//==============================================================================
/** Reverb de sala: la misma red que la Placa pero con reflexiones tempranas,
    líneas más largas y un carácter de habitación que crece hasta catedral. */
class SalaPlugin : public te::Plugin
{
public:
    static const char* getPluginName() { return "Sala"; }
    static const char* xmlTypeName;

    SalaPlugin (te::PluginCreationInfo);
    ~SalaPlugin() override;

    juce::String getName() const override { return getPluginName(); }
    juce::String getPluginType() override { return xmlTypeName; }
    juce::String getSelectableDescription() override { return getName(); }

    void initialise (const te::PluginInitialisationInfo&) override;
    void deinitialise() override {}
    void applyToBuffer (const te::PluginRenderContext&) override;
    void restorePluginStateFromValueTree (const juce::ValueTree&) override;

    juce::CachedValue<float> tamano, decaimiento, amortiguacion, tempranas, mezcla;
    te::AutomatableParameter::Ptr pTamano, pDecaimiento, pAmortiguacion, pTempranas, pMezcla;

private:
    static constexpr int LINEAS = 8;
    static constexpr int ECOS = 6;

    double frecuencia = 48000.0;
    std::vector<float> lineas[LINEAS];
    int posLinea[LINEAS] = {};
    float pasoBajo[LINEAS] = {};
    std::vector<float> tempranasLinea[2];
    int posTempranas = 0;
};

//==============================================================================
/** Bus VCA estilo SSL: ataque lento que deja pasar el golpe, auto-release y
    mando de mezcla. El pegamento de una mezcla. */
class PegamentoPlugin : public te::Plugin
{
public:
    static const char* getPluginName() { return "Pegamento"; }
    static const char* xmlTypeName;

    PegamentoPlugin (te::PluginCreationInfo);
    ~PegamentoPlugin() override;

    juce::String getName() const override { return getPluginName(); }
    juce::String getPluginType() override { return xmlTypeName; }
    juce::String getSelectableDescription() override { return getName(); }

    void initialise (const te::PluginInitialisationInfo&) override;
    void deinitialise() override {}
    void applyToBuffer (const te::PluginRenderContext&) override;
    void restorePluginStateFromValueTree (const juce::ValueTree&) override;

    juce::CachedValue<float> umbral, ratio, ataque, ganancia, mezcla;
    te::AutomatableParameter::Ptr pUmbral, pRatio, pAtaque, pGanancia, pMezcla;

private:
    double frecuencia = 48000.0;
    float envolvente = 0.0f;
    float relajacionAuto = 0.3f; // el auto-release: se adapta al programa
};

//==============================================================================
/** De-eser: compresor de una banda estrecha de agudos, para las eses. */
class DeeserPlugin : public te::Plugin
{
public:
    static const char* getPluginName() { return "De-eser"; }
    static const char* xmlTypeName;

    DeeserPlugin (te::PluginCreationInfo);
    ~DeeserPlugin() override;

    juce::String getName() const override { return getPluginName(); }
    juce::String getPluginType() override { return xmlTypeName; }
    juce::String getSelectableDescription() override { return getName(); }

    void initialise (const te::PluginInitialisationInfo&) override;
    void deinitialise() override {}
    void applyToBuffer (const te::PluginRenderContext&) override;
    void restorePluginStateFromValueTree (const juce::ValueTree&) override;

    juce::CachedValue<float> frecuencia_, umbral, cantidad;
    te::AutomatableParameter::Ptr pFrecuencia, pUmbral, pCantidad;

private:
    double frecuencia = 48000.0;
    juce::dsp::LinkwitzRileyFilter<float> partidorLp[2], partidorHp[2];
    float envolvente = 0.0f;
};

//==============================================================================
/** EQ dinámico de tres bandas: cada campana solo actúa cuando su banda pasa
    del umbral. La herramienta quirúrgica del mastering moderno. */
class EQDinamicoPlugin : public te::Plugin
{
public:
    static const char* getPluginName() { return "EQ Dinámico"; }
    static const char* xmlTypeName;

    EQDinamicoPlugin (te::PluginCreationInfo);
    ~EQDinamicoPlugin() override;

    juce::String getName() const override { return juce::String::fromUTF8 ("EQ Din\xc3\xa1mico"); }
    juce::String getPluginType() override { return xmlTypeName; }
    juce::String getSelectableDescription() override { return getName(); }

    void initialise (const te::PluginInitialisationInfo&) override;
    void deinitialise() override {}
    void applyToBuffer (const te::PluginRenderContext&) override;
    void restorePluginStateFromValueTree (const juce::ValueTree&) override;

    static constexpr int BANDAS = 3;
    juce::CachedValue<float> frecuencias[BANDAS], umbrales[BANDAS], reducciones[BANDAS];
    te::AutomatableParameter::Ptr pFrecuencias[BANDAS], pUmbrales[BANDAS], pReducciones[BANDAS];

private:
    void refrescar (int banda, float ganancia);

    double frecuencia = 48000.0;
    juce::dsp::IIR::Filter<float> filtros[BANDAS][2];   // la campana que corta
    juce::dsp::IIR::Filter<float> detectores[BANDAS][2]; // la banda que escucha
    float envolventes[BANDAS] = {};
    float aplicadas[BANDAS] = { 1e9f, 1e9f, 1e9f };      // ganancia en uso, para no rehacer coeficientes
};

//==============================================================================
/** Balancín: un solo mando, de más oscuro a más brillante. */
class BalancinPlugin : public te::Plugin
{
public:
    static const char* getPluginName() { return "Balancín"; }
    static const char* xmlTypeName;

    BalancinPlugin (te::PluginCreationInfo);
    ~BalancinPlugin() override;

    juce::String getName() const override { return juce::String::fromUTF8 ("Balanc\xc3\xadn"); }
    juce::String getPluginType() override { return xmlTypeName; }
    juce::String getSelectableDescription() override { return getName(); }

    void initialise (const te::PluginInitialisationInfo&) override;
    void deinitialise() override {}
    void applyToBuffer (const te::PluginRenderContext&) override;
    void restorePluginStateFromValueTree (const juce::ValueTree&) override;

    juce::CachedValue<float> balance;
    te::AutomatableParameter::Ptr pBalance;

private:
    double frecuencia = 48000.0;
    juce::dsp::IIR::Filter<float> graves[2], agudos[2];
    float aplicado = 1e9f;
};

/** Interpolador de 4 fases para pico verdadero (BS.1770): lo comparten el
    Techo y el Medidor. Devuelve el pico sobremuestreado de una muestra nueva. */
class PicoVerdadero
{
public:
    void preparar();
    float medir (int canal, float muestra); // pico de las 4 fases interpoladas

private:
    static constexpr int TAPS = 12;
    float fir[4][TAPS] = {};
    float historia[2][TAPS] = {};
    int pos[2] = {};
    bool listo = false;
};

//==============================================================================
/** Reverb de convolución: una respuesta de impulso de verdad (WAV) por
    juce::dsp::Convolution. Trae IRs sintéticas de fábrica y carga las tuyas. */
class ConvolucionPlugin : public te::Plugin
{
public:
    static const char* getPluginName() { return "Convolución"; }
    static const char* xmlTypeName;

    ConvolucionPlugin (te::PluginCreationInfo);
    ~ConvolucionPlugin() override;

    juce::String getName() const override { return juce::String::fromUTF8 ("Convoluci\xc3\xb3n"); }
    juce::String getPluginType() override { return xmlTypeName; }
    juce::String getSelectableDescription() override { return getName(); }

    void initialise (const te::PluginInitialisationInfo&) override;
    void deinitialise() override {}
    void applyToBuffer (const te::PluginRenderContext&) override;
    void restorePluginStateFromValueTree (const juce::ValueTree&) override;

    double getLatencySeconds() override;

    juce::CachedValue<float> mezcla, ganancia;
    juce::CachedValue<juce::String> rutaIR;
    te::AutomatableParameter::Ptr pMezcla, pGanancia;

private:
    void cargarSiCambio();

    double frecuencia = 48000.0;
    int bloqueMax = 512;
    juce::dsp::Convolution convolucion;
    juce::String cargada;
    juce::AudioBuffer<float> seca;
};

/** Genera (si no están) las IRs sintéticas de fábrica y devuelve la carpeta. */
juce::File carpetaIRsDeFabrica();
