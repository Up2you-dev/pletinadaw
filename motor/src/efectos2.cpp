/*  Pletina DAW · motor
    Copyright (C) 2026 Up2you — GPLv3: ver el LICENSE de la raíz.

    DSP de la segunda ola. Mismas reglas de la casa: nada de memoria ni
    bloqueos en el hilo de audio, parámetros por bloque, suavizado donde
    un mando pueda hacer clicks.
*/

#include "efectos2.h"

namespace
{
    inline float dbAGanancia (float db) { return juce::Decibels::decibelsToGain (db, -100.0f); }

    juce::dsp::ProcessSpec especificacionMono (double frecuencia)
    {
        return { frecuencia, 512, 1 };
    }
}

/* ============================================================ Multibanda */

const char* MultibandaPlugin::xmlTypeName = "multibanda";

MultibandaPlugin::MultibandaPlugin (te::PluginCreationInfo info) : te::Plugin (info)
{
    auto um = getUndoManager();
    cruceBajo.referTo (state, "cruceBajo", um, 250.0f);
    cruceAlto.referTo (state, "cruceAlto", um, 2500.0f);
    ratio.referTo (state, "ratio", um, 3.0f);
    ataque.referTo (state, "ataque", um, 15.0f);
    relajacion.referTo (state, "relajacion", um, 150.0f);

    pCruceBajo = addParam ("cruceBajo", TRANS("Cruce grave"), { 60.0f, 800.0f, 0.0f, 0.5f });
    pCruceAlto = addParam ("cruceAlto", TRANS("Cruce agudo"), { 1000.0f, 8000.0f, 0.0f, 0.5f });
    pRatio = addParam ("ratio", TRANS("Ratio"), { 1.0f, 10.0f });
    pAtaque = addParam ("ataque", TRANS("Ataque"), { 0.5f, 100.0f });
    pRelajacion = addParam ("relajacion", juce::String::fromUTF8 ("Relajaci\xc3\xb3n"), { 20.0f, 1000.0f });

    pCruceBajo->attachToCurrentValue (cruceBajo);
    pCruceAlto->attachToCurrentValue (cruceAlto);
    pRatio->attachToCurrentValue (ratio);
    pAtaque->attachToCurrentValue (ataque);
    pRelajacion->attachToCurrentValue (relajacion);

    const char* zonas[3] = { "Grave", "Medio", "Agudo" };
    for (int b = 0; b < 3; ++b)
    {
        umbrales[b].referTo (state, juce::String ("umbral") + zonas[b], um, -20.0f);
        ganancias[b].referTo (state, juce::String ("ganancia") + zonas[b], um, 0.0f);
        pUmbrales[b] = addParam (juce::String ("umbral") + zonas[b], TRANS("Umbral ") + zonas[b], { -60.0f, 0.0f });
        pGanancias[b] = addParam (juce::String ("ganancia") + zonas[b], TRANS("Ganancia ") + zonas[b], { -12.0f, 12.0f });
        pUmbrales[b]->attachToCurrentValue (umbrales[b]);
        pGanancias[b]->attachToCurrentValue (ganancias[b]);
    }
}

MultibandaPlugin::~MultibandaPlugin()
{
    notifyListenersOfDeletion();
    for (auto p : { &pCruceBajo, &pCruceAlto, &pRatio, &pAtaque, &pRelajacion }) (*p)->detachFromCurrentValue();
    for (int b = 0; b < 3; ++b) { pUmbrales[b]->detachFromCurrentValue(); pGanancias[b]->detachFromCurrentValue(); }
}

void MultibandaPlugin::initialise (const te::PluginInitialisationInfo& info)
{
    frecuencia = info.sampleRate;
    const auto espec = especificacionMono (frecuencia);

    for (int c = 0; c < 2; ++c)
    {
        lpBajo[c].setType (juce::dsp::LinkwitzRileyFilterType::lowpass);
        hpBajo[c].setType (juce::dsp::LinkwitzRileyFilterType::highpass);
        lpAlto[c].setType (juce::dsp::LinkwitzRileyFilterType::lowpass);
        hpAlto[c].setType (juce::dsp::LinkwitzRileyFilterType::highpass);
        apBajo[c].setType (juce::dsp::LinkwitzRileyFilterType::allpass);
        for (auto* f : { &lpBajo[c], &hpBajo[c], &lpAlto[c], &hpAlto[c], &apBajo[c] })
        {
            f->prepare (espec);
            f->reset();
        }
    }
    for (auto& e : envolventes) e = 0.0f;
}

void MultibandaPlugin::applyToBuffer (const te::PluginRenderContext& fc)
{
    if (! isEnabled() || fc.destBuffer == nullptr)
        return;

    auto& buffer = *fc.destBuffer;
    const int inicio = fc.bufferStartSample;
    const int n = fc.bufferNumSamples;
    const int canales = juce::jmin (2, buffer.getNumChannels());

    const float fBajo = pCruceBajo->getCurrentValue();
    const float fAlto = juce::jmax (fBajo * 1.5f, pCruceAlto->getCurrentValue());
    for (int c = 0; c < 2; ++c)
    {
        lpBajo[c].setCutoffFrequency (fBajo);
        hpBajo[c].setCutoffFrequency (fBajo);
        lpAlto[c].setCutoffFrequency (fAlto);
        hpAlto[c].setCutoffFrequency (fAlto);
        apBajo[c].setCutoffFrequency (fAlto);
    }

    const float vRatio = juce::jmax (1.0f, pRatio->getCurrentValue());
    const float aAtaque = std::exp (-1.0f / (float) (juce::jmax (0.1f, pAtaque->getCurrentValue()) * 0.001 * frecuencia));
    const float aRelaja = std::exp (-1.0f / (float) (juce::jmax (5.0f, pRelajacion->getCurrentValue()) * 0.001 * frecuencia));

    float vUmbral[3], extra[3];
    for (int b = 0; b < 3; ++b)
    {
        vUmbral[b] = pUmbrales[b]->getCurrentValue();
        extra[b] = dbAGanancia (pGanancias[b]->getCurrentValue());
    }

    float* datos[2] = { buffer.getWritePointer (0, inicio),
                        canales > 1 ? buffer.getWritePointer (1, inicio) : nullptr };

    for (int i = 0; i < n; ++i)
    {
        float bandas[3][2];

        for (int c = 0; c < canales; ++c)
        {
            const float x = datos[c] != nullptr ? datos[c][i] : datos[0][i];
            // La banda grave pasa además por el allpass del cruce agudo:
            // así las tres suman planas, que es todo el chiste del LR de 4.º orden.
            bandas[0][c] = apBajo[c].processSample (0, lpBajo[c].processSample (0, x));
            const float resto = hpBajo[c].processSample (0, x);
            bandas[1][c] = lpAlto[c].processSample (0, resto);
            bandas[2][c] = hpAlto[c].processSample (0, resto);
        }
        if (canales < 2)
            for (int b = 0; b < 3; ++b) bandas[b][1] = bandas[b][0];

        float salida[2] = { 0.0f, 0.0f };
        for (int b = 0; b < 3; ++b)
        {
            const float pico = juce::jmax (std::abs (bandas[b][0]), std::abs (bandas[b][1]));
            const float a = pico > envolventes[b] ? aAtaque : aRelaja;
            envolventes[b] = a * envolventes[b] + (1.0f - a) * pico;

            const float envDb = juce::Decibels::gainToDecibels (envolventes[b], -100.0f);
            const float exceso = envDb - vUmbral[b];
            const float reduccion = exceso > 0.0f ? exceso * (1.0f - 1.0f / vRatio) : 0.0f;
            const float g = dbAGanancia (-reduccion) * extra[b];

            salida[0] += bandas[b][0] * g;
            salida[1] += bandas[b][1] * g;
        }

        datos[0][i] = salida[0];
        if (datos[1] != nullptr) datos[1][i] = salida[1];
    }
}

