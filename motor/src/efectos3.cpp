/*  Pletina DAW · motor
    Copyright (C) 2026 Up2you — GPLv3: ver el LICENSE de la raíz.

    El DSP de los clásicos. Cada emulación persigue el comportamiento del
    aparato — curvas, tiempos, saturación — con las reglas de siempre del
    hilo de audio.
*/

#include "efectos3.h"

#include <array>
#include <string>

namespace
{
    inline float dbAGanancia (float db) { return juce::Decibels::decibelsToGain (db, -100.0f); }
    inline float aDb (float g) { return juce::Decibels::gainToDecibels (g, -100.0f); }

    struct Canales
    {
        float* datos[2];
        int n, canales;
    };

    Canales abrir (const te::PluginRenderContext& fc)
    {
        auto& buffer = *fc.destBuffer;
        const int canales = juce::jmin (2, buffer.getNumChannels());
        return { { buffer.getWritePointer (0, fc.bufferStartSample),
                   canales > 1 ? buffer.getWritePointer (1, fc.bufferStartSample) : nullptr },
                 fc.bufferNumSamples, canales };
    }

    constexpr double FRACCIONES_TAP[9] = { 0.0625, 0.0833333, 0.09375, 0.125, 0.1666667, 0.1875, 0.25, 0.5, 1.0 };
}

/* ============================================================== Válvulas */

const char* ValvulasPlugin::xmlTypeName = "valvulas";
const char* ValvulasPlugin::NOMBRE = "V\xc3\xa1lvulas";
const std::vector<EspecParametro> ValvulasPlugin::PARAMETROS = {
    { "freqGrave", "Frecuencia grave", 20.0f, 100.0f, 60.0f, 0.0f, 0.5f },
    { "realce", "Realce grave", 0.0f, 10.0f, 3.0f },
    { "atenuacion", "Atenuaci\xc3\xb3n grave", 0.0f, 10.0f, 0.0f },
    { "freqAgudo", "Frecuencia aguda", 3000.0f, 16000.0f, 8000.0f, 0.0f, 0.5f },
    { "realceAgudo", "Realce agudo", 0.0f, 10.0f, 3.0f },
    { "anchoAgudo", "Ancho agudo", 0.4f, 3.0f, 1.0f },
    { "atenAgudos", "Aten\xc3\xba""a 10k", 0.0f, 10.0f, 0.0f },
};

void ValvulasPlugin::initialise (const te::PluginInitialisationInfo& info)
{
    fs = info.sampleRate;
    for (auto& banda : filtros) for (auto& f : banda) f.reset();
    for (auto& c : cache) c = -1e9f;
}

void ValvulasPlugin::applyToBuffer (const te::PluginRenderContext& fc)
{
    if (! isEnabled() || fc.destBuffer == nullptr) return;
    auto io = abrir (fc);

    bool cambio = false;
    for (int i = 0; i < 7; ++i) { if (P (i) != cache[i]) { cache[i] = P (i); cambio = true; } }

    if (cambio)
    {
        // El truco Pultec: realzar y atenuar graves A LA VEZ, con la atenuación
        // un poco más arriba: el codo que hace grandes los graves sin barro.
        auto realceG = juce::dsp::IIR::Coefficients<float>::makeLowShelf (fs, cache[0], 0.6f, dbAGanancia (cache[1] * 1.2f));
        auto corteG = juce::dsp::IIR::Coefficients<float>::makeLowShelf (fs, cache[0] * 1.9f, 0.8f, dbAGanancia (-cache[2] * 1.1f));
        auto campanaA = juce::dsp::IIR::Coefficients<float>::makePeakFilter (fs, cache[3], 0.7f / cache[5], dbAGanancia (cache[4] * 1.4f));
        auto corteA = juce::dsp::IIR::Coefficients<float>::makeHighShelf (fs, 10000.0, 0.7f, dbAGanancia (-cache[6] * 1.0f));
        for (int c = 0; c < 2; ++c)
        {
            filtros[0][c].coefficients = realceG;
            filtros[1][c].coefficients = corteG;
            filtros[2][c].coefficients = campanaA;
            filtros[3][c].coefficients = corteA;
        }
    }

    for (int c = 0; c < io.canales; ++c)
        for (int i = 0; i < io.n; ++i)
        {
            float v = io.datos[c][i];
            for (auto& banda : filtros) v = banda[c].processSample (v);
            io.datos[c][i] = std::tanh (v * 1.02f) * 0.9803f; // el aliento de la válvula
        }
}

/* =============================================================== Consola */

const char* ConsolaPlugin::xmlTypeName = "consola";
const char* ConsolaPlugin::NOMBRE = "Consola";
const std::vector<EspecParametro> ConsolaPlugin::PARAMETROS = {
    { "pasoAlto", "Paso alto", 20.0f, 350.0f, 20.0f, 0.0f, 0.5f },
    { "grave", "Grave", -15.0f, 15.0f, 0.0f },
    { "freqGrave", "Frec. grave", 40.0f, 600.0f, 110.0f, 0.0f, 0.5f },
    { "medioBajo", "Medio bajo", -15.0f, 15.0f, 0.0f },
    { "freqMedioBajo", "Frec. m. bajo", 200.0f, 2500.0f, 700.0f, 0.0f, 0.5f },
    { "medioAlto", "Medio alto", -15.0f, 15.0f, 0.0f },
    { "freqMedioAlto", "Frec. m. alto", 600.0f, 7000.0f, 2500.0f, 0.0f, 0.5f },
    { "agudo", "Agudo", -15.0f, 15.0f, 0.0f },
    { "freqAgudo", "Frec. agudo", 1500.0f, 16000.0f, 8000.0f, 0.0f, 0.5f },
};

