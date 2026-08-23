/*  Pletina DAW · motor
    Copyright (C) 2026 Up2you — GPLv3: ver el LICENSE de la raíz.

    DSP de la primera ola de la suite. Regla de la casa: el hilo de audio no
    reserva memoria ni bloquea; los parámetros se leen por bloque y lo que
    necesita suavizado lo tiene, que un mando no puede hacer clicks.
*/

#include "efectos.h"
#include "efectos2.h"
#include "efectos3.h"
#include "efectos4.h"

namespace
{
    const juce::Identifier ID_GANANCIA ("ganancia"), ID_ANCHURA ("anchura"), ID_FASE ("fase"),
                           ID_MONO ("mono"), ID_UMBRAL ("umbral"), ID_RATIO ("ratio"),
                           ID_ATAQUE ("ataque"), ID_RELAJACION ("relajacion"), ID_TECHO ("techo");

    inline float dbAGanancia (float db) { return juce::Decibels::decibelsToGain (db, -100.0f); }
}

void registrarEfectos (te::Engine& engine)
{
    auto& plugins = engine.getPluginManager();
    plugins.createBuiltInType<UtilidadPlugin>();
    plugins.createBuiltInType<CompresorPlugin>();
    plugins.createBuiltInType<TechoPlugin>();
    plugins.createBuiltInType<EQOchoPlugin>();
    plugins.createBuiltInType<MedidorPlugin>();
    plugins.createBuiltInType<PlacaPlugin>();
    plugins.createBuiltInType<DelayPlugin>();
    plugins.createBuiltInType<PuertaPlugin>();
    plugins.createBuiltInType<MultibandaPlugin>();
    plugins.createBuiltInType<AnchuraPlugin>();
    plugins.createBuiltInType<ChispaPlugin>();
    plugins.createBuiltInType<OxidoPlugin>();
    plugins.createBuiltInType<DitherPlugin>();
    plugins.createBuiltInType<OsciladorPlugin>();
    plugins.createBuiltInType<SalaPlugin>();
    plugins.createBuiltInType<PegamentoPlugin>();
    plugins.createBuiltInType<DeeserPlugin>();
    plugins.createBuiltInType<EQDinamicoPlugin>();
    plugins.createBuiltInType<BalancinPlugin>();
    plugins.createBuiltInType<ConvolucionPlugin>();
    registrarClasicos (engine);
    registrarInstrumentos (engine);
}

bool esPluginDeSerie (const te::Plugin& plugin)
{
    // El volumen/pan y el medidor de VU que el motor cuelga de cada pista son
    // el mezclador, no la cadena del usuario: la interfaz no debe enseñarlos.
    return dynamic_cast<const te::VolumeAndPanPlugin*> (&plugin) != nullptr
        || dynamic_cast<const te::LevelMeterPlugin*> (&plugin) != nullptr;
}

juce::var describirParametros (te::Plugin& plugin)
{
    juce::Array<juce::var> lista;

    for (auto parametro : plugin.getAutomatableParameters())
    {
        auto d = new juce::DynamicObject();
        d->setProperty ("id", parametro->paramID);
        d->setProperty ("nombre", parametro->getParameterName());
        d->setProperty ("valor", parametro->getCurrentValue());
        d->setProperty ("min", parametro->valueRange.start);
        d->setProperty ("max", parametro->valueRange.end);
        lista.add (juce::var (d));
    }

    return lista;
}

/* ============================================================== Utilidad */

const char* UtilidadPlugin::xmlTypeName = "utilidad";

UtilidadPlugin::UtilidadPlugin (te::PluginCreationInfo info) : te::Plugin (info)
{
    auto um = getUndoManager();
    ganancia.referTo (state, ID_GANANCIA, um, 0.0f);
    anchura.referTo (state, ID_ANCHURA, um, 1.0f);
    fase.referTo (state, ID_FASE, um, 0.0f);
    mono.referTo (state, ID_MONO, um, 0.0f);

    pGanancia = addParam ("ganancia", TRANS("Ganancia"), { -60.0f, 12.0f });
    pAnchura = addParam ("anchura", TRANS("Anchura"), { 0.0f, 2.0f });
    pFase = addParam ("fase", TRANS("Fase"), { 0.0f, 1.0f });
    pMono = addParam ("mono", TRANS("Mono"), { 0.0f, 1.0f });

    pGanancia->attachToCurrentValue (ganancia);
    pAnchura->attachToCurrentValue (anchura);
    pFase->attachToCurrentValue (fase);
    pMono->attachToCurrentValue (mono);
}

UtilidadPlugin::~UtilidadPlugin()
{
    notifyListenersOfDeletion();
    for (auto p : { &pGanancia, &pAnchura, &pFase, &pMono }) (*p)->detachFromCurrentValue();
}

void UtilidadPlugin::initialise (const te::PluginInitialisationInfo&)
{
    gananciaSuavizada = dbAGanancia (pGanancia->getCurrentValue());
}

void UtilidadPlugin::applyToBuffer (const te::PluginRenderContext& fc)
{
    if (! isEnabled() || fc.destBuffer == nullptr)
        return;

    auto& buffer = *fc.destBuffer;
    const int inicio = fc.bufferStartSample;
    const int n = fc.bufferNumSamples;
    const int canales = buffer.getNumChannels();

    const float objetivo = dbAGanancia (pGanancia->getCurrentValue()) * (pFase->getCurrentValue() >= 0.5f ? -1.0f : 1.0f);

    if (canales >= 2)
    {
        auto* izq = buffer.getWritePointer (0, inicio);
        auto* der = buffer.getWritePointer (1, inicio);
        const bool aMono = pMono->getCurrentValue() >= 0.5f;
        const float ancho = aMono ? 0.0f : pAnchura->getCurrentValue();

        for (int i = 0; i < n; ++i)
        {
            const float m = 0.5f * (izq[i] + der[i]);
            const float s = 0.5f * (izq[i] - der[i]) * ancho;
            izq[i] = m + s;
            der[i] = m - s;
        }
    }

    // La ganancia entra en rampa por bloque: girar el mando no hace escalones.
    for (int c = 0; c < canales; ++c)
        buffer.applyGainRamp (c, inicio, n, gananciaSuavizada, objetivo);

    gananciaSuavizada = objetivo;
}