void MultibandaPlugin::restorePluginStateFromValueTree (const juce::ValueTree& v)
{
    te::copyPropertiesToCachedValues (v, cruceBajo, cruceAlto, ratio, ataque, relajacion,
                                      umbrales[0], umbrales[1], umbrales[2],
                                      ganancias[0], ganancias[1], ganancias[2]);
    for (auto p : getAutomatableParameters()) p->updateFromAttachedValue();
}

/* =============================================================== Anchura */

const char* AnchuraPlugin::xmlTypeName = "anchura";

AnchuraPlugin::AnchuraPlugin (te::PluginCreationInfo info) : te::Plugin (info)
{
    auto um = getUndoManager();
    cruce.referTo (state, "cruce", um, 220.0f);
    anchoGraves.referTo (state, "anchoGraves", um, 0.6f);
    anchoAgudos.referTo (state, "anchoAgudos", um, 1.1f);

    pCruce = addParam ("cruce", TRANS("Cruce"), { 60.0f, 1000.0f, 0.0f, 0.5f });
    pAnchoGraves = addParam ("anchoGraves", TRANS("Ancho graves"), { 0.0f, 2.0f });
    pAnchoAgudos = addParam ("anchoAgudos", TRANS("Ancho agudos"), { 0.0f, 2.0f });

    pCruce->attachToCurrentValue (cruce);
    pAnchoGraves->attachToCurrentValue (anchoGraves);
    pAnchoAgudos->attachToCurrentValue (anchoAgudos);
}

AnchuraPlugin::~AnchuraPlugin()
{
    notifyListenersOfDeletion();
    for (auto p : { &pCruce, &pAnchoGraves, &pAnchoAgudos }) (*p)->detachFromCurrentValue();
}

void AnchuraPlugin::initialise (const te::PluginInitialisationInfo& info)
{
    frecuencia = info.sampleRate;
    const auto espec = especificacionMono (frecuencia);
    lpLado.setType (juce::dsp::LinkwitzRileyFilterType::lowpass);
    hpLado.setType (juce::dsp::LinkwitzRileyFilterType::highpass);
    lpLado.prepare (espec); lpLado.reset();
    hpLado.prepare (espec); hpLado.reset();
}

void AnchuraPlugin::applyToBuffer (const te::PluginRenderContext& fc)
{
    if (! isEnabled() || fc.destBuffer == nullptr || fc.destBuffer->getNumChannels() < 2)
        return;

    auto& buffer = *fc.destBuffer;
    const int inicio = fc.bufferStartSample;
    const int n = fc.bufferNumSamples;

    lpLado.setCutoffFrequency (pCruce->getCurrentValue());
    hpLado.setCutoffFrequency (pCruce->getCurrentValue());
    const float grave = pAnchoGraves->getCurrentValue();
    const float agudo = pAnchoAgudos->getCurrentValue();

    auto* izq = buffer.getWritePointer (0, inicio);
    auto* der = buffer.getWritePointer (1, inicio);

    for (int i = 0; i < n; ++i)
    {
        const float m = 0.5f * (izq[i] + der[i]);
        const float s = 0.5f * (izq[i] - der[i]);
        const float sGrave = lpLado.processSample (0, s) * grave;
        const float sAgudo = hpLado.processSample (0, s) * agudo;
        const float lado = sGrave + sAgudo;
        izq[i] = m + lado;
        der[i] = m - lado;
    }
}

void AnchuraPlugin::restorePluginStateFromValueTree (const juce::ValueTree& v)
{
    te::copyPropertiesToCachedValues (v, cruce, anchoGraves, anchoAgudos);
    for (auto p : getAutomatableParameters()) p->updateFromAttachedValue();
}

/* ================================================================ Chispa */

const char* ChispaPlugin::xmlTypeName = "chispa";

ChispaPlugin::ChispaPlugin (te::PluginCreationInfo info) : te::Plugin (info)
{
    auto um = getUndoManager();
    frecuencia_.referTo (state, "frecuencia", um, 4500.0f);
    empuje.referTo (state, "empuje", um, 8.0f);
    cantidad.referTo (state, "cantidad", um, 0.25f);

    pFrecuencia = addParam ("frecuencia", TRANS("Frecuencia"), { 1000.0f, 12000.0f, 0.0f, 0.5f });
    pEmpuje = addParam ("empuje", TRANS("Empuje"), { 0.0f, 24.0f });
    pCantidad = addParam ("cantidad", TRANS("Cantidad"), { 0.0f, 1.0f });

    pFrecuencia->attachToCurrentValue (frecuencia_);
    pEmpuje->attachToCurrentValue (empuje);
    pCantidad->attachToCurrentValue (cantidad);
}

ChispaPlugin::~ChispaPlugin()
{
    notifyListenersOfDeletion();
    for (auto p : { &pFrecuencia, &pEmpuje, &pCantidad }) (*p)->detachFromCurrentValue();
}

void ChispaPlugin::initialise (const te::PluginInitialisationInfo& info)
{
    frecuencia = info.sampleRate;
    for (auto& f : pasoAlto)
    {
        f.coefficients = juce::dsp::IIR::Coefficients<float>::makeHighPass (frecuencia, 4500.0f, 0.707f);
        f.reset();
    }
}

void ChispaPlugin::applyToBuffer (const te::PluginRenderContext& fc)
{
    if (! isEnabled() || fc.destBuffer == nullptr)
        return;

    auto& buffer = *fc.destBuffer;
    const int inicio = fc.bufferStartSample;
    const int n = fc.bufferNumSamples;
    const int canales = juce::jmin (2, buffer.getNumChannels());

    static thread_local float ultimaF = 0.0f;
    const float f = pFrecuencia->getCurrentValue();
    if (std::abs (f - ultimaF) > 1.0f)
    {
        auto coeficientes = juce::dsp::IIR::Coefficients<float>::makeHighPass (frecuencia, f, 0.707f);
        for (auto& filtro : pasoAlto) filtro.coefficients = coeficientes;
        ultimaF = f;
    }

    const float g = dbAGanancia (pEmpuje->getCurrentValue());
    const float mezcla = pCantidad->getCurrentValue() * 0.5f;

    for (int c = 0; c < canales; ++c)
    {
        auto* datos = buffer.getWritePointer (c, inicio);
        for (int i = 0; i < n; ++i)
        {
            const float alto = pasoAlto[c].processSample (datos[i]);
            datos[i] += std::tanh (alto * g) * mezcla;
        }
    }
}

void ChispaPlugin::restorePluginStateFromValueTree (const juce::ValueTree& v)
{
    te::copyPropertiesToCachedValues (v, frecuencia_, empuje, cantidad);
    for (auto p : getAutomatableParameters()) p->updateFromAttachedValue();
}