void ConsolaPlugin::initialise (const te::PluginInitialisationInfo& info)
{
    fs = info.sampleRate;
    for (auto& banda : filtros) for (auto& f : banda) f.reset();
    for (auto& c : cache) c = -1e9f;
}

void ConsolaPlugin::applyToBuffer (const te::PluginRenderContext& fc)
{
    if (! isEnabled() || fc.destBuffer == nullptr) return;
    auto io = abrir (fc);

    bool cambio = false;
    for (int i = 0; i < 9; ++i) { if (P (i) != cache[i]) { cache[i] = P (i); cambio = true; } }

    if (cambio)
    {
        auto hp = juce::dsp::IIR::Coefficients<float>::makeHighPass (fs, juce::jmax (20.0f, cache[0]), 0.707f);
        auto lf = juce::dsp::IIR::Coefficients<float>::makeLowShelf (fs, cache[2], 0.75f, dbAGanancia (cache[1]));
        auto lmf = juce::dsp::IIR::Coefficients<float>::makePeakFilter (fs, cache[4], 0.9f, dbAGanancia (cache[3]));
        auto hmf = juce::dsp::IIR::Coefficients<float>::makePeakFilter (fs, cache[6], 0.9f, dbAGanancia (cache[5]));
        auto hf = juce::dsp::IIR::Coefficients<float>::makeHighShelf (fs, cache[8], 0.75f, dbAGanancia (cache[7]));
        for (int c = 0; c < 2; ++c)
        {
            filtros[0][c].coefficients = hp;
            filtros[1][c].coefficients = lf;
            filtros[2][c].coefficients = lmf;
            filtros[3][c].coefficients = hmf;
            filtros[4][c].coefficients = hf;
        }
    }

    const bool conPasoAlto = cache[0] > 22.0f;
    for (int c = 0; c < io.canales; ++c)
        for (int i = 0; i < io.n; ++i)
        {
            float v = io.datos[c][i];
            for (int b = conPasoAlto ? 0 : 1; b < 5; ++b) v = filtros[b][c].processSample (v);
            io.datos[c][i] = v;
        }
}

/* =============================================================== Remache */

const char* RemachePlugin::xmlTypeName = "remache";
const char* RemachePlugin::NOMBRE = "Remache";
const std::vector<EspecParametro> RemachePlugin::PARAMETROS = {
    { "entrada", "Entrada", -12.0f, 24.0f, 6.0f },
    { "salida", "Salida", -12.0f, 12.0f, 0.0f },
    { "ataque", "Ataque \xc2\xb5s", 20.0f, 800.0f, 200.0f, 0.0f, 0.5f },
    { "relajacion", "Relajaci\xc3\xb3n", 50.0f, 1100.0f, 300.0f },
    { "ratio", "Ratio (4/8/12/20/TODOS)", 0.0f, 4.0f, 1.0f, 1.0f },
};

void RemachePlugin::initialise (const te::PluginInitialisationInfo& info)
{
    fs = info.sampleRate;
    envolvente = 0.0f;
}

void RemachePlugin::applyToBuffer (const te::PluginRenderContext& fc)
{
    if (! isEnabled() || fc.destBuffer == nullptr) return;
    auto io = abrir (fc);

    const float gEntrada = dbAGanancia (P (0));
    const float gSalida = dbAGanancia (P (1));
    const int sel = juce::jlimit (0, 4, (int) std::lround (P (4)));
    const bool todos = sel == 4;
    static constexpr float RATIOS[4] = { 4.0f, 8.0f, 12.0f, 20.0f };
    const float ratio = todos ? 20.0f : RATIOS[sel];
    // El umbral del FET es fijo: se pega más metiendo entrada, como el de verdad.
    const float umbral = todos ? -14.0f : -10.0f;

    const float aAtaque = std::exp (-1.0f / (float) (P (2) * 1.0e-6 * fs));
    const float aRelaja = std::exp (-1.0f / (float) (P (3) * 0.001 * fs));

    for (int i = 0; i < io.n; ++i)
    {
        float izq = io.datos[0][i] * gEntrada;
        float der = io.datos[1] != nullptr ? io.datos[1][i] * gEntrada : izq;

        float pico = juce::jmax (std::abs (izq), std::abs (der));
        const float a = pico > envolvente ? aAtaque : aRelaja;
        envolvente = a * envolvente + (1.0f - a) * pico;

        const float exceso = aDb (envolvente) - umbral;
        float reduccion = exceso > 0.0f ? exceso * (1.0f - 1.0f / ratio) : 0.0f;
        if (todos) reduccion *= 1.15f; // el bombeo del modo TODOS

        const float g = dbAGanancia (-reduccion) * gSalida;
        // El hierro y el FET siempre dejan su firma, gane lo que gane.
        izq = std::tanh (izq * g * 1.1f) * 0.909f;
        der = std::tanh (der * g * 1.1f) * 0.909f;

        io.datos[0][i] = izq;
        if (io.datos[1] != nullptr) io.datos[1][i] = der;
    }
}

/* ================================================================== Ópto */

const char* OptoPlugin::xmlTypeName = "opto";
const char* OptoPlugin::NOMBRE = "\xc3\x93pto";
const std::vector<EspecParametro> OptoPlugin::PARAMETROS = {
    { "reduccion", "Reducci\xc3\xb3n", 0.0f, 100.0f, 40.0f },
    { "ganancia", "Ganancia", -12.0f, 24.0f, 4.0f },
    { "limitar", "Limitar", 0.0f, 1.0f, 0.0f, 1.0f },
};

void OptoPlugin::initialise (const te::PluginInitialisationInfo& info)
{
    fs = info.sampleRate;
    envolvente = celula = 0.0f;
}