void UtilidadPlugin::restorePluginStateFromValueTree (const juce::ValueTree& v)
{
    te::copyPropertiesToCachedValues (v, ganancia, anchura, fase, mono);
    for (auto p : getAutomatableParameters()) p->updateFromAttachedValue();
}

/* ============================================================= Compresor */

const char* CompresorPlugin::xmlTypeName = "compresor";

CompresorPlugin::CompresorPlugin (te::PluginCreationInfo info) : te::Plugin (info)
{
    auto um = getUndoManager();
    umbral.referTo (state, ID_UMBRAL, um, -18.0f);
    ratio.referTo (state, ID_RATIO, um, 3.0f);
    ataque.referTo (state, ID_ATAQUE, um, 10.0f);
    relajacion.referTo (state, ID_RELAJACION, um, 120.0f);
    ganancia.referTo (state, ID_GANANCIA, um, 0.0f);

    pUmbral = addParam ("umbral", TRANS("Umbral"), { -60.0f, 0.0f });
    pRatio = addParam ("ratio", TRANS("Ratio"), { 1.0f, 20.0f });
    pAtaque = addParam ("ataque", TRANS("Ataque"), { 0.1f, 100.0f });
    pRelajacion = addParam ("relajacion", juce::String::fromUTF8 ("Relajaci\xc3\xb3n"), { 10.0f, 1000.0f });
    pGanancia = addParam ("ganancia", TRANS("Ganancia"), { 0.0f, 24.0f });

    pUmbral->attachToCurrentValue (umbral);
    pRatio->attachToCurrentValue (ratio);
    pAtaque->attachToCurrentValue (ataque);
    pRelajacion->attachToCurrentValue (relajacion);
    pGanancia->attachToCurrentValue (ganancia);
}

CompresorPlugin::~CompresorPlugin()
{
    notifyListenersOfDeletion();
    for (auto p : { &pUmbral, &pRatio, &pAtaque, &pRelajacion, &pGanancia }) (*p)->detachFromCurrentValue();
}

void CompresorPlugin::initialise (const te::PluginInitialisationInfo& info)
{
    frecuencia = info.sampleRate;
    envolvente = 0.0f;
    reduccionDb = 0.0f;
}

void CompresorPlugin::applyToBuffer (const te::PluginRenderContext& fc)
{
    if (! isEnabled() || fc.destBuffer == nullptr)
        return;

    auto& buffer = *fc.destBuffer;
    const int inicio = fc.bufferStartSample;
    const int n = fc.bufferNumSamples;
    const int canales = juce::jmin (2, buffer.getNumChannels());

    const float vUmbral = pUmbral->getCurrentValue();
    const float vRatio = juce::jmax (1.0f, pRatio->getCurrentValue());
    const float extra = dbAGanancia (pGanancia->getCurrentValue());

    // Coeficientes de un polo por milisegundos de ataque y relajación.
    const float aAtaque = std::exp (-1.0f / (float) (juce::jmax (0.05f, pAtaque->getCurrentValue()) * 0.001 * frecuencia));
    const float aRelaja = std::exp (-1.0f / (float) (juce::jmax (1.0f, pRelajacion->getCurrentValue()) * 0.001 * frecuencia));

    float* datos[2] = { buffer.getWritePointer (0, inicio),
                        canales > 1 ? buffer.getWritePointer (1, inicio) : nullptr };

    for (int i = 0; i < n; ++i)
    {
        float pico = std::abs (datos[0][i]);
        if (datos[1] != nullptr) pico = juce::jmax (pico, std::abs (datos[1][i]));

        // Seguidor de envolvente con los dos canales enlazados: sube al ritmo
        // del ataque, baja al de la relajación, y no se cuela entre ciclos de
        // la propia onda (el error clásico que deja al compresor sin hacer nada).
        const float a = pico > envolvente ? aAtaque : aRelaja;
        envolvente = a * envolvente + (1.0f - a) * pico;

        const float envolventeDb = juce::Decibels::gainToDecibels (envolvente, -100.0f);
        const float exceso = envolventeDb - vUmbral;
        reduccionDb = exceso > 0.0f ? exceso * (1.0f - 1.0f / vRatio) : 0.0f;

        const float g = dbAGanancia (-reduccionDb) * extra;
        datos[0][i] *= g;
        if (datos[1] != nullptr) datos[1][i] *= g;
    }
}

void CompresorPlugin::restorePluginStateFromValueTree (const juce::ValueTree& v)
{
    te::copyPropertiesToCachedValues (v, umbral, ratio, ataque, relajacion, ganancia);
    for (auto p : getAutomatableParameters()) p->updateFromAttachedValue();
}

/* ================================================================= Techo */

const char* TechoPlugin::xmlTypeName = "techo";

TechoPlugin::TechoPlugin (te::PluginCreationInfo info) : te::Plugin (info)
{
    auto um = getUndoManager();
    techo.referTo (state, ID_TECHO, um, -1.0f);
    relajacion.referTo (state, ID_RELAJACION, um, 80.0f);

    pTecho = addParam ("techo", TRANS("Techo"), { -20.0f, 0.0f });
    pRelajacion = addParam ("relajacion", juce::String::fromUTF8 ("Relajaci\xc3\xb3n"), { 10.0f, 500.0f });

    pTecho->attachToCurrentValue (techo);
    pRelajacion->attachToCurrentValue (relajacion);
}

TechoPlugin::~TechoPlugin()
{
    notifyListenersOfDeletion();
    pTecho->detachFromCurrentValue();
    pRelajacion->detachFromCurrentValue();
    delete tp;
}