/* ================================================================= Óxido */

const char* OxidoPlugin::xmlTypeName = "oxido";

OxidoPlugin::OxidoPlugin (te::PluginCreationInfo info) : te::Plugin (info)
{
    auto um = getUndoManager();
    empuje.referTo (state, "empuje", um, 6.0f);
    tono.referTo (state, "tono", um, 12000.0f);
    wow.referTo (state, "wow", um, 0.15f);
    mezcla.referTo (state, "mezcla", um, 1.0f);

    pEmpuje = addParam ("empuje", TRANS("Empuje"), { 0.0f, 18.0f });
    pTono = addParam ("tono", TRANS("Tono"), { 4000.0f, 20000.0f, 0.0f, 0.5f });
    pWow = addParam ("wow", TRANS("Wow"), { 0.0f, 1.0f });
    pMezcla = addParam ("mezcla", TRANS("Mezcla"), { 0.0f, 1.0f });

    pEmpuje->attachToCurrentValue (empuje);
    pTono->attachToCurrentValue (tono);
    pWow->attachToCurrentValue (wow);
    pMezcla->attachToCurrentValue (mezcla);
}

OxidoPlugin::~OxidoPlugin()
{
    notifyListenersOfDeletion();
    for (auto p : { &pEmpuje, &pTono, &pWow, &pMezcla }) (*p)->detachFromCurrentValue();
}

void OxidoPlugin::initialise (const te::PluginInitialisationInfo& info)
{
    frecuencia = info.sampleRate;

    for (int c = 0; c < 2; ++c)
    {
        enfasis[c].coefficients = juce::dsp::IIR::Coefficients<float>::makeHighShelf (frecuencia, 3000.0, 0.6f, dbAGanancia (4.0f));
        desenfasis[c].coefficients = juce::dsp::IIR::Coefficients<float>::makeHighShelf (frecuencia, 3000.0, 0.6f, dbAGanancia (-4.0f));
        pasoBajo[c].coefficients = juce::dsp::IIR::Coefficients<float>::makeLowPass (frecuencia, 12000.0, 0.707f);
        enfasis[c].reset(); desenfasis[c].reset(); pasoBajo[c].reset();
        lineaWow[c].assign ((size_t) std::lround (0.008 * frecuencia) + 4, 0.0f);
    }
    posWow = 0;
    faseWow = faseFlutter = 0.0;
}

void OxidoPlugin::applyToBuffer (const te::PluginRenderContext& fc)
{
    if (! isEnabled() || fc.destBuffer == nullptr)
        return;

    auto& buffer = *fc.destBuffer;
    const int inicio = fc.bufferStartSample;
    const int n = fc.bufferNumSamples;
    const int canales = juce::jmin (2, buffer.getNumChannels());

    static thread_local float ultimoTono = 0.0f;
    const float fTono = pTono->getCurrentValue();
    if (std::abs (fTono - ultimoTono) > 10.0f)
    {
        auto coeficientes = juce::dsp::IIR::Coefficients<float>::makeLowPass (frecuencia, fTono, 0.707f);
        for (auto& filtro : pasoBajo) filtro.coefficients = coeficientes;
        ultimoTono = fTono;
    }

    const float g = dbAGanancia (pEmpuje->getCurrentValue());
    const float compensacion = dbAGanancia (-pEmpuje->getCurrentValue() * 0.6f);
    const float vWow = pWow->getCurrentValue();
    const float humedo = pMezcla->getCurrentValue();
    const float base = 0.03f;
    const float sesgoBase = std::tanh (g * base);
    const int largo = (int) lineaWow[0].size();

    float* datos[2] = { buffer.getWritePointer (0, inicio),
                        canales > 1 ? buffer.getWritePointer (1, inicio) : nullptr };

    for (int i = 0; i < n; ++i)
    {
        // El wow y el flutter mueven juntos una línea de retardo cortísima.
        faseWow += 0.7 / frecuencia;
        faseFlutter += 5.5 / frecuencia;
        const float lfo = (float) (0.6 * std::sin (2.0 * juce::MathConstants<double>::pi * faseWow)
                                 + 0.25 * std::sin (2.0 * juce::MathConstants<double>::pi * faseFlutter));
        const float retardo = 2.0f + vWow * lfo * 1.4f; // ms alrededor de 2

        for (int c = 0; c < canales; ++c)
        {
            const float seca = datos[c][i];
            float v = enfasis[c].processSample (seca);
            v = std::tanh (g * (v + base)) - sesgoBase;   // leve asimetría: pares
            v = desenfasis[c].processSample (v);
            v = pasoBajo[c].processSample (v);

            lineaWow[c][(size_t) posWow] = v;
            const float posicion = (float) posWow - (retardo / 1000.0f) * (float) frecuencia;
            int a = (int) std::floor (posicion);
            const float fraccion = posicion - (float) a;
            a = ((a % largo) + largo) % largo;
            const float leida = lineaWow[c][(size_t) a] * (1.0f - fraccion)
                              + lineaWow[c][(size_t) ((a + 1) % largo)] * fraccion;

            datos[c][i] = (1.0f - humedo) * seca + humedo * leida * compensacion;
        }
        posWow = (posWow + 1) % largo;
    }
}

void OxidoPlugin::restorePluginStateFromValueTree (const juce::ValueTree& v)
{
    te::copyPropertiesToCachedValues (v, empuje, tono, wow, mezcla);
    for (auto p : getAutomatableParameters()) p->updateFromAttachedValue();
}

/* ================================================================ Dither */

const char* DitherPlugin::xmlTypeName = "dither";

DitherPlugin::DitherPlugin (te::PluginCreationInfo info) : te::Plugin (info)
{
    auto um = getUndoManager();
    bits.referTo (state, "bits", um, 16.0f);
    moldeado.referTo (state, "moldeado", um, 1.0f);

    pBits = addParam ("bits", TRANS("Bits"), { 16.0f, 24.0f, 8.0f });
    pMoldeado = addParam ("moldeado", TRANS("Moldeado"), { 0.0f, 1.0f });

    pBits->attachToCurrentValue (bits);
    pMoldeado->attachToCurrentValue (moldeado);
}

DitherPlugin::~DitherPlugin()
{
    notifyListenersOfDeletion();
    pBits->detachFromCurrentValue();
    pMoldeado->detachFromCurrentValue();
}

void DitherPlugin::initialise (const te::PluginInitialisationInfo&)
{
    for (auto& canal : error) canal[0] = canal[1] = 0.0f;
}

void DitherPlugin::applyToBuffer (const te::PluginRenderContext& fc)
{
    if (! isEnabled() || fc.destBuffer == nullptr)
        return;

    auto& buffer = *fc.destBuffer;
    const int inicio = fc.bufferStartSample;
    const int n = fc.bufferNumSamples;
    const int canales = juce::jmin (2, buffer.getNumChannels());

    const int nBits = pBits->getCurrentValue() >= 20.0f ? 24 : 16;
    const float paso = std::pow (2.0f, (float) (1 - nBits));
    const bool moldear = pMoldeado->getCurrentValue() >= 0.5f;

    for (int c = 0; c < canales; ++c)
    {
        auto* datos = buffer.getWritePointer (c, inicio);
        for (int i = 0; i < n; ++i)
        {
            // Error moldeado hacia los agudos (2 coeficientes clásicos) y
            // ruido TPDF de un LSB: el silencio deja de ser granulado.
            const float empujado = datos[i] + (moldear ? error[c][0] * 1.53f - error[c][1] * 0.71f : 0.0f);
            const float tpdf = (azar.nextFloat() - azar.nextFloat()) * paso;
            const float cuantizada = std::round ((empujado + tpdf) / paso) * paso;
            error[c][1] = error[c][0];
            error[c][0] = empujado - cuantizada;
            datos[i] = cuantizada;
        }
    }
}