void OptoPlugin::applyToBuffer (const te::PluginRenderContext& fc)
{
    if (! isEnabled() || fc.destBuffer == nullptr) return;
    auto io = abrir (fc);

    const float umbral = -30.0f + (100.0f - P (0)) * 0.35f; // más rueda, más abajo
    const float ratio = P (2) >= 0.5f ? 8.0f : 3.0f;
    const float gSalida = dbAGanancia (P (1));

    const float aRapido = std::exp (-1.0f / (0.010f * (float) fs));
    // La célula T4: una memoria lenta que se suma a la respuesta rápida. Es lo
    // que hace que el Ópto "respire" y suelte la voz con esa elegancia.
    const float aCelula = std::exp (-1.0f / (2.5f * (float) fs));

    for (int i = 0; i < io.n; ++i)
    {
        const float izq = io.datos[0][i];
        const float der = io.datos[1] != nullptr ? io.datos[1][i] : izq;
        const float pico = juce::jmax (std::abs (izq), std::abs (der));

        envolvente = pico > envolvente ? pico : aRapido * envolvente + (1.0f - aRapido) * pico;
        celula = aCelula * celula + (1.0f - aCelula) * envolvente;
        const float sentida = 0.7f * envolvente + 0.3f * celula;

        const float exceso = aDb (sentida) - umbral;
        // Codo suave de 10 dB: el Ópto nunca tiene esquinas.
        const float codo = juce::jlimit (0.0f, 1.0f, (exceso + 5.0f) / 10.0f);
        const float reduccion = juce::jmax (0.0f, exceso) * (1.0f - 1.0f / ratio) * codo;

        const float g = dbAGanancia (-reduccion) * gSalida;
        io.datos[0][i] = izq * g;
        if (io.datos[1] != nullptr) io.datos[1][i] = der * g;
    }
}

/* ================================================================ Lámpara */

const char* LamparaPlugin::xmlTypeName = "lampara";
const char* LamparaPlugin::NOMBRE = "L\xc3\xa1mpara";
const std::vector<EspecParametro> LamparaPlugin::PARAMETROS = {
    { "empuje", "Empuje", 0.0f, 24.0f, 6.0f },
    { "densidad", "Densidad", 0.0f, 10.0f, 4.0f },
    { "salida", "Salida", -12.0f, 12.0f, 0.0f },
};

void LamparaPlugin::initialise (const te::PluginInitialisationInfo& info)
{
    fs = info.sampleRate;
    envolvente = 0.0f;
}

void LamparaPlugin::applyToBuffer (const te::PluginRenderContext& fc)
{
    if (! isEnabled() || fc.destBuffer == nullptr) return;
    auto io = abrir (fc);

    const float gEntrada = dbAGanancia (P (0));
    const float gSalida = dbAGanancia (P (2) - P (0) * 0.5f);
    const float densidad = P (1);

    // Vari-mu: cuanto más señal, más ratio. Ataque y vuelta lentos, de programa.
    const float aAtaque = std::exp (-1.0f / (0.030f * (float) fs));
    const float aRelaja = std::exp (-1.0f / (0.400f * (float) fs));

    for (int i = 0; i < io.n; ++i)
    {
        float izq = io.datos[0][i] * gEntrada;
        float der = io.datos[1] != nullptr ? io.datos[1][i] * gEntrada : izq;

        const float pico = juce::jmax (std::abs (izq), std::abs (der));
        const float a = pico > envolvente ? aAtaque : aRelaja;
        envolvente = a * envolvente + (1.0f - a) * pico;

        const float exceso = juce::jmax (0.0f, aDb (envolvente) + 18.0f);
        const float reduccion = std::pow (exceso, 1.3f) * densidad * 0.02f;
        const float g = dbAGanancia (-juce::jmin (reduccion, 20.0f)) * gSalida;

        io.datos[0][i] = std::tanh (izq * g);
        if (io.datos[1] != nullptr) io.datos[1][i] = std::tanh (der * g);
    }
}

/* ==================================================================== Eco */

const char* EcoPlugin::xmlTypeName = "eco";
const char* EcoPlugin::NOMBRE = "Eco";
const std::vector<EspecParametro> EcoPlugin::PARAMETROS = {
    { "tiempo", "Tiempo", 30.0f, 1500.0f, 320.0f, 0.0f, 0.5f },
    { "realimentacion", "Realimentaci\xc3\xb3n", 0.0f, 1.1f, 0.45f },
    { "tono", "Tono", 1000.0f, 12000.0f, 4500.0f, 0.0f, 0.5f },
    { "flutter", "Flutter", 0.0f, 1.0f, 0.3f },
    { "mezcla", "Mezcla", 0.0f, 1.0f, 0.35f },
};

void EcoPlugin::initialise (const te::PluginInitialisationInfo& info)
{
    fs = info.sampleRate;
    for (auto& l : linea) l.assign ((size_t) (1.8 * fs), 0.0f);
    pos = 0;
    fases[0] = fases[1] = 0.0;
    retardoSuavizado = 0.0f;
    cacheTono = -1e9f;
    auto lp = juce::dsp::IIR::Coefficients<float>::makeLowPass (fs, 4500.0, 0.6f);
    auto shelf = juce::dsp::IIR::Coefficients<float>::makeLowShelf (fs, 120.0, 0.7f, dbAGanancia (2.5f));
    for (int c = 0; c < 2; ++c)
    {
        tono[c].coefficients = lp; tono[c].reset();
        bump[c].coefficients = shelf; bump[c].reset();
    }
}