void TechoPlugin::initialise (const te::PluginInitialisationInfo& info)
{
    frecuencia = info.sampleRate;
    mirada = juce::jmax (16, (int) std::lround (frecuencia * MIRADA_MS / 1000.0));
    retardo.setSize (2, mirada, false, true, true);
    retardo.clear();
    ventanaTP.assign ((size_t) mirada, 0.0f);
    if (tp == nullptr)
        tp = new PicoVerdadero();
    tp->preparar();
    posRetardo = 0;
    atenuacion = 1.0f;
}

void TechoPlugin::applyToBuffer (const te::PluginRenderContext& fc)
{
    if (! isEnabled() || fc.destBuffer == nullptr)
        return;

    auto& buffer = *fc.destBuffer;
    const int inicio = fc.bufferStartSample;
    const int n = fc.bufferNumSamples;
    const int canales = juce::jmin (2, buffer.getNumChannels());

    const float limite = dbAGanancia (pTecho->getCurrentValue());
    const float aRelaja = std::exp (-1.0f / (float) (juce::jmax (1.0f, pRelajacion->getCurrentValue()) * 0.001 * frecuencia));

    float* datos[2] = { buffer.getWritePointer (0, inicio),
                        canales > 1 ? buffer.getWritePointer (1, inicio) : nullptr };
    float* linea[2] = { retardo.getWritePointer (0), retardo.getWritePointer (1) };

    for (int i = 0; i < n; ++i)
    {
        // Entra la muestra nueva en la línea de retardo y sale la vieja. De la
        // entrante se apunta su pico VERDADERO (sobremuestreado ×4): lo que el
        // techo promete es que ni los picos entre muestras lo superan.
        float picoEntrante = 0.0f;
        for (int c = 0; c < 2; ++c)
        {
            float* canal = datos[c] != nullptr ? datos[c] : datos[0];
            const float entrante = canal[i];
            if (datos[c] != nullptr) datos[c][i] = linea[c][posRetardo];
            linea[c][posRetardo] = entrante;
            picoEntrante = juce::jmax (picoEntrante, tp != nullptr ? tp->medir (c, entrante) : std::abs (entrante));
        }
        ventanaTP[(size_t) posRetardo] = picoEntrante;
        posRetardo = (posRetardo + 1) % mirada;

        // Lo más alto que viene por la ventana de mirada decide la atenuación:
        // el ataque es instantáneo (para eso está el retardo) y la vuelta, suave.
        float pico = 0.0f;
        for (int j = 0; j < mirada; ++j)
            pico = juce::jmax (pico, ventanaTP[(size_t) j]);

        const float necesaria = pico > limite ? limite / pico : 1.0f;
        atenuacion = necesaria < atenuacion ? necesaria
                                            : aRelaja * atenuacion + (1.0f - aRelaja) * necesaria;

        datos[0][i] *= atenuacion;
        if (datos[1] != nullptr) datos[1][i] *= atenuacion;
    }
}

void TechoPlugin::restorePluginStateFromValueTree (const juce::ValueTree& v)
{
    te::copyPropertiesToCachedValues (v, techo, relajacion);
    for (auto p : getAutomatableParameters()) p->updateFromAttachedValue();
}

/* =============================================================== EQ Ocho */

const char* EQOchoPlugin::xmlTypeName = "eqocho";

EQOchoPlugin::EQOchoPlugin (te::PluginCreationInfo info) : te::Plugin (info)
{
    auto um = getUndoManager();
    const float porDefecto[BANDAS] = { 100.0f, 400.0f, 2500.0f, 8000.0f };

    for (int b = 0; b < BANDAS; ++b)
    {
        const auto num = juce::String (b + 1);
        frecuencias[b].referTo (state, "frecuencia" + num, um, porDefecto[b]);
        ganancias[b].referTo (state, "ganancia" + num, um, 0.0f);
        anchos[b].referTo (state, "q" + num, um, 0.71f);

        pFrecuencias[b] = addParam ("frecuencia" + num, TRANS("Frecuencia ") + num,
                                    { 20.0f, 20000.0f, 0.0f, 0.25f });
        pGanancias[b] = addParam ("ganancia" + num, TRANS("Ganancia ") + num, { -18.0f, 18.0f });
        pAnchos[b] = addParam ("q" + num, "Q " + num, { 0.3f, 8.0f, 0.0f, 0.5f });

        pFrecuencias[b]->attachToCurrentValue (frecuencias[b]);
        pGanancias[b]->attachToCurrentValue (ganancias[b]);
        pAnchos[b]->attachToCurrentValue (anchos[b]);
    }
}

EQOchoPlugin::~EQOchoPlugin()
{
    notifyListenersOfDeletion();
    for (int b = 0; b < BANDAS; ++b)
    {
        pFrecuencias[b]->detachFromCurrentValue();
        pGanancias[b]->detachFromCurrentValue();
        pAnchos[b]->detachFromCurrentValue();
    }
}

void EQOchoPlugin::initialise (const te::PluginInitialisationInfo& info)
{
    frecuencia = info.sampleRate;
    for (auto& porBanda : filtros)
        for (auto& filtro : porBanda)
            filtro.reset();
    refrescarCoeficientes (true);
}

void EQOchoPlugin::refrescarCoeficientes (bool forzar)
{
    for (int b = 0; b < BANDAS; ++b)
    {
        const float f = juce::jlimit (20.0f, (float) (frecuencia * 0.45), pFrecuencias[b]->getCurrentValue());
        const float g = pGanancias[b]->getCurrentValue();
        const float q = juce::jmax (0.1f, pAnchos[b]->getCurrentValue());

        if (! forzar && f == cacheF[b] && g == cacheG[b] && q == cacheQ[b])
            continue;

        cacheF[b] = f; cacheG[b] = g; cacheQ[b] = q;
        const float lineal = juce::Decibels::decibelsToGain (g);

        auto coeficientes = b == 0 ? juce::dsp::IIR::Coefficients<float>::makeLowShelf (frecuencia, f, q, lineal)
                          : b == BANDAS - 1 ? juce::dsp::IIR::Coefficients<float>::makeHighShelf (frecuencia, f, q, lineal)
                          : juce::dsp::IIR::Coefficients<float>::makePeakFilter (frecuencia, f, q, lineal);

        for (int c = 0; c < 2; ++c)
            filtros[b][c].coefficients = coeficientes;
    }
}