void DitherPlugin::restorePluginStateFromValueTree (const juce::ValueTree& v)
{
    te::copyPropertiesToCachedValues (v, bits, moldeado);
    for (auto p : getAutomatableParameters()) p->updateFromAttachedValue();
}

/* ============================================================= Oscilador */

const char* OsciladorPlugin::xmlTypeName = "oscilador";

OsciladorPlugin::OsciladorPlugin (te::PluginCreationInfo info) : te::Plugin (info)
{
    auto um = getUndoManager();
    tipo.referTo (state, "tipo", um, 0.0f);
    frecuencia_.referTo (state, "frecuencia", um, 440.0f);
    nivel.referTo (state, "nivel", um, -18.0f);

    pTipo = addParam ("tipo", TRANS("Tipo"), { 0.0f, 2.0f, 1.0f });
    pFrecuencia = addParam ("frecuencia", TRANS("Frecuencia"), { 20.0f, 20000.0f, 0.0f, 0.25f });
    pNivel = addParam ("nivel", TRANS("Nivel"), { -60.0f, 0.0f });

    pTipo->attachToCurrentValue (tipo);
    pFrecuencia->attachToCurrentValue (frecuencia_);
    pNivel->attachToCurrentValue (nivel);
}

OsciladorPlugin::~OsciladorPlugin()
{
    notifyListenersOfDeletion();
    for (auto p : { &pTipo, &pFrecuencia, &pNivel }) (*p)->detachFromCurrentValue();
}

void OsciladorPlugin::initialise (const te::PluginInitialisationInfo& info)
{
    frecuencia = info.sampleRate;
    fase = faseBarrido = 0.0;
    rosa[0] = rosa[1] = rosa[2] = 0.0f;
}

void OsciladorPlugin::applyToBuffer (const te::PluginRenderContext& fc)
{
    if (! isEnabled() || fc.destBuffer == nullptr)
        return;

    auto& buffer = *fc.destBuffer;
    const int inicio = fc.bufferStartSample;
    const int n = fc.bufferNumSamples;
    const int canales = juce::jmin (2, buffer.getNumChannels());

    const int modo = juce::jlimit (0, 2, (int) std::lround (pTipo->getCurrentValue()));
    const float g = dbAGanancia (pNivel->getCurrentValue());
    const double f = pFrecuencia->getCurrentValue();

    for (int i = 0; i < n; ++i)
    {
        float v = 0.0f;

        if (modo == 0)
        {
            fase += f / frecuencia;
            v = (float) std::sin (2.0 * juce::MathConstants<double>::pi * fase);
        }
        else if (modo == 1)
        {
            // Rosa por tres polos (Paul Kellet, versión corta): -3 dB/octava.
            const float blanco = azar.nextFloat() * 2.0f - 1.0f;
            rosa[0] = 0.99765f * rosa[0] + blanco * 0.0990460f;
            rosa[1] = 0.96300f * rosa[1] + blanco * 0.2965164f;
            rosa[2] = 0.57000f * rosa[2] + blanco * 1.0526913f;
            v = (rosa[0] + rosa[1] + rosa[2] + blanco * 0.1848f) * 0.2f;
        }
        else
        {
            // Barrido logarítmico de 20 Hz a 20 kHz cada 8 segundos, en bucle.
            faseBarrido += 1.0 / (8.0 * frecuencia);
            if (faseBarrido >= 1.0) faseBarrido -= 1.0;
            const double fBarrido = 20.0 * std::pow (1000.0, faseBarrido);
            fase += fBarrido / frecuencia;
            v = (float) std::sin (2.0 * juce::MathConstants<double>::pi * fase);
        }

        for (int c = 0; c < canales; ++c)
            buffer.setSample (c, inicio + i, v * g);
    }
}

void OsciladorPlugin::restorePluginStateFromValueTree (const juce::ValueTree& v)
{
    te::copyPropertiesToCachedValues (v, tipo, frecuencia_, nivel);
    for (auto p : getAutomatableParameters()) p->updateFromAttachedValue();
}

/* ================================================================== Sala */

const char* SalaPlugin::xmlTypeName = "sala";

namespace
{
    constexpr double MS_SALA[8] = { 41.3, 53.9, 61.7, 73.1, 83.3, 97.7, 109.3, 127.9 };
    constexpr double MS_ECOS[6] = { 11.3, 17.9, 23.3, 31.7, 39.1, 47.3 };
    constexpr float NIVEL_ECOS[6] = { 0.85f, 0.7f, 0.62f, 0.5f, 0.42f, 0.35f };
}

SalaPlugin::SalaPlugin (te::PluginCreationInfo info) : te::Plugin (info)
{
    auto um = getUndoManager();
    tamano.referTo (state, "tamano", um, 1.0f);
    decaimiento.referTo (state, "decaimiento", um, 1.8f);
    amortiguacion.referTo (state, "amortiguacion", um, 5500.0f);
    tempranas.referTo (state, "tempranas", um, 0.5f);
    mezcla.referTo (state, "mezcla", um, 0.3f);

    pTamano = addParam ("tamano", juce::String::fromUTF8 ("Tama\xc3\xb1o"), { 0.6f, 1.6f });
    pDecaimiento = addParam ("decaimiento", TRANS("Decaimiento"), { 0.2f, 20.0f, 0.0f, 0.4f });
    pAmortiguacion = addParam ("amortiguacion", juce::String::fromUTF8 ("Amortiguaci\xc3\xb3n"), { 1000.0f, 16000.0f, 0.0f, 0.5f });
    pTempranas = addParam ("tempranas", TRANS("Tempranas"), { 0.0f, 1.0f });
    pMezcla = addParam ("mezcla", TRANS("Mezcla"), { 0.0f, 1.0f });

    pTamano->attachToCurrentValue (tamano);
    pDecaimiento->attachToCurrentValue (decaimiento);
    pAmortiguacion->attachToCurrentValue (amortiguacion);
    pTempranas->attachToCurrentValue (tempranas);
    pMezcla->attachToCurrentValue (mezcla);
}

SalaPlugin::~SalaPlugin()
{
    notifyListenersOfDeletion();
    for (auto p : { &pTamano, &pDecaimiento, &pAmortiguacion, &pTempranas, &pMezcla }) (*p)->detachFromCurrentValue();
}

void SalaPlugin::initialise (const te::PluginInitialisationInfo& info)
{
    frecuencia = info.sampleRate;

    for (int i = 0; i < LINEAS; ++i)
    {
        lineas[i].assign ((size_t) std::lround (MS_SALA[i] * 1.6 / 1000.0 * frecuencia) + 4, 0.0f);
        posLinea[i] = 0;
        pasoBajo[i] = 0.0f;
    }
    for (int c = 0; c < 2; ++c)
        tempranasLinea[c].assign ((size_t) std::lround (0.13 * frecuencia) + 4, 0.0f);
    posTempranas = 0;
}