void EcoPlugin::applyToBuffer (const te::PluginRenderContext& fc)
{
    if (! isEnabled() || fc.destBuffer == nullptr) return;
    auto io = abrir (fc);

    if (std::abs (P (2) - cacheTono) > 20.0f)
    {
        auto lp = juce::dsp::IIR::Coefficients<float>::makeLowPass (fs, P (2), 0.6f);
        for (auto& f : tono) f.coefficients = lp;
        cacheTono = P (2);
    }

    const float objetivo = (float) (P (0) / 1000.0 * fs);
    if (retardoSuavizado <= 0.0f) retardoSuavizado = objetivo;
    const float vuelta = P (1);
    const float vFlutter = P (3);
    const float humedo = P (4);
    const float seco = 1.0f - humedo * 0.5f;
    const int largo = (int) linea[0].size();

    for (int i = 0; i < io.n; ++i)
    {
        // La cinta nunca va exacta: wow lento y flutter rápido sobre el cabezal.
        fases[0] += 0.9 / fs;
        fases[1] += 6.3 / fs;
        const float lfo = (float) (std::sin (2.0 * juce::MathConstants<double>::pi * fases[0]) * 2.2
                                 + std::sin (2.0 * juce::MathConstants<double>::pi * fases[1]) * 0.7);
        retardoSuavizado += juce::jlimit (-1.0f, 1.0f, objetivo - retardoSuavizado) * 0.0015f;
        const float retardo = juce::jlimit (16.0f, (float) largo - 4.0f,
                                            retardoSuavizado + vFlutter * lfo * (float) (fs / 1000.0) * 0.35f);

        for (int c = 0; c < io.canales; ++c)
        {
            const float posicion = (float) pos - retardo;
            int a = (int) std::floor (posicion);
            const float fraccion = posicion - (float) a;
            a = ((a % largo) + largo) % largo;
            const float leida = linea[c][(size_t) a] * (1.0f - fraccion)
                              + linea[c][(size_t) ((a + 1) % largo)] * fraccion;

            // La vuelta pasa por el tono, el head-bump y la saturación de cinta:
            // cada repetición sale más oscura y más gorda, hasta autooscilar.
            const float filtrada = bump[c].processSample (tono[c].processSample (leida));
            const float entrante = io.datos[c] != nullptr ? io.datos[c][i] : io.datos[0][i];
            linea[c][(size_t) pos] = std::tanh (entrante + filtrada * vuelta);

            if (io.datos[c] != nullptr)
                io.datos[c][i] = entrante * seco + leida * humedo;
        }
        pos = (pos + 1) % largo;
    }
}

/* ================================================================= Muelle */

const char* MuellePlugin::xmlTypeName = "muelle";
const char* MuellePlugin::NOMBRE = "Muelle";
const std::vector<EspecParametro> MuellePlugin::PARAMETROS = {
    { "tension", "Tensi\xc3\xb3n", 0.0f, 1.0f, 0.5f },
    { "brillo", "Brillo", 1000.0f, 8000.0f, 3000.0f, 0.0f, 0.5f },
    { "mezcla", "Mezcla", 0.0f, 1.0f, 0.3f },
};

namespace { constexpr double MS_MUELLE[6] = { 3.1, 4.7, 6.1, 7.9, 9.7, 11.3 }; }

void MuellePlugin::initialise (const te::PluginInitialisationInfo& info)
{
    fs = info.sampleRate;
    for (int d = 0; d < 6; ++d)
    {
        dispersion[d].assign ((size_t) (MS_MUELLE[d] / 1000.0 * fs) + 2, 0.0f);
        posD[d] = 0;
    }
    for (auto& l : lazo) l.assign ((size_t) (0.055 * fs), 0.0f);
    posLazo = 0;
    cacheBrillo = -1e9f;
    auto lp = juce::dsp::IIR::Coefficients<float>::makeLowPass (fs, 3000.0, 0.707f);
    for (auto& f : brillo) { f.coefficients = lp; f.reset(); }
}

void MuellePlugin::applyToBuffer (const te::PluginRenderContext& fc)
{
    if (! isEnabled() || fc.destBuffer == nullptr) return;
    auto io = abrir (fc);

    if (std::abs (P (1) - cacheBrillo) > 20.0f)
    {
        auto lp = juce::dsp::IIR::Coefficients<float>::makeLowPass (fs, P (1), 0.707f);
        for (auto& f : brillo) f.coefficients = lp;
        cacheBrillo = P (1);
    }

    const float humedo = P (2);
    const float seco = 1.0f - humedo;
    const int retardoLazo = juce::jlimit (8, (int) lazo[0].size() - 2,
                                          (int) ((0.028 + P (0) * 0.022) * fs));
    const float g = 0.5f;

    for (int i = 0; i < io.n; ++i)
    {
        for (int c = 0; c < io.canales; ++c)
        {
            const float entrada = io.datos[c][i];

            // Los allpasses en cadena hacen el "chirrido dispersivo" del muelle:
            // los agudos llegan antes que los graves y ahí está el boing.
            float v = entrada;
            for (int d = c; d < 6; d += 2)
            {
                auto& l = dispersion[d];
                const float leida = l[(size_t) posD[d]];
                const float w = v - g * leida;
                l[(size_t) posD[d]] = w;
                posD[d] = (posD[d] + 1) % (int) l.size();
                v = leida + g * w;
            }

            const int largo = (int) lazo[c].size();
            const int lectura = (posLazo - retardoLazo + largo) % largo;
            const float cola = lazo[c][(size_t) lectura];
            lazo[c][(size_t) posLazo] = brillo[c].processSample (v + cola * 0.62f);

            io.datos[c][i] = entrada * seco + (v * 0.4f + cola) * humedo;
        }

        // El lazo avanza una vez por muestra, no por canal.
        posLazo = (posLazo + 1) % (int) lazo[0].size();
    }
}