void EQOchoPlugin::applyToBuffer (const te::PluginRenderContext& fc)
{
    if (! isEnabled() || fc.destBuffer == nullptr)
        return;

    refrescarCoeficientes (false);

    auto& buffer = *fc.destBuffer;
    const int inicio = fc.bufferStartSample;
    const int n = fc.bufferNumSamples;
    const int canales = juce::jmin (2, buffer.getNumChannels());

    for (int c = 0; c < canales; ++c)
    {
        auto* datos = buffer.getWritePointer (c, inicio);

        for (int i = 0; i < n; ++i)
        {
            float v = datos[i];
            for (int b = 0; b < BANDAS; ++b)
                v = filtros[b][c].processSample (v);
            datos[i] = v;
        }
    }
}

void EQOchoPlugin::restorePluginStateFromValueTree (const juce::ValueTree& v)
{
    for (int b = 0; b < BANDAS; ++b)
        te::copyPropertiesToCachedValues (v, frecuencias[b], ganancias[b], anchos[b]);
    for (auto p : getAutomatableParameters()) p->updateFromAttachedValue();
    refrescarCoeficientes (true);
}

/* =============================================================== Medidor */

const char* MedidorPlugin::xmlTypeName = "medidor";

MedidorPlugin::MedidorPlugin (te::PluginCreationInfo info) : te::Plugin (info) {}

MedidorPlugin::~MedidorPlugin()
{
    notifyListenersOfDeletion();
    delete tp;
}

void MedidorPlugin::initialise (const te::PluginInitialisationInfo& info)
{
    frecuencia = info.sampleRate;

    // Prefiltro K de BS.1770: realce shelf en agudos y paso alto grave.
    auto shelf = juce::dsp::IIR::Coefficients<float>::makeHighShelf (frecuencia, 1681.97, 0.7071752f,
                                                                     juce::Decibels::decibelsToGain (3.99966f));
    auto pasoAlto = juce::dsp::IIR::Coefficients<float>::makeHighPass (frecuencia, 38.1354f, 0.5003270f);

    for (int c = 0; c < 2; ++c)
    {
        preK1[c].coefficients = shelf;
        preK2[c].coefficients = pasoAlto;
        preK1[c].reset();
        preK2[c].reset();
    }

    if (tp == nullptr)
        tp = new PicoVerdadero();
    tp->preparar();

    const juce::SpinLock::ScopedLockType bloqueo (candado);
    bloques100.assign (30, 0.0);
    posBloque = 0;
    energiaAcumulada = 0.0;
    muestrasAcumuladas = 0;
    bloquesIntegrada.clear();
    picoLineal = 0.0f;
    picoVerdaderoLineal = 0.0f;
    sumaLR = sumaL2 = sumaR2 = 0.0;
    correlacionActual = 1.0f;
    historiaCorta.clear();
    anilloFFT.assign (1024, 0.0f);
    posFFT = 0;
}

void MedidorPlugin::volcarBloque100ms (double energia)
{
    // Llega ya con el candado echado, desde el hilo de audio.
    bloques100[posBloque % bloques100.size()] = energia;
    posBloque += 1;

    // Bloque de 400 ms solapado al 75 %: la unidad de la sonoridad integrada.
    if (posBloque >= 4)
    {
        double suma = 0.0;
        for (size_t j = 0; j < 4; ++j)
            suma += bloques100[(posBloque - 1 - j) % bloques100.size()];
        const double media = suma / 4.0;
        const double lufs = -0.691 + 10.0 * std::log10 (juce::jmax (1e-12, media));

        // Puerta absoluta de −70 LUFS; la relativa se aplica al leer.
        if (lufs > -70.0 && bloquesIntegrada.size() < 500000)
            bloquesIntegrada.push_back (media);
    }
}

void MedidorPlugin::applyToBuffer (const te::PluginRenderContext& fc)
{
    if (fc.destBuffer == nullptr)
        return;

    auto& buffer = *fc.destBuffer;
    const int inicio = fc.bufferStartSample;
    const int n = fc.bufferNumSamples;
    const int canales = juce::jmin (2, buffer.getNumChannels());
    const int porBloque = juce::jmax (1, (int) std::lround (frecuencia / 10.0));

    const juce::SpinLock::ScopedLockType bloqueo (candado);

    for (int i = 0; i < n; ++i)
    {
        double energia = 0.0;
        float muestras[2] = { 0.0f, 0.0f };

        for (int c = 0; c < canales; ++c)
        {
            const float v = buffer.getSample (c, inicio + i);
            muestras[juce::jmin (c, 1)] = v;
            picoLineal = juce::jmax (picoLineal, std::abs (v));
            if (tp != nullptr)
                picoVerdaderoLineal = juce::jmax (picoVerdaderoLineal, tp->medir (c, v));
            const float filtrada = preK2[c].processSample (preK1[c].processSample (v));
            energia += (double) filtrada * filtrada;
        }
        if (canales < 2) muestras[1] = muestras[0];

        sumaLR += (double) muestras[0] * muestras[1];
        sumaL2 += (double) muestras[0] * muestras[0];
        sumaR2 += (double) muestras[1] * muestras[1];

        anilloFFT[posFFT] = 0.5f * (muestras[0] + muestras[1]);
        posFFT = (posFFT + 1) % anilloFFT.size();

        energiaAcumulada += energia;
        muestrasAcumuladas += 1;

        if (muestrasAcumuladas >= porBloque)
        {
            volcarBloque100ms (energiaAcumulada / muestrasAcumuladas);
            energiaAcumulada = 0.0;
            muestrasAcumuladas = 0;

            // Correlación del bloque; con silencio se queda en la última.
            if (sumaL2 + sumaR2 > 1e-9)
                correlacionActual = (float) (sumaLR / std::sqrt (juce::jmax (1e-12, sumaL2 * sumaR2)));
            sumaLR = sumaL2 = sumaR2 = 0.0;

            // Cada segundo, la sonoridad corta entra en la serie del LRA.
            if (posBloque % 10 == 0 && posBloque >= 30 && historiaCorta.size() < 200000)
            {
                double suma = 0.0;
                for (size_t j = 0; j < 30; ++j)
                    suma += bloques100[(posBloque - 1 - j) % bloques100.size()];
                historiaCorta.push_back (suma / 30.0);
            }
        }
    }
}

