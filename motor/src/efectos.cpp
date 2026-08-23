/*  Pletina DAW · motor
    Copyright (C) 2026 Up2you — GPLv3: ver el LICENSE de la raíz.

    DSP de la primera ola de la suite. Regla de la casa: el hilo de audio no
    reserva memoria ni bloquea; los parámetros se leen por bloque y lo que
    necesita suavizado lo tiene, que un mando no puede hacer clicks.
*/

#include "efectos.h"

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
}

void TechoPlugin::initialise (const te::PluginInitialisationInfo& info)
{
    frecuencia = info.sampleRate;
    mirada = juce::jmax (16, (int) std::lround (frecuencia * MIRADA_MS / 1000.0));
    retardo.setSize (2, mirada, false, true, true);
    retardo.clear();
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
        // Entra la muestra nueva en la línea de retardo y sale la vieja.
        for (int c = 0; c < 2; ++c)
        {
            float* canal = datos[c] != nullptr ? datos[c] : datos[0];
            const float entrante = canal[i];
            if (datos[c] != nullptr) datos[c][i] = linea[c][posRetardo];
            linea[c][posRetardo] = entrante;
        }
        posRetardo = (posRetardo + 1) % mirada;

        // Lo más alto que viene por la ventana de mirada decide la atenuación:
        // el ataque es instantáneo (para eso está el retardo) y la vuelta, suave.
        float pico = 0.0f;
        for (int j = 0; j < mirada; ++j)
            pico = juce::jmax (pico, std::abs (linea[0][j]), datos[1] != nullptr ? std::abs (linea[1][j]) : 0.0f);

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

    const juce::SpinLock::ScopedLockType bloqueo (candado);
    bloques100.assign (30, 0.0);
    posBloque = 0;
    energiaAcumulada = 0.0;
    muestrasAcumuladas = 0;
    bloquesIntegrada.clear();
    picoLineal = 0.0f;
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

        for (int c = 0; c < canales; ++c)
        {
            const float v = buffer.getSample (c, inicio + i);
            picoLineal = juce::jmax (picoLineal, std::abs (v));
            const float filtrada = preK2[c].processSample (preK1[c].processSample (v));
            energia += (double) filtrada * filtrada;
        }

        energiaAcumulada += energia;
        muestrasAcumuladas += 1;

        if (muestrasAcumuladas >= porBloque)
        {
            volcarBloque100ms (energiaAcumulada / muestrasAcumuladas);
            energiaAcumulada = 0.0;
            muestrasAcumuladas = 0;
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
    picoLineal = 0.0f;
    r.lufsM = aLufs (mediaVentana (4));
    r.lufsS = aLufs (mediaVentana (30));

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
    energiaAcumulada = 0.0;
    muestrasAcumuladas = 0;
    posBloque = 0;
    std::fill (bloques100.begin(), bloques100.end(), 0.0);
}