/* ============================================================== Espejismo */

const char* EspejismoPlugin::xmlTypeName = "espejismo";
const char* EspejismoPlugin::NOMBRE = "Espejismo";
const std::vector<EspecParametro> EspejismoPlugin::PARAMETROS = {
    { "decaimiento", "Decaimiento", 1.0f, 15.0f, 5.0f, 0.0f, 0.5f },
    { "brillo", "Brillo", 0.0f, 1.0f, 0.5f },
    { "mezcla", "Mezcla", 0.0f, 1.0f, 0.3f },
};

namespace { constexpr double MS_ESPEJISMO[8] = { 47.9, 59.3, 67.1, 76.7, 85.1, 94.3, 103.9, 115.3 }; }

void EspejismoPlugin::initialise (const te::PluginInitialisationInfo& info)
{
    fs = info.sampleRate;
    for (int l = 0; l < 8; ++l)
    {
        lineas[l].assign ((size_t) (MS_ESPEJISMO[l] / 1000.0 * fs) + 2, 0.0f);
        posLinea[l] = 0;
        pasoBajo[l] = 0.0f;
    }
    octavador.assign ((size_t) (0.09 * fs), 0.0f);
    posOct = 0;
    lectura = 0.0;
}

void EspejismoPlugin::applyToBuffer (const te::PluginRenderContext& fc)
{
    if (! isEnabled() || fc.destBuffer == nullptr) return;
    auto io = abrir (fc);

    const float rt60 = P (0);
    const float vBrillo = P (1);
    const float humedo = P (2);
    const float seco = 1.0f - humedo;
    const float cAmortiguacion = 1.0f - std::exp (-2.0f * juce::MathConstants<float>::pi * 6000.0f / (float) fs);
    const int largoOct = (int) octavador.size();

    float ganancias[8];
    for (int l = 0; l < 8; ++l)
        ganancias[l] = std::pow (10.0f, -3.0f * (float) (MS_ESPEJISMO[l] / 1000.0) / rt60);

    for (int i = 0; i < io.n; ++i)
    {
        const float entradaIzq = io.datos[0][i];
        const float entradaDer = io.datos[1] != nullptr ? io.datos[1][i] : entradaIzq;

        float salidas[8];
        float suma = 0.0f;
        for (int l = 0; l < 8; ++l)
        {
            const float leida = lineas[l][(size_t) posLinea[l]];
            pasoBajo[l] += cAmortiguacion * (leida - pasoBajo[l]);
            salidas[l] = pasoBajo[l] * ganancias[l];
            suma += salidas[l];
        }

        const float colaIzq = salidas[0] - salidas[2] + salidas[4] - salidas[6];
        const float colaDer = salidas[1] - salidas[3] + salidas[5] - salidas[7];

        // El octavador granular barato: dos cabezales leyendo a 2× con
        // crossfade triangular. Sube la cola una octava y la devuelve al lazo.
        octavador[(size_t) posOct] = (colaIzq + colaDer) * 0.5f;
        lectura += 2.0;
        if (lectura >= largoOct) lectura -= largoOct;
        const auto leerOct = [&] (double desplazada)
        {
            double posicion = lectura + desplazada;
            while (posicion >= largoOct) posicion -= largoOct;
            const int a = (int) posicion;
            const float fraccion = (float) (posicion - a);
            return octavador[(size_t) a] * (1.0f - fraccion) + octavador[(size_t) ((a + 1) % largoOct)] * fraccion;
        };
        const float fase = (float) ((posOct - (int) lectura + largoOct) % largoOct) / (float) largoOct;
        const float ventana = 0.5f * (1.0f - std::cos (2.0f * juce::MathConstants<float>::pi * fase));
        const float octava = leerOct (0.0) * ventana + leerOct (largoOct / 2.0) * (1.0f - ventana);
        posOct = (posOct + 1) % largoOct;

        const float media2 = suma * 0.25f;
        const float alimentacion[2] = { entradaIzq + octava * vBrillo, entradaDer + octava * vBrillo };
        for (int l = 0; l < 8; ++l)
        {
            lineas[l][(size_t) posLinea[l]] = salidas[l] - media2 + alimentacion[l & 1] * 0.32f;
            posLinea[l] = (posLinea[l] + 1) % (int) lineas[l].size();
        }

        io.datos[0][i] = seco * entradaIzq + humedo * colaIzq;
        if (io.datos[1] != nullptr) io.datos[1][i] = seco * entradaDer + humedo * colaDer;
    }
}

/* =============================================================== Multitap */

const char* MultitapPlugin::xmlTypeName = "multitap";
const char* MultitapPlugin::NOMBRE = "Multitap";
const std::vector<EspecParametro> MultitapPlugin::PARAMETROS = {
    { "tiempo", "Tiempo", 0.0f, 8.0f, 4.0f, 1.0f },
    { "ecos", "Ecos", 1.0f, 4.0f, 3.0f, 1.0f },
    { "caida", "Ca\xc3\xad""da", 0.0f, 1.0f, 0.6f },
    { "anchura", "Anchura", 0.0f, 1.0f, 0.7f },
    { "mezcla", "Mezcla", 0.0f, 1.0f, 0.3f },
};

void MultitapPlugin::initialise (const te::PluginInitialisationInfo& info)
{
    fs = info.sampleRate;
    for (auto& l : linea) l.assign ((size_t) (4.0 * fs), 0.0f);
    pos = 0;
}