MedidorPlugin::Lectura MedidorPlugin::leer()
{
    const juce::SpinLock::ScopedLockType bloqueo (candado);

    auto mediaVentana = [this] (size_t cuantos)
    {
        if (posBloque == 0) return 0.0;
        const size_t disponibles = juce::jmin (posBloque, cuantos);
        double suma = 0.0;
        for (size_t j = 0; j < disponibles; ++j)
            suma += bloques100[(posBloque - 1 - j) % bloques100.size()];
        return suma / (double) disponibles;
    };

    auto aLufs = [] (double energia)
    {
        return (float) (-0.691 + 10.0 * std::log10 (juce::jmax (1e-12, energia)));
    };

    Lectura r;
    r.picoDb = juce::Decibels::gainToDecibels (picoLineal, -100.0f);
    r.picoVerdaderoDb = juce::Decibels::gainToDecibels (picoVerdaderoLineal, -100.0f);
    picoLineal = 0.0f;
    picoVerdaderoLineal = 0.0f;
    r.correlacion = juce::jlimit (-1.0f, 1.0f, correlacionActual);
    r.lufsM = aLufs (mediaVentana (4));
    r.lufsS = aLufs (mediaVentana (30));

    // Rango de sonoridad (EBU Tech 3342): serie corta con puerta absoluta de
    // −70 y relativa de −20, y la distancia entre los percentiles 10 y 95.
    r.lra = 0.0f;
    {
        std::vector<double> vivas;
        vivas.reserve (historiaCorta.size());
        for (auto e : historiaCorta)
            if (aLufs (e) > -70.0f) vivas.push_back (e);

        if (vivas.size() >= 4)
        {
            double media = 0.0;
            for (auto e : vivas) media += e;
            media /= (double) vivas.size();
            const float puerta = aLufs (media) - 20.0f;

            std::vector<float> sonoridades;
            sonoridades.reserve (vivas.size());
            for (auto e : vivas)
                if (aLufs (e) > puerta) sonoridades.push_back (aLufs (e));

            if (sonoridades.size() >= 4)
            {
                std::sort (sonoridades.begin(), sonoridades.end());
                const auto p = [&] (double q) { return sonoridades[(size_t) std::lround (q * (sonoridades.size() - 1))]; };
                r.lra = juce::jmax (0.0f, p (0.95) - p (0.10));
            }
        }
    }

    // Espectro: FFT de 1024 con ventana de Hann, agrupada en bandas logarítmicas.
    {
        static juce::dsp::FFT fft (10);
        float bloque[2048] = {};
        for (size_t j = 0; j < anilloFFT.size(); ++j)
        {
            const float ventana = 0.5f * (1.0f - std::cos (2.0f * juce::MathConstants<float>::pi * (float) j / (float) anilloFFT.size()));
            bloque[j] = anilloFFT[(posFFT + j) % anilloFFT.size()] * ventana;
        }
        fft.performFrequencyOnlyForwardTransform (bloque);

        const double fMin = 40.0, fMax = juce::jmin (20000.0, frecuencia * 0.5);
        for (int b = 0; b < BANDAS_ESPECTRO; ++b)
        {
            const double fa = fMin * std::pow (fMax / fMin, (double) b / BANDAS_ESPECTRO);
            const double fb = fMin * std::pow (fMax / fMin, (double) (b + 1) / BANDAS_ESPECTRO);
            const int binA = juce::jmax (1, (int) std::floor (fa * 1024 / frecuencia));
            const int binB = juce::jmax (binA + 1, (int) std::ceil (fb * 1024 / frecuencia));
            float pico = 0.0f;
            for (int k = binA; k < binB && k < 512; ++k)
                pico = juce::jmax (pico, bloque[k]);
            r.espectro[b] = juce::Decibels::gainToDecibels (pico / 256.0f, -100.0f);
        }
    }

    // Integrada con la puerta relativa: media de los bloques que superan la
    // media sin puerta menos 10 LU. Con silencio, −inf honesto (−100).
    r.lufsI = -100.0f;
    if (! bloquesIntegrada.empty())
    {
        double media = 0.0;
        for (auto e : bloquesIntegrada) media += e;
        media /= (double) bloquesIntegrada.size();

        const double umbralRelativo = media * std::pow (10.0, -1.0); // −10 LU en energía
        double suma = 0.0; size_t cuenta = 0;
        for (auto e : bloquesIntegrada)
            if (e > umbralRelativo) { suma += e; cuenta += 1; }

        if (cuenta > 0)
            r.lufsI = aLufs (suma / (double) cuenta);
    }

    return r;
}

void MedidorPlugin::reiniciar()
{
    const juce::SpinLock::ScopedLockType bloqueo (candado);
    bloquesIntegrada.clear();
    historiaCorta.clear();
    energiaAcumulada = 0.0;
    muestrasAcumuladas = 0;
    posBloque = 0;
    std::fill (bloques100.begin(), bloques100.end(), 0.0);
}