void SalaPlugin::applyToBuffer (const te::PluginRenderContext& fc)
{
    if (! isEnabled() || fc.destBuffer == nullptr)
        return;

    auto& buffer = *fc.destBuffer;
    const int inicio = fc.bufferStartSample;
    const int n = fc.bufferNumSamples;
    const int canales = juce::jmin (2, buffer.getNumChannels());

    const float humedo = pMezcla->getCurrentValue();
    const float seco = 1.0f - humedo;
    const float vTamano = pTamano->getCurrentValue();
    const float rt60 = juce::jmax (0.2f, pDecaimiento->getCurrentValue());
    const float vTempranas = pTempranas->getCurrentValue();
    const float cAmortiguacion = 1.0f - std::exp (-2.0f * juce::MathConstants<float>::pi
                                                  * pAmortiguacion->getCurrentValue() / (float) frecuencia);

    float ganancias[LINEAS];
    int retardos[LINEAS];
    for (int i = 0; i < LINEAS; ++i)
    {
        const double t = MS_SALA[i] * vTamano / 1000.0;
        ganancias[i] = std::pow (10.0f, -3.0f * (float) t / rt60);
        retardos[i] = juce::jlimit (4, (int) lineas[i].size() - 2, (int) std::lround (t * frecuencia));
    }

    int ecos[ECOS];
    for (int e = 0; e < ECOS; ++e)
        ecos[e] = juce::jlimit (1, (int) tempranasLinea[0].size() - 2,
                                (int) std::lround (MS_ECOS[e] * vTamano / 1000.0 * frecuencia));

    float* datos[2] = { buffer.getWritePointer (0, inicio),
                        canales > 1 ? buffer.getWritePointer (1, inicio) : nullptr };

    const int largoTempranas = (int) tempranasLinea[0].size();

    for (int i = 0; i < n; ++i)
    {
        const float entradaIzq = datos[0][i];
        const float entradaDer = datos[1] != nullptr ? datos[1][i] : entradaIzq;

        tempranasLinea[0][(size_t) posTempranas] = entradaIzq;
        tempranasLinea[1][(size_t) posTempranas] = entradaDer;

        float ecoIzq = 0.0f, ecoDer = 0.0f;
        for (int e = 0; e < ECOS; ++e)
        {
            const int lecturaIzq = (posTempranas - ecos[e] + largoTempranas) % largoTempranas;
            const int lecturaDer = (posTempranas - ecos[(e + 3) % ECOS] + largoTempranas) % largoTempranas;
            ecoIzq += tempranasLinea[0][(size_t) lecturaIzq] * NIVEL_ECOS[e];
            ecoDer += tempranasLinea[1][(size_t) lecturaDer] * NIVEL_ECOS[e];
        }
        ecoIzq *= vTempranas * 0.5f;
        ecoDer *= vTempranas * 0.5f;
        posTempranas = (posTempranas + 1) % largoTempranas;

        // La cola late-field: FDN alimentada con la señal más sus tempranas.
        float salidas[LINEAS];
        float suma = 0.0f;
        for (int l = 0; l < LINEAS; ++l)
        {
            const int largo = (int) lineas[l].size();
            const int lectura = (posLinea[l] - retardos[l] + largo) % largo;
            const float leida = lineas[l][(size_t) lectura];
            pasoBajo[l] += cAmortiguacion * (leida - pasoBajo[l]);
            salidas[l] = pasoBajo[l] * ganancias[l];
            suma += salidas[l];
        }

        const float media2 = suma * (2.0f / LINEAS);
        const float alimentacion[2] = { entradaIzq + ecoIzq, entradaDer + ecoDer };
        for (int l = 0; l < LINEAS; ++l)
        {
            lineas[l][(size_t) posLinea[l]] = salidas[l] - media2 + alimentacion[l & 1] * 0.3f;
            posLinea[l] = (posLinea[l] + 1) % (int) lineas[l].size();
        }

        const float colaIzq = salidas[0] - salidas[2] + salidas[4] - salidas[6];
        const float colaDer = salidas[1] - salidas[3] + salidas[5] - salidas[7];

        datos[0][i] = seco * entradaIzq + humedo * (colaIzq + ecoIzq);
        if (datos[1] != nullptr) datos[1][i] = seco * entradaDer + humedo * (colaDer + ecoDer);
    }
}

void SalaPlugin::restorePluginStateFromValueTree (const juce::ValueTree& v)
{
    te::copyPropertiesToCachedValues (v, tamano, decaimiento, amortiguacion, tempranas, mezcla);
    for (auto p : getAutomatableParameters()) p->updateFromAttachedValue();
}

/* ============================================================= Pegamento */

const char* PegamentoPlugin::xmlTypeName = "pegamento";

PegamentoPlugin::PegamentoPlugin (te::PluginCreationInfo info) : te::Plugin (info)
{
    auto um = getUndoManager();
    umbral.referTo (state, "umbral", um, -18.0f);
    ratio.referTo (state, "ratio", um, 4.0f);
    ataque.referTo (state, "ataque", um, 10.0f);
    ganancia.referTo (state, "ganancia", um, 0.0f);
    mezcla.referTo (state, "mezcla", um, 1.0f);

    pUmbral = addParam ("umbral", TRANS("Umbral"), { -40.0f, 0.0f });
    pRatio = addParam ("ratio", TRANS("Ratio"), { 1.5f, 10.0f });
    pAtaque = addParam ("ataque", TRANS("Ataque"), { 0.1f, 30.0f, 0.0f, 0.5f });
    pGanancia = addParam ("ganancia", TRANS("Ganancia"), { 0.0f, 12.0f });
    pMezcla = addParam ("mezcla", TRANS("Mezcla"), { 0.0f, 1.0f });

    pUmbral->attachToCurrentValue (umbral);
    pRatio->attachToCurrentValue (ratio);
    pAtaque->attachToCurrentValue (ataque);
    pGanancia->attachToCurrentValue (ganancia);
    pMezcla->attachToCurrentValue (mezcla);
}

PegamentoPlugin::~PegamentoPlugin()
{
    notifyListenersOfDeletion();
    for (auto p : { &pUmbral, &pRatio, &pAtaque, &pGanancia, &pMezcla }) (*p)->detachFromCurrentValue();
}

void PegamentoPlugin::initialise (const te::PluginInitialisationInfo& info)
{
    frecuencia = info.sampleRate;
    envolvente = 0.0f;
    relajacionAuto = 0.3f;
}