void MultitapPlugin::applyToBuffer (const te::PluginRenderContext& fc)
{
    if (! isEnabled() || fc.destBuffer == nullptr) return;
    auto io = abrir (fc);

    const double bpm = edit.tempoSequence.getBpmAt (te::TimePosition());
    const int indice = juce::jlimit (0, 8, (int) std::lround (P (0)));
    const int base = juce::jlimit (32, (int) linea[0].size() / 4 - 2,
                                   (int) (FRACCIONES_TAP[indice] * 4.0 * 60.0 / juce::jmax (20.0, bpm) * fs));
    const int ecos = juce::jlimit (1, 4, (int) std::lround (P (1)));
    const float caida = P (2);
    const float ancho = P (3);
    const float humedo = P (4);
    const float seco = 1.0f - humedo;
    const int largo = (int) linea[0].size();

    for (int i = 0; i < io.n; ++i)
    {
        const float entradaIzq = io.datos[0][i];
        const float entradaDer = io.datos[1] != nullptr ? io.datos[1][i] : entradaIzq;
        linea[0][(size_t) pos] = entradaIzq;
        linea[1][(size_t) pos] = entradaDer;

        float ecoIzq = 0.0f, ecoDer = 0.0f;
        float nivel = 1.0f;
        for (int t = 1; t <= ecos; ++t)
        {
            const int lectura = (pos - base * t % largo + largo) % largo;
            // Los taps alternan lados según la anchura: el ping-pong dibujado.
            const float lado = (t & 1) ? ancho : -ancho;
            const float izq = linea[0][(size_t) lectura] * nivel * (1.0f + lado) * 0.5f
                            + linea[1][(size_t) lectura] * nivel * (1.0f - lado) * 0.5f;
            const float der = linea[1][(size_t) lectura] * nivel * (1.0f + lado) * 0.5f
                            + linea[0][(size_t) lectura] * nivel * (1.0f - lado) * 0.5f;
            ecoIzq += izq;
            ecoDer += der;
            nivel *= caida;
        }

        io.datos[0][i] = entradaIzq * seco + ecoIzq * humedo;
        if (io.datos[1] != nullptr) io.datos[1][i] = entradaDer * seco + ecoDer * humedo;
        pos = (pos + 1) % largo;
    }
}

/* =================================================================== Coro */

const char* CoroPlugin::xmlTypeName = "coro";
const char* CoroPlugin::NOMBRE = "Coro";
const std::vector<EspecParametro> CoroPlugin::PARAMETROS = {
    { "modo", "Modo (coro/flanger/fase)", 0.0f, 2.0f, 0.0f, 1.0f },
    { "velocidad", "Velocidad", 0.05f, 5.0f, 0.6f, 0.0f, 0.5f },
    { "profundidad", "Profundidad", 0.0f, 1.0f, 0.5f },
    { "realimentacion", "Realimentaci\xc3\xb3n", 0.0f, 0.9f, 0.2f },
    { "mezcla", "Mezcla", 0.0f, 1.0f, 0.5f },
};

void CoroPlugin::initialise (const te::PluginInitialisationInfo& info)
{
    fs = info.sampleRate;
    for (auto& l : linea) l.assign ((size_t) (0.06 * fs), 0.0f);
    pos = 0;
    fase = 0.0;
    for (auto& etapas : allpass) etapas[0] = etapas[1] = 0.0f;
    re[0] = re[1] = 0.0f;
}

void CoroPlugin::applyToBuffer (const te::PluginRenderContext& fc)
{
    if (! isEnabled() || fc.destBuffer == nullptr) return;
    auto io = abrir (fc);

    const int modo = juce::jlimit (0, 2, (int) std::lround (P (0)));
    const double velocidad = P (1);
    const float profundidad = P (2);
    const float vuelta = P (3);
    const float humedo = P (4);
    const float seco = 1.0f - humedo * 0.5f;
    const int largo = (int) linea[0].size();

    for (int i = 0; i < io.n; ++i)
    {
        fase += velocidad / fs;
        if (fase >= 1.0) fase -= 1.0;

        for (int c = 0; c < io.canales; ++c)
        {
            const float entrada = io.datos[c][i];
            const double faseCanal = fase + (c == 1 ? 0.25 : 0.0);
            const float lfo = 0.5f + 0.5f * (float) std::sin (2.0 * juce::MathConstants<double>::pi * faseCanal);
            float mojada;

            if (modo == 2)
            {
                // Fase: cuatro allpasses de primer orden barridos por el LFO.
                const float f = 220.0f + lfo * profundidad * 1600.0f;
                const float k = (float) ((std::tan (juce::MathConstants<double>::pi * f / fs) - 1.0)
                                        / (std::tan (juce::MathConstants<double>::pi * f / fs) + 1.0));
                float v = entrada + re[c] * vuelta;
                for (auto& z : allpass)
                {
                    const float y = k * v + z[c];
                    z[c] = v - k * y;
                    v = y;
                }
                mojada = v;
                re[c] = v;
            }
            else
            {
                const float centro = modo == 0 ? 18.0f : 3.5f;   // ms
                const float vaiven = modo == 0 ? 6.0f : 3.0f;
                const float retardo = (centro + (lfo - 0.5f) * 2.0f * vaiven * profundidad) * (float) (fs / 1000.0);

                const float posicion = (float) pos - juce::jlimit (8.0f, (float) largo - 4.0f, retardo);
                int a = (int) std::floor (posicion);
                const float fraccion = posicion - (float) a;
                a = ((a % largo) + largo) % largo;
                mojada = linea[c][(size_t) a] * (1.0f - fraccion) + linea[c][(size_t) ((a + 1) % largo)] * fraccion;
                linea[c][(size_t) pos] = entrada + mojada * (modo == 1 ? vuelta : vuelta * 0.3f);
            }

            io.datos[c][i] = entrada * seco + mojada * humedo;
        }
        pos = (pos + 1) % largo;
    }
}

/* ================================================================ Trémolo */