/* ================================================================= Placa */

const char* PlacaPlugin::xmlTypeName = "placa";

PlacaPlugin::PlacaPlugin (te::PluginCreationInfo info) : te::Plugin (info)
{
    auto um = getUndoManager();
    predelay.referTo (state, "predelay", um, 12.0f);
    decaimiento.referTo (state, "decaimiento", um, 2.2f);
    amortiguacion.referTo (state, "amortiguacion", um, 6500.0f);
    mezcla.referTo (state, "mezcla", um, 0.35f);

    pPredelay = addParam ("predelay", TRANS("Predelay"), { 0.0f, 120.0f });
    pDecaimiento = addParam ("decaimiento", TRANS("Decaimiento"), { 0.2f, 12.0f, 0.0f, 0.5f });
    pAmortiguacion = addParam ("amortiguacion", juce::String::fromUTF8 ("Amortiguaci\xc3\xb3n"), { 1000.0f, 16000.0f, 0.0f, 0.5f });
    pMezcla = addParam ("mezcla", TRANS("Mezcla"), { 0.0f, 1.0f });

    pPredelay->attachToCurrentValue (predelay);
    pDecaimiento->attachToCurrentValue (decaimiento);
    pAmortiguacion->attachToCurrentValue (amortiguacion);
    pMezcla->attachToCurrentValue (mezcla);
}

PlacaPlugin::~PlacaPlugin()
{
    notifyListenersOfDeletion();
    for (auto p : { &pPredelay, &pDecaimiento, &pAmortiguacion, &pMezcla }) (*p)->detachFromCurrentValue();
}

namespace
{
    // Longitudes primas entre sí, en milisegundos: la receta clásica de FDN.
    constexpr double MS_LINEAS[8] = { 29.7, 37.1, 41.1, 43.7, 53.3, 59.0, 61.3, 68.9 };
    constexpr double MS_DIFUSORES[4] = { 4.7, 3.6, 12.7, 9.3 };
}

void PlacaPlugin::initialise (const te::PluginInitialisationInfo& info)
{
    frecuencia = info.sampleRate;

    for (int i = 0; i < LINEAS; ++i)
    {
        lineas[i].assign ((size_t) std::lround (MS_LINEAS[i] / 1000.0 * frecuencia) + 1, 0.0f);
        posLinea[i] = 0;
        pasoBajo[i] = 0.0f;
    }
    for (int i = 0; i < 4; ++i)
    {
        difusores[i].assign ((size_t) std::lround (MS_DIFUSORES[i] / 1000.0 * frecuencia) + 1, 0.0f);
        posDifusor[i] = 0;
    }
    for (int c = 0; c < 2; ++c)
        preLinea[c].assign ((size_t) std::lround (0.15 * frecuencia) + 1, 0.0f);
    posPre = 0;
}

void PlacaPlugin::applyToBuffer (const te::PluginRenderContext& fc)
{
    if (! isEnabled() || fc.destBuffer == nullptr)
        return;

    auto& buffer = *fc.destBuffer;
    const int inicio = fc.bufferStartSample;
    const int n = fc.bufferNumSamples;
    const int canales = juce::jmin (2, buffer.getNumChannels());

    const float humedo = pMezcla->getCurrentValue();
    const float seco = 1.0f - humedo;
    const int muestrasPre = juce::jlimit (1, (int) preLinea[0].size() - 1,
                                          (int) std::lround (pPredelay->getCurrentValue() / 1000.0 * frecuencia));
    const float rt60 = juce::jmax (0.2f, pDecaimiento->getCurrentValue());
    const float corte = pAmortiguacion->getCurrentValue();
    const float cAmortiguacion = 1.0f - std::exp (-2.0f * juce::MathConstants<float>::pi * corte / (float) frecuencia);

    // Ganancia por línea para el RT60 pedido: g = 10^(-3·t_linea / rt60).
    float ganancias[LINEAS];
    for (int i = 0; i < LINEAS; ++i)
        ganancias[i] = std::pow (10.0f, -3.0f * (float) (MS_LINEAS[i] / 1000.0) / rt60);

    float* datos[2] = { buffer.getWritePointer (0, inicio),
                        canales > 1 ? buffer.getWritePointer (1, inicio) : nullptr };

    for (int i = 0; i < n; ++i)
    {
        const float entradaIzq = datos[0][i];
        const float entradaDer = datos[1] != nullptr ? datos[1][i] : entradaIzq;

        // Predelay por canal.
        preLinea[0][(size_t) posPre] = entradaIzq;
        preLinea[1][(size_t) posPre] = entradaDer;
        const int lecturaPre = (int) ((posPre - muestrasPre + (int) preLinea[0].size()) % (int) preLinea[0].size());
        float izq = preLinea[0][(size_t) lecturaPre];
        float der = preLinea[1][(size_t) lecturaPre];
        posPre = (posPre + 1) % (int) preLinea[0].size();

        // Difusión de entrada: dos allpass en serie por canal (g = 0.68).
        auto allpass = [this] (int cual, float x)
        {
            auto& linea = difusores[cual];
            const float leida = linea[(size_t) posDifusor[cual]];
            const float g = 0.68f;
            const float v = x - g * leida;
            linea[(size_t) posDifusor[cual]] = v;
            posDifusor[cual] = (posDifusor[cual] + 1) % (int) linea.size();
            return leida + g * v;
        };
        izq = allpass (1, allpass (0, izq));
        der = allpass (3, allpass (2, der));

        // Salidas de las 8 líneas, con su amortiguación de agudos.
        float salidas[LINEAS];
        float suma = 0.0f;
        for (int l = 0; l < LINEAS; ++l)
        {
            const float leida = lineas[l][(size_t) posLinea[l]];
            pasoBajo[l] += cAmortiguacion * (leida - pasoBajo[l]);
            salidas[l] = pasoBajo[l] * ganancias[l];
            suma += salidas[l];
        }

        // Matriz de Householder: y_i = x_i − (2/N)·Σx. Energía conservada.
        const float media2 = suma * (2.0f / LINEAS);
        const float alimentacion[2] = { izq, der };
        for (int l = 0; l < LINEAS; ++l)
        {
            const float realimentada = salidas[l] - media2 + alimentacion[l & 1] * 0.35f;
            lineas[l][(size_t) posLinea[l]] = realimentada;
            posLinea[l] = (posLinea[l] + 1) % (int) lineas[l].size();
        }

        // Toma estéreo: mitades cruzadas con signos alternos para decorrelar.
        const float placaIzq = salidas[0] - salidas[2] + salidas[4] - salidas[6];
        const float placaDer = salidas[1] - salidas[3] + salidas[5] - salidas[7];

        datos[0][i] = seco * entradaIzq + humedo * placaIzq;
        if (datos[1] != nullptr) datos[1][i] = seco * entradaDer + humedo * placaDer;
    }
}