void PegamentoPlugin::applyToBuffer (const te::PluginRenderContext& fc)
{
    if (! isEnabled() || fc.destBuffer == nullptr)
        return;

    auto& buffer = *fc.destBuffer;
    const int inicio = fc.bufferStartSample;
    const int n = fc.bufferNumSamples;
    const int canales = juce::jmin (2, buffer.getNumChannels());

    const float vUmbral = pUmbral->getCurrentValue();
    const float vRatio = pRatio->getCurrentValue();
    const float extra = dbAGanancia (pGanancia->getCurrentValue());
    const float humedo = pMezcla->getCurrentValue();
    const float aAtaque = std::exp (-1.0f / (float) (juce::jmax (0.1f, pAtaque->getCurrentValue()) * 0.001 * frecuencia));

    float* datos[2] = { buffer.getWritePointer (0, inicio),
                        canales > 1 ? buffer.getWritePointer (1, inicio) : nullptr };

    for (int i = 0; i < n; ++i)
    {
        float pico = std::abs (datos[0][i]);
        if (datos[1] != nullptr) pico = juce::jmax (pico, std::abs (datos[1][i]));

        // Auto-release: cuanto más aprieta, más despacio suelta. Es lo que
        // hace que el bus respire con el programa en vez de bombear.
        const float envDb = juce::Decibels::gainToDecibels (envolvente, -100.0f);
        const float apretando = juce::jlimit (0.0f, 1.0f, (envDb - vUmbral) / 12.0f);
        relajacionAuto += 0.0005f * ((0.08f + apretando * 0.9f) - relajacionAuto);
        const float aRelaja = std::exp (-1.0f / (relajacionAuto * (float) frecuencia));

        const float a = pico > envolvente ? aAtaque : aRelaja;
        envolvente = a * envolvente + (1.0f - a) * pico;

        const float exceso = juce::Decibels::gainToDecibels (envolvente, -100.0f) - vUmbral;
        const float reduccion = exceso > 0.0f ? exceso * (1.0f - 1.0f / vRatio) : 0.0f;
        const float g = dbAGanancia (-reduccion) * extra;

        for (int c = 0; c < canales; ++c)
        {
            const float seca = datos[c][i];
            datos[c][i] = seca * (1.0f - humedo) + seca * g * humedo;
        }
    }
}

void PegamentoPlugin::restorePluginStateFromValueTree (const juce::ValueTree& v)
{
    te::copyPropertiesToCachedValues (v, umbral, ratio, ataque, ganancia, mezcla);
    for (auto p : getAutomatableParameters()) p->updateFromAttachedValue();
}

/* =============================================================== De-eser */

const char* DeeserPlugin::xmlTypeName = "deeser";

DeeserPlugin::DeeserPlugin (te::PluginCreationInfo info) : te::Plugin (info)
{
    auto um = getUndoManager();
    frecuencia_.referTo (state, "frecuencia", um, 6500.0f);
    umbral.referTo (state, "umbral", um, -28.0f);
    cantidad.referTo (state, "cantidad", um, 8.0f);

    pFrecuencia = addParam ("frecuencia", TRANS("Frecuencia"), { 2000.0f, 12000.0f, 0.0f, 0.5f });
    pUmbral = addParam ("umbral", TRANS("Umbral"), { -60.0f, 0.0f });
    pCantidad = addParam ("cantidad", TRANS("Cantidad"), { 0.0f, 24.0f });

    pFrecuencia->attachToCurrentValue (frecuencia_);
    pUmbral->attachToCurrentValue (umbral);
    pCantidad->attachToCurrentValue (cantidad);
}

DeeserPlugin::~DeeserPlugin()
{
    notifyListenersOfDeletion();
    for (auto p : { &pFrecuencia, &pUmbral, &pCantidad }) (*p)->detachFromCurrentValue();
}

void DeeserPlugin::initialise (const te::PluginInitialisationInfo& info)
{
    frecuencia = info.sampleRate;
    const auto espec = especificacionMono (frecuencia);
    for (int c = 0; c < 2; ++c)
    {
        partidorLp[c].setType (juce::dsp::LinkwitzRileyFilterType::lowpass);
        partidorHp[c].setType (juce::dsp::LinkwitzRileyFilterType::highpass);
        partidorLp[c].prepare (espec); partidorLp[c].reset();
        partidorHp[c].prepare (espec); partidorHp[c].reset();
    }
    envolvente = 0.0f;
}

void DeeserPlugin::applyToBuffer (const te::PluginRenderContext& fc)
{
    if (! isEnabled() || fc.destBuffer == nullptr)
        return;

    auto& buffer = *fc.destBuffer;
    const int inicio = fc.bufferStartSample;
    const int n = fc.bufferNumSamples;
    const int canales = juce::jmin (2, buffer.getNumChannels());

    const float f = pFrecuencia->getCurrentValue();
    for (int c = 0; c < 2; ++c)
    {
        partidorLp[c].setCutoffFrequency (f);
        partidorHp[c].setCutoffFrequency (f);
    }

    const float vUmbral = pUmbral->getCurrentValue();
    const float maxReduccion = pCantidad->getCurrentValue();
    const float aRapida = 1.0f - std::exp (-1.0f / (0.0005f * (float) frecuencia));
    const float aLenta = 1.0f - std::exp (-1.0f / (0.06f * (float) frecuencia));

    float* datos[2] = { buffer.getWritePointer (0, inicio),
                        canales > 1 ? buffer.getWritePointer (1, inicio) : nullptr };

    for (int i = 0; i < n; ++i)
    {
        float altas[2];
        float bajas[2];
        for (int c = 0; c < canales; ++c)
        {
            const float x = datos[c][i];
            bajas[c] = partidorLp[c].processSample (0, x);
            altas[c] = partidorHp[c].processSample (0, x);
        }
        if (canales < 2) { altas[1] = altas[0]; bajas[1] = bajas[0]; }

        const float pico = juce::jmax (std::abs (altas[0]), std::abs (altas[1]));
        envolvente += (pico > envolvente ? aRapida : aLenta) * (pico - envolvente);

        const float envDb = juce::Decibels::gainToDecibels (envolvente, -100.0f);
        const float exceso = envDb - vUmbral;
        const float reduccion = juce::jlimit (0.0f, maxReduccion, exceso > 0.0f ? exceso * 0.75f : 0.0f);
        const float g = dbAGanancia (-reduccion);

        datos[0][i] = bajas[0] + altas[0] * g;
        if (datos[1] != nullptr) datos[1][i] = bajas[1] + altas[1] * g;
    }
}

void DeeserPlugin::restorePluginStateFromValueTree (const juce::ValueTree& v)
{
    te::copyPropertiesToCachedValues (v, frecuencia_, umbral, cantidad);
    for (auto p : getAutomatableParameters()) p->updateFromAttachedValue();
}

/* =========================================================== EQ Dinámico */

const char* EQDinamicoPlugin::xmlTypeName = "eqdinamico";

EQDinamicoPlugin::EQDinamicoPlugin (te::PluginCreationInfo info) : te::Plugin (info)
{
    auto um = getUndoManager();
    const float porDefecto[BANDAS] = { 220.0f, 900.0f, 4200.0f };

    for (int b = 0; b < BANDAS; ++b)
    {
        const auto num = juce::String (b + 1);
        frecuencias[b].referTo (state, "frecuencia" + num, um, porDefecto[b]);
        umbrales[b].referTo (state, "umbral" + num, um, -24.0f);
        reducciones[b].referTo (state, "reduccion" + num, um, 0.0f);

        pFrecuencias[b] = addParam ("frecuencia" + num, TRANS("Frecuencia ") + num, { 40.0f, 16000.0f, 0.0f, 0.3f });
        pUmbrales[b] = addParam ("umbral" + num, TRANS("Umbral ") + num, { -60.0f, 0.0f });
        pReducciones[b] = addParam ("reduccion" + num, juce::String::fromUTF8 ("Reducci\xc3\xb3n ") + num, { 0.0f, 18.0f });

        pFrecuencias[b]->attachToCurrentValue (frecuencias[b]);
        pUmbrales[b]->attachToCurrentValue (umbrales[b]);
        pReducciones[b]->attachToCurrentValue (reducciones[b]);
    }
}