const char* TremoloPlugin::xmlTypeName = "tremolo";
const char* TremoloPlugin::NOMBRE = "Tr\xc3\xa9molo";
const std::vector<EspecParametro> TremoloPlugin::PARAMETROS = {
    { "modo", "Modo (tr\xc3\xa9molo/autopan)", 0.0f, 1.0f, 0.0f, 1.0f },
    { "velocidad", "Velocidad", 0.1f, 20.0f, 4.5f, 0.0f, 0.5f },
    { "profundidad", "Profundidad", 0.0f, 1.0f, 0.6f },
};

void TremoloPlugin::initialise (const te::PluginInitialisationInfo& info)
{
    fs = info.sampleRate;
    fase = 0.0;
}

void TremoloPlugin::applyToBuffer (const te::PluginRenderContext& fc)
{
    if (! isEnabled() || fc.destBuffer == nullptr) return;
    auto io = abrir (fc);

    const bool panorama = P (0) >= 0.5f;
    const double velocidad = P (1);
    const float profundidad = P (2);

    for (int i = 0; i < io.n; ++i)
    {
        fase += velocidad / fs;
        if (fase >= 1.0) fase -= 1.0;
        const float lfo = (float) std::sin (2.0 * juce::MathConstants<double>::pi * fase);

        if (panorama && io.canales > 1)
        {
            const float giro = lfo * profundidad;
            io.datos[0][i] *= 0.5f * (1.0f - giro) + 0.5f;
            io.datos[1][i] *= 0.5f * (1.0f + giro) + 0.5f;
        }
        else
        {
            const float g = 1.0f - profundidad * (0.5f + 0.5f * lfo);
            io.datos[0][i] *= g;
            if (io.datos[1] != nullptr) io.datos[1][i] *= g;
        }
    }
}

/* ================================================================= Triodo */

const char* TriodoPlugin::xmlTypeName = "triodo";
const char* TriodoPlugin::NOMBRE = "Triodo";
const std::vector<EspecParametro> TriodoPlugin::PARAMETROS = {
    { "empuje", "Empuje", 0.0f, 30.0f, 8.0f },
    { "sesgo", "Sesgo", 0.0f, 1.0f, 0.3f },
    { "tono", "Tono", 2000.0f, 20000.0f, 10000.0f, 0.0f, 0.5f },
    { "mezcla", "Mezcla", 0.0f, 1.0f, 1.0f },
};

void TriodoPlugin::initialise (const te::PluginInitialisationInfo& info)
{
    fs = info.sampleRate;
    cacheTono = -1e9f;
    for (auto& f : tono) f.reset();
}

void TriodoPlugin::applyToBuffer (const te::PluginRenderContext& fc)
{
    if (! isEnabled() || fc.destBuffer == nullptr) return;
    auto io = abrir (fc);

    if (std::abs (P (2) - cacheTono) > 20.0f)
    {
        cacheTono = P (2);
        auto lp = juce::dsp::IIR::Coefficients<float>::makeLowPass (fs, cacheTono, 0.707f);
        for (auto& f : tono) f.coefficients = lp;
    }

    const float g = dbAGanancia (P (0));
    const float sesgo = P (1) * 0.12f;
    const float compensa = std::tanh (g * sesgo);
    const float humedo = P (3);
    const float seco = 1.0f - humedo;
    const float salida = dbAGanancia (-P (0) * 0.55f);

    for (int c = 0; c < io.canales; ++c)
        for (int i = 0; i < io.n; ++i)
        {
            const float x = io.datos[c][i];
            // El sesgo desplaza el punto de trabajo: armónicos pares, calor.
            const float v = tono[c].processSample ((std::tanh (g * (x + sesgo)) - compensa) * salida);
            io.datos[c][i] = x * seco + v * humedo;
        }
}

/* =============================================================== Sumadora */

const char* SumadoraPlugin::xmlTypeName = "sumadora";
const char* SumadoraPlugin::NOMBRE = "Sumadora";
const std::vector<EspecParametro> SumadoraPlugin::PARAMETROS = {
    { "empuje", "Empuje", 0.0f, 12.0f, 4.0f },
    { "diafonia", "Diafon\xc3\xad""a", 0.0f, 1.0f, 0.3f },
    { "mezcla", "Mezcla", 0.0f, 1.0f, 1.0f },
};

void SumadoraPlugin::initialise (const te::PluginInitialisationInfo& info) { fs = info.sampleRate; }

void SumadoraPlugin::applyToBuffer (const te::PluginRenderContext& fc)
{
    if (! isEnabled() || fc.destBuffer == nullptr || fc.destBuffer->getNumChannels() < 2) return;
    auto io = abrir (fc);

    const float g = dbAGanancia (P (0));
    const float salida = dbAGanancia (-P (0) * 0.7f);
    const float fuga = P (1) * 0.012f; // −38 dB de diafonía al máximo
    const float humedo = P (2);
    const float seco = 1.0f - humedo;

    for (int i = 0; i < io.n; ++i)
    {
        const float izq = io.datos[0][i];
        const float der = io.datos[1][i];
        // Cada canal satura ligeramente distinto, como dos VCA hermanos no
        // idénticos, y algo del vecino siempre se cuela por el bastidor.
        const float vIzq = std::tanh (g * (izq + der * fuga) * 1.00f) * salida;
        const float vDer = std::tanh (g * (der + izq * fuga) * 1.02f) * salida * 0.995f;
        io.datos[0][i] = izq * seco + vIzq * humedo;
        io.datos[1][i] = der * seco + vDer * humedo;
    }
}

/* ============================================================ Machacadora */