void PlacaPlugin::restorePluginStateFromValueTree (const juce::ValueTree& v)
{
    te::copyPropertiesToCachedValues (v, predelay, decaimiento, amortiguacion, mezcla);
    for (auto p : getAutomatableParameters()) p->updateFromAttachedValue();
}

/* ================================================================= Delay */

const char* DelayPlugin::xmlTypeName = "delay";

namespace
{
    // Fracciones de redonda: 1/16, 1/8T, 1/16., 1/8, 1/4T, 1/8., 1/4, 1/2, 1/1.
    constexpr double FRACCIONES[9] = { 0.0625, 0.0833333, 0.09375, 0.125, 0.1666667, 0.1875, 0.25, 0.5, 1.0 };
}

DelayPlugin::DelayPlugin (te::PluginCreationInfo info) : te::Plugin (info)
{
    auto um = getUndoManager();
    tiempo.referTo (state, "tiempo", um, 6.0f);
    realimentacion.referTo (state, "realimentacion", um, 0.35f);
    tono.referTo (state, "tono", um, 6000.0f);
    pingpong.referTo (state, "pingpong", um, 0.0f);
    mezcla.referTo (state, "mezcla", um, 0.3f);

    pTiempo = addParam ("tiempo", TRANS("Tiempo"), { 0.0f, 8.0f, 1.0f });
    pRealimentacion = addParam ("realimentacion", juce::String::fromUTF8 ("Realimentaci\xc3\xb3n"), { 0.0f, 0.95f });
    pTono = addParam ("tono", TRANS("Tono"), { 500.0f, 16000.0f, 0.0f, 0.5f });
    pPingpong = addParam ("pingpong", TRANS("Ping-pong"), { 0.0f, 1.0f });
    pMezcla = addParam ("mezcla", TRANS("Mezcla"), { 0.0f, 1.0f });

    pTiempo->attachToCurrentValue (tiempo);
    pRealimentacion->attachToCurrentValue (realimentacion);
    pTono->attachToCurrentValue (tono);
    pPingpong->attachToCurrentValue (pingpong);
    pMezcla->attachToCurrentValue (mezcla);
}

DelayPlugin::~DelayPlugin()
{
    notifyListenersOfDeletion();
    for (auto p : { &pTiempo, &pRealimentacion, &pTono, &pPingpong, &pMezcla }) (*p)->detachFromCurrentValue();
}

void DelayPlugin::initialise (const te::PluginInitialisationInfo& info)
{
    frecuencia = info.sampleRate;
    for (auto& l : linea)
        l.assign ((size_t) std::lround (4.0 * frecuencia), 0.0f);
    posEscritura = 0;
    retardoSuavizado = 0.0f;
    filtro[0] = filtro[1] = 0.0f;
}

void DelayPlugin::applyToBuffer (const te::PluginRenderContext& fc)
{
    if (! isEnabled() || fc.destBuffer == nullptr)
        return;

    auto& buffer = *fc.destBuffer;
    const int inicio = fc.bufferStartSample;
    const int n = fc.bufferNumSamples;
    const int canales = juce::jmin (2, buffer.getNumChannels());

    // El tiempo sale del tempo del proyecto: una redonda son 4 pulsos.
    const double bpm = edit.tempoSequence.getBpmAt (te::TimePosition());
    const int indiceFraccion = juce::jlimit (0, 8, (int) std::lround (pTiempo->getCurrentValue()));
    const double segundos = FRACCIONES[indiceFraccion] * 4.0 * 60.0 / juce::jmax (20.0, bpm);
    const float objetivo = juce::jlimit (1.0f, (float) linea[0].size() - 4.0f, (float) (segundos * frecuencia));
    if (retardoSuavizado <= 0.0f) retardoSuavizado = objetivo;

    const float humedo = pMezcla->getCurrentValue();
    const float seco = 1.0f - humedo;
    const float vuelta = pRealimentacion->getCurrentValue();
    const bool cruzado = pPingpong->getCurrentValue() >= 0.5f;
    const float cTono = 1.0f - std::exp (-2.0f * juce::MathConstants<float>::pi * pTono->getCurrentValue() / (float) frecuencia);

    float* datos[2] = { buffer.getWritePointer (0, inicio),
                        canales > 1 ? buffer.getWritePointer (1, inicio) : nullptr };
    const int largo = (int) linea[0].size();

    for (int i = 0; i < n; ++i)
    {
        // El retardo se desliza despacio hacia su objetivo: cambiar el tempo
        // o la fracción no chasquea, hace un pequeño barrido de cinta.
        retardoSuavizado += juce::jlimit (-0.5f, 0.5f, objetivo - retardoSuavizado) * 0.002f
                          + juce::jlimit (-0.002f, 0.002f, objetivo - retardoSuavizado);

        auto leer = [&] (int canal)
        {
            const float posicion = (float) posEscritura - retardoSuavizado;
            int a = (int) std::floor (posicion);
            const float fraccion = posicion - (float) a;
            a = ((a % largo) + largo) % largo;
            const int b = (a + 1) % largo;
            return linea[canal][(size_t) a] * (1.0f - fraccion) + linea[canal][(size_t) b] * fraccion;
        };

        const float ecoIzq = leer (0);
        const float ecoDer = canales > 1 ? leer (1) : ecoIzq;

        filtro[0] += cTono * (ecoIzq - filtro[0]);
        filtro[1] += cTono * (ecoDer - filtro[1]);

        const float entradaIzq = datos[0][i];
        const float entradaDer = datos[1] != nullptr ? datos[1][i] : entradaIzq;

        // Ping-pong: la vuelta de cada canal alimenta al contrario.
        linea[0][(size_t) posEscritura] = entradaIzq + (cruzado ? filtro[1] : filtro[0]) * vuelta;
        linea[1][(size_t) posEscritura] = entradaDer + (cruzado ? filtro[0] : filtro[1]) * vuelta;
        posEscritura = (posEscritura + 1) % largo;

        datos[0][i] = seco * entradaIzq + humedo * filtro[0];
        if (datos[1] != nullptr) datos[1][i] = seco * entradaDer + humedo * filtro[1];
    }
}