EQDinamicoPlugin::~EQDinamicoPlugin()
{
    notifyListenersOfDeletion();
    for (int b = 0; b < BANDAS; ++b)
    {
        pFrecuencias[b]->detachFromCurrentValue();
        pUmbrales[b]->detachFromCurrentValue();
        pReducciones[b]->detachFromCurrentValue();
    }
}

void EQDinamicoPlugin::initialise (const te::PluginInitialisationInfo& info)
{
    frecuencia = info.sampleRate;

    for (int b = 0; b < BANDAS; ++b)
    {
        const float f = juce::jlimit (40.0f, (float) (frecuencia * 0.45), pFrecuencias[b]->getCurrentValue());
        auto deteccion = juce::dsp::IIR::Coefficients<float>::makeBandPass (frecuencia, f, 1.0f);
        for (int c = 0; c < 2; ++c)
        {
            detectores[b][c].coefficients = deteccion;
            detectores[b][c].reset();
            filtros[b][c].reset();
        }
        envolventes[b] = 0.0f;
        aplicadas[b] = 1e9f;
        refrescar (b, 0.0f);
    }
}

void EQDinamicoPlugin::refrescar (int banda, float gananciaDb)
{
    if (std::abs (gananciaDb - aplicadas[banda]) < 0.2f)
        return;

    aplicadas[banda] = gananciaDb;
    const float f = juce::jlimit (40.0f, (float) (frecuencia * 0.45), pFrecuencias[banda]->getCurrentValue());
    auto coeficientes = juce::dsp::IIR::Coefficients<float>::makePeakFilter (frecuencia, f, 2.0f,
                                                                             dbAGanancia (gananciaDb));
    for (int c = 0; c < 2; ++c)
        filtros[banda][c].coefficients = coeficientes;
}

void EQDinamicoPlugin::applyToBuffer (const te::PluginRenderContext& fc)
{
    if (! isEnabled() || fc.destBuffer == nullptr)
        return;

    auto& buffer = *fc.destBuffer;
    const int inicio = fc.bufferStartSample;
    const int n = fc.bufferNumSamples;
    const int canales = juce::jmin (2, buffer.getNumChannels());

    const float aRapida = 1.0f - std::exp (-1.0f / (0.003f * (float) frecuencia));
    const float aLenta = 1.0f - std::exp (-1.0f / (0.15f * (float) frecuencia));

    float* datos[2] = { buffer.getWritePointer (0, inicio),
                        canales > 1 ? buffer.getWritePointer (1, inicio) : nullptr };

    // La envolvente de cada banda se mide por muestra; la campana se rehace
    // una vez por bloque, que a 15 ms el oído no lo distingue de continuo.
    for (int i = 0; i < n; ++i)
        for (int c = 0; c < canales; ++c)
        {
            const float x = datos[c][i];
            for (int b = 0; b < BANDAS; ++b)
            {
                const float banda = detectores[b][c].processSample (x);
                const float pico = std::abs (banda);
                envolventes[b] += (pico > envolventes[b] ? aRapida : aLenta) * (pico - envolventes[b]);
            }
        }

    for (int b = 0; b < BANDAS; ++b)
    {
        const float envDb = juce::Decibels::gainToDecibels (envolventes[b], -100.0f);
        const float exceso = envDb - pUmbrales[b]->getCurrentValue();
        const float reduccion = juce::jlimit (0.0f, pReducciones[b]->getCurrentValue(),
                                              exceso > 0.0f ? exceso * 0.7f : 0.0f);
        refrescar (b, -reduccion);
    }

    for (int c = 0; c < canales; ++c)
    {
        auto* canal = datos[c];
        for (int i = 0; i < n; ++i)
        {
            float v = canal[i];
            for (int b = 0; b < BANDAS; ++b)
                v = filtros[b][c].processSample (v);
            canal[i] = v;
        }
    }
}

void EQDinamicoPlugin::restorePluginStateFromValueTree (const juce::ValueTree& v)
{
    for (int b = 0; b < BANDAS; ++b)
        te::copyPropertiesToCachedValues (v, frecuencias[b], umbrales[b], reducciones[b]);
    for (auto p : getAutomatableParameters()) p->updateFromAttachedValue();
}

/* ============================================================== Balancín */

const char* BalancinPlugin::xmlTypeName = "balancin";

BalancinPlugin::BalancinPlugin (te::PluginCreationInfo info) : te::Plugin (info)
{
    auto um = getUndoManager();
    balance.referTo (state, "balance", um, 0.0f);
    pBalance = addParam ("balance", TRANS("Balance"), { -6.0f, 6.0f });
    pBalance->attachToCurrentValue (balance);
}

BalancinPlugin::~BalancinPlugin()
{
    notifyListenersOfDeletion();
    pBalance->detachFromCurrentValue();
}

void BalancinPlugin::initialise (const te::PluginInitialisationInfo& info)
{
    frecuencia = info.sampleRate;
    aplicado = 1e9f;
    for (int c = 0; c < 2; ++c) { graves[c].reset(); agudos[c].reset(); }
}

void BalancinPlugin::applyToBuffer (const te::PluginRenderContext& fc)
{
    if (! isEnabled() || fc.destBuffer == nullptr)
        return;

    auto& buffer = *fc.destBuffer;
    const int inicio = fc.bufferStartSample;
    const int n = fc.bufferNumSamples;
    const int canales = juce::jmin (2, buffer.getNumChannels());

    const float v = pBalance->getCurrentValue();
    if (std::abs (v - aplicado) > 0.05f)
    {
        aplicado = v;
        auto abajo = juce::dsp::IIR::Coefficients<float>::makeLowShelf (frecuencia, 650.0, 0.5f, dbAGanancia (-v));
        auto arriba = juce::dsp::IIR::Coefficients<float>::makeHighShelf (frecuencia, 650.0, 0.5f, dbAGanancia (v));
        for (int c = 0; c < 2; ++c)
        {
            graves[c].coefficients = abajo;
            agudos[c].coefficients = arriba;
        }
    }

    for (int c = 0; c < canales; ++c)
    {
        auto* datos = buffer.getWritePointer (c, inicio);
        for (int i = 0; i < n; ++i)
            datos[i] = agudos[c].processSample (graves[c].processSample (datos[i]));
    }
}

void BalancinPlugin::restorePluginStateFromValueTree (const juce::ValueTree& v)
{
    te::copyPropertiesToCachedValues (v, balance);
    for (auto p : getAutomatableParameters()) p->updateFromAttachedValue();
}

/* ======================================================== Pico verdadero */