const char* MachacadoraPlugin::xmlTypeName = "machacadora";
const char* MachacadoraPlugin::NOMBRE = "Machacadora";
const std::vector<EspecParametro> MachacadoraPlugin::PARAMETROS = {
    { "bits", "Bits", 1.0f, 16.0f, 8.0f, 1.0f },
    { "diezmado", "Diezmado", 1.0f, 50.0f, 4.0f, 1.0f },
    { "mezcla", "Mezcla", 0.0f, 1.0f, 1.0f },
};

void MachacadoraPlugin::initialise (const te::PluginInitialisationInfo& info)
{
    fs = info.sampleRate;
    retenida[0] = retenida[1] = 0.0f;
    contador = 0;
}

void MachacadoraPlugin::applyToBuffer (const te::PluginRenderContext& fc)
{
    if (! isEnabled() || fc.destBuffer == nullptr) return;
    auto io = abrir (fc);

    const float paso = std::pow (2.0f, 1.0f - std::round (P (0)));
    const int diezmado = juce::jlimit (1, 50, (int) std::lround (P (1)));
    const float humedo = P (2);
    const float seco = 1.0f - humedo;

    for (int i = 0; i < io.n; ++i)
    {
        if (contador == 0)
            for (int c = 0; c < io.canales; ++c)
                retenida[c] = std::round (io.datos[c][i] / paso) * paso;
        contador = (contador + 1) % diezmado;

        for (int c = 0; c < io.canales; ++c)
            io.datos[c][i] = io.datos[c][i] * seco + retenida[c] * humedo;
    }
}

/* ================================================================== Peine */

const char* PeinePlugin::xmlTypeName = "peine";
const char* PeinePlugin::NOMBRE = "Peine";

namespace
{
    constexpr int FRECUENCIAS_PEINE[31] = { 20, 25, 31, 40, 50, 63, 80, 100, 125, 160, 200, 250,
                                            315, 400, 500, 630, 800, 1000, 1250, 1600, 2000, 2500,
                                            3150, 4000, 5000, 6300, 8000, 10000, 12500, 16000, 20000 };

    std::vector<EspecParametro> hacerBandasPeine()
    {
        // Las 31 bandas ISO de tercio de octava, de 20 Hz a 20 kHz. Los ids y
        // etiquetas viven en arrays estáticos de tamaño fijo: EspecParametro
        // guarda const char* y un vector que crece los invalidaría.
        static std::array<std::string, 31> nombres, visibles;
        std::vector<EspecParametro> bandas;
        for (size_t i = 0; i < 31; ++i)
        {
            const int f = FRECUENCIAS_PEINE[i];
            nombres[i] = "g" + std::to_string (i);
            if (f < 1000)
                visibles[i] = std::to_string (f);
            else if (f % 1000 == 0)
                visibles[i] = std::to_string (f / 1000) + "k";
            else
            {
                // 1250 -> 1.25k, 1600 -> 1.6k, 3150 -> 3.15k, 12500 -> 12.5k
                std::string s = std::to_string (f / 1000) + "." + std::to_string (f % 1000 / 10);
                while (s.back() == '0') s.pop_back();
                visibles[i] = s + "k";
            }
            bandas.push_back ({ nombres[i].c_str(), visibles[i].c_str(), -12.0f, 12.0f, 0.0f });
        }
        return bandas;
    }
}

const std::vector<EspecParametro> PeinePlugin::PARAMETROS = hacerBandasPeine();

void PeinePlugin::initialise (const te::PluginInitialisationInfo& info)
{
    fs = info.sampleRate;
    for (auto& banda : filtros) for (auto& f : banda) f.reset();
    for (auto& c : cache) c = -1e9f;
}

void PeinePlugin::applyToBuffer (const te::PluginRenderContext& fc)
{
    if (! isEnabled() || fc.destBuffer == nullptr) return;
    auto io = abrir (fc);

    for (int b = 0; b < 31; ++b)
    {
        const float v = P (b);
        if (v != cache[b] && FRECUENCIAS_PEINE[b] < fs * 0.48)
        {
            cache[b] = v;
            auto coeficientes = juce::dsp::IIR::Coefficients<float>::makePeakFilter (fs, FRECUENCIAS_PEINE[b], 4.3f, dbAGanancia (v));
            for (int c = 0; c < 2; ++c) filtros[b][c].coefficients = coeficientes;
        }
    }

    for (int c = 0; c < io.canales; ++c)
        for (int i = 0; i < io.n; ++i)
        {
            float v = io.datos[c][i];
            for (int b = 0; b < 31; ++b)
                if (cache[b] != 0.0f && FRECUENCIAS_PEINE[b] < fs * 0.48)
                    v = filtros[b][c].processSample (v);
            io.datos[c][i] = v;
        }
}

/* ============================================================== registro */

void registrarClasicos (te::Engine& engine)
{
    auto& plugins = engine.getPluginManager();
    plugins.createBuiltInType<ValvulasPlugin>();
    plugins.createBuiltInType<ConsolaPlugin>();
    plugins.createBuiltInType<RemachePlugin>();
    plugins.createBuiltInType<OptoPlugin>();
    plugins.createBuiltInType<LamparaPlugin>();
    plugins.createBuiltInType<EcoPlugin>();
    plugins.createBuiltInType<MuellePlugin>();
    plugins.createBuiltInType<EspejismoPlugin>();
    plugins.createBuiltInType<MultitapPlugin>();
    plugins.createBuiltInType<CoroPlugin>();
    plugins.createBuiltInType<TremoloPlugin>();
    plugins.createBuiltInType<TriodoPlugin>();
    plugins.createBuiltInType<SumadoraPlugin>();
    plugins.createBuiltInType<MachacadoraPlugin>();
    plugins.createBuiltInType<PeinePlugin>();
}