void DelayPlugin::restorePluginStateFromValueTree (const juce::ValueTree& v)
{
    te::copyPropertiesToCachedValues (v, tiempo, realimentacion, tono, pingpong, mezcla);
    for (auto p : getAutomatableParameters()) p->updateFromAttachedValue();
}

/* ================================================================ Puerta */

const char* PuertaPlugin::xmlTypeName = "puerta";

PuertaPlugin::PuertaPlugin (te::PluginCreationInfo info) : te::Plugin (info)
{
    auto um = getUndoManager();
    umbral.referTo (state, "umbral", um, -45.0f);
    ataque.referTo (state, "ataque", um, 1.5f);
    relajacion.referTo (state, "relajacion", um, 120.0f);
    retencion.referTo (state, "retencion", um, 60.0f);
    rango.referTo (state, "rango", um, -70.0f);

    pUmbral = addParam ("umbral", TRANS("Umbral"), { -80.0f, 0.0f });
    pAtaque = addParam ("ataque", TRANS("Ataque"), { 0.1f, 50.0f });
    pRelajacion = addParam ("relajacion", juce::String::fromUTF8 ("Relajaci\xc3\xb3n"), { 5.0f, 1000.0f });
    pRetencion = addParam ("retencion", juce::String::fromUTF8 ("Retenci\xc3\xb3n"), { 0.0f, 500.0f });
    pRango = addParam ("rango", TRANS("Rango"), { -80.0f, 0.0f });

    pUmbral->attachToCurrentValue (umbral);
    pAtaque->attachToCurrentValue (ataque);
    pRelajacion->attachToCurrentValue (relajacion);
    pRetencion->attachToCurrentValue (retencion);
    pRango->attachToCurrentValue (rango);
}

PuertaPlugin::~PuertaPlugin()
{
    notifyListenersOfDeletion();
    for (auto p : { &pUmbral, &pAtaque, &pRelajacion, &pRetencion, &pRango }) (*p)->detachFromCurrentValue();
}

void PuertaPlugin::initialise (const te::PluginInitialisationInfo& info)
{
    frecuencia = info.sampleRate;
    envolvente = 0.0f;
    apertura = 0.0f;
    retenidas = 0;
}

void PuertaPlugin::applyToBuffer (const te::PluginRenderContext& fc)
{
    if (! isEnabled() || fc.destBuffer == nullptr)
        return;

    auto& buffer = *fc.destBuffer;
    const int inicio = fc.bufferStartSample;
    const int n = fc.bufferNumSamples;
    const int canales = juce::jmin (2, buffer.getNumChannels());

    const float abrir = dbAGanancia (pUmbral->getCurrentValue());
    const float cerrar = abrir * 0.708f; // histéresis de 3 dB: sin tartamudeo
    const float suelo = dbAGanancia (pRango->getCurrentValue());
    const int retener = (int) std::lround (pRetencion->getCurrentValue() / 1000.0 * frecuencia);

    const float cSeguidor = 1.0f - std::exp (-1.0f / (0.0005f * (float) frecuencia));
    const float cAtaque = 1.0f - std::exp (-1.0f / (juce::jmax (0.05f, pAtaque->getCurrentValue()) * 0.001f * (float) frecuencia));
    const float cRelaja = 1.0f - std::exp (-1.0f / (juce::jmax (1.0f, pRelajacion->getCurrentValue()) * 0.001f * (float) frecuencia));

    float* datos[2] = { buffer.getWritePointer (0, inicio),
                        canales > 1 ? buffer.getWritePointer (1, inicio) : nullptr };

    for (int i = 0; i < n; ++i)
    {
        float pico = std::abs (datos[0][i]);
        if (datos[1] != nullptr) pico = juce::jmax (pico, std::abs (datos[1][i]));

        envolvente += (pico > envolvente ? cSeguidor : cRelaja * 0.5f) * (pico - envolvente);

        bool abierta;
        if (envolvente > abrir) { abierta = true; retenidas = retener; }
        else if (envolvente > cerrar) { abierta = apertura > 0.5f; }
        else if (retenidas > 0) { abierta = true; retenidas -= 1; }
        else { abierta = false; }

        apertura += (abierta ? cAtaque : cRelaja) * ((abierta ? 1.0f : 0.0f) - apertura);

        const float g = suelo + (1.0f - suelo) * apertura;
        datos[0][i] *= g;
        if (datos[1] != nullptr) datos[1][i] *= g;
    }
}

void PuertaPlugin::restorePluginStateFromValueTree (const juce::ValueTree& v)
{
    te::copyPropertiesToCachedValues (v, umbral, ataque, relajacion, retencion, rango);
    for (auto p : getAutomatableParameters()) p->updateFromAttachedValue();
}