void PicoVerdadero::preparar()
{
    if (listo)
        return;

    // Interpolador de 4 fases por sinc enventanada (Hann): la aproximación
    // práctica del sobremuestreo ×4 que pide BS.1770 para el pico verdadero.
    for (int fase = 0; fase < 4; ++fase)
        for (int k = 0; k < TAPS; ++k)
        {
            const double x = (double) k - (TAPS - 1) / 2.0 - fase / 4.0;
            const double sinc = x == 0.0 ? 1.0 : std::sin (juce::MathConstants<double>::pi * x)
                                                / (juce::MathConstants<double>::pi * x);
            const double ventana = 0.5 * (1.0 + std::cos (juce::MathConstants<double>::pi * x / (TAPS / 2.0)));
            fir[fase][k] = (float) (sinc * juce::jmax (0.0, ventana));
        }

    for (auto& h : historia) std::fill (std::begin (h), std::end (h), 0.0f);
    pos[0] = pos[1] = 0;
    listo = true;
}

float PicoVerdadero::medir (int canal, float muestra)
{
    canal = juce::jlimit (0, 1, canal);
    auto& h = historia[canal];
    h[(size_t) pos[canal]] = muestra;
    pos[canal] = (pos[canal] + 1) % TAPS;

    float pico = 0.0f;
    for (int fase = 0; fase < 4; ++fase)
    {
        float v = 0.0f;
        for (int k = 0; k < TAPS; ++k)
            v += h[(size_t) ((pos[canal] + k) % TAPS)] * fir[fase][k];
        pico = juce::jmax (pico, std::abs (v));
    }
    return pico;
}

/* =========================================================== Convolución */

const char* ConvolucionPlugin::xmlTypeName = "convolucion";

juce::File carpetaIRsDeFabrica()
{
    auto carpeta = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
                     .getChildFile ("PletinaDAW").getChildFile ("irs");

    if (! carpeta.getChildFile ("Placa brillante.wav").existsAsFile())
    {
        carpeta.createDirectory();

        // IRs sintéticas: ruido decayente filtrado. Honestas y sin licencias:
        // el que quiera una sala real, que cargue su propio WAV.
        struct Receta { const char* nombre; double segundos; float tono; float brillo; };
        const Receta recetas[] = { { "Placa brillante.wav", 1.4, 0.9995f, 0.75f },
                                   { "Sala pequena.wav",    0.5, 0.998f,  0.4f },
                                   { "Catedral.wav",        3.5, 0.9997f, 0.25f } };

        for (const auto& receta : recetas)
        {
            const double fs = 48000.0;
            const int n = (int) (receta.segundos * fs);
            juce::AudioBuffer<float> ir (2, n);
            juce::Random azar (1234);
            float lp[2] = {};

            for (int c = 0; c < 2; ++c)
            {
                float envolvente = 1.0f;
                const float caida = std::pow (0.001f, 1.0f / (float) n); // −60 dB al final
                for (int i = 0; i < n; ++i)
                {
                    const float blanco = azar.nextFloat() * 2.0f - 1.0f;
                    lp[c] += receta.brillo * (blanco - lp[c]);
                    ir.setSample (c, i, lp[c] * envolvente * 0.5f);
                    envolvente *= caida * receta.tono / receta.tono; // caída pura
                    envolvente *= caida;
                }
            }

            const auto archivo = carpeta.getChildFile (receta.nombre);
            archivo.deleteFile();
            juce::WavAudioFormat wav;
            if (auto flujo = archivo.createOutputStream())
                if (auto* escritor = wav.createWriterFor (flujo.release(), fs, 2, 24, {}, 0))
                {
                    std::unique_ptr<juce::AudioFormatWriter> e (escritor);
                    e->writeFromAudioSampleBuffer (ir, 0, n);
                }
        }
    }

    return carpeta;
}

ConvolucionPlugin::ConvolucionPlugin (te::PluginCreationInfo info) : te::Plugin (info)
{
    auto um = getUndoManager();
    mezcla.referTo (state, "mezcla", um, 0.3f);
    ganancia.referTo (state, "ganancia", um, 0.0f);
    rutaIR.referTo (state, "ir", um, {});

    pMezcla = addParam ("mezcla", TRANS("Mezcla"), { 0.0f, 1.0f });
    pGanancia = addParam ("ganancia", TRANS("Ganancia"), { -24.0f, 12.0f });
    pMezcla->attachToCurrentValue (mezcla);
    pGanancia->attachToCurrentValue (ganancia);
}

ConvolucionPlugin::~ConvolucionPlugin()
{
    notifyListenersOfDeletion();
    pMezcla->detachFromCurrentValue();
    pGanancia->detachFromCurrentValue();
}

double ConvolucionPlugin::getLatencySeconds()
{
    return convolucion.getLatency() / juce::jmax (1.0, frecuencia);
}

void ConvolucionPlugin::initialise (const te::PluginInitialisationInfo& info)
{
    frecuencia = info.sampleRate;
    bloqueMax = juce::jmax (64, info.blockSizeSamples);
    seca.setSize (2, bloqueMax);

    convolucion.prepare ({ frecuencia, (juce::uint32) bloqueMax, 2 });
    cargada.clear();
    cargarSiCambio();
}

void ConvolucionPlugin::cargarSiCambio()
{
    // Sin IR elegida, la de fábrica más socorrida.
    auto ruta = rutaIR.get();
    if (ruta.isEmpty())
        ruta = carpetaIRsDeFabrica().getChildFile ("Placa brillante.wav").getFullPathName();

    if (ruta == cargada)
        return;

    const juce::File archivo (ruta);
    if (! archivo.existsAsFile())
        return;

    cargada = ruta;
    // La carga es diferida y sin bloquear el audio: lo gestiona juce::dsp.
    convolucion.loadImpulseResponse (archivo,
                                     juce::dsp::Convolution::Stereo::yes,
                                     juce::dsp::Convolution::Trim::yes,
                                     0);
}

void ConvolucionPlugin::applyToBuffer (const te::PluginRenderContext& fc)
{
    if (! isEnabled() || fc.destBuffer == nullptr)
        return;

    cargarSiCambio();

    auto& buffer = *fc.destBuffer;
    const int inicio = fc.bufferStartSample;
    const int n = juce::jmin (fc.bufferNumSamples, bloqueMax);
    const int canales = juce::jmin (2, buffer.getNumChannels());

    for (int c = 0; c < canales; ++c)
        seca.copyFrom (c, 0, buffer, c, inicio, n);

    juce::dsp::AudioBlock<float> bloque (buffer);
    auto sub = bloque.getSubBlock ((size_t) inicio, (size_t) n).getSubsetChannelBlock (0, (size_t) canales);
    juce::dsp::ProcessContextReplacing<float> contexto (sub);
    convolucion.process (contexto);

    const float humedo = pMezcla->getCurrentValue() * juce::Decibels::decibelsToGain (pGanancia->getCurrentValue());
    const float secoNivel = 1.0f - pMezcla->getCurrentValue();

    for (int c = 0; c < canales; ++c)
    {
        auto* datos = buffer.getWritePointer (c, inicio);
        const auto* original = seca.getReadPointer (c);
        for (int i = 0; i < n; ++i)
            datos[i] = original[i] * secoNivel + datos[i] * humedo;
    }
}

void ConvolucionPlugin::restorePluginStateFromValueTree (const juce::ValueTree& v)
{
    te::copyPropertiesToCachedValues (v, mezcla, ganancia, rutaIR);
    for (auto p : getAutomatableParameters()) p->updateFromAttachedValue();
}
