/*  Pletina DAW · motor
    Copyright (C) 2026 Up2you — GPLv3: ver el LICENSE de la raíz.

    Los instrumentos. Regla compartida: los eventos MIDI se aplican al
    principio del bloque (≤ ~10 ms de rejilla interna, suficiente para la
    v1 y dicho sin vergüenza); las voces se roban por edad; el hilo de audio
    no toca disco jamás — las muestras se cargan desde el hilo de mensajes.
*/

#include "efectos4.h"

#include <cmath>

namespace
{
    inline float dbAGanancia (float db) { return juce::Decibels::decibelsToGain (db, -100.0f); }

    struct Canales
    {
        float* datos[2];
        int n, canales;
    };

    Canales abrirLimpio (const te::PluginRenderContext& fc)
    {
        auto& buffer = *fc.destBuffer;
        const int canales = juce::jmin (2, buffer.getNumChannels());
        for (int c = 0; c < buffer.getNumChannels(); ++c)
            buffer.clear (c, fc.bufferStartSample, fc.bufferNumSamples);
        return { { buffer.getWritePointer (0, fc.bufferStartSample),
                   canales > 1 ? buffer.getWritePointer (1, fc.bufferStartSample) : nullptr },
                 fc.bufferNumSamples, canales };
    }

    inline double hzDeNota (double nota) { return 440.0 * std::pow (2.0, (nota - 69.0) / 12.0); }

    // El parche polyBLEP: lima el salto de sierra y cuadrada para que el
    // aliasing no pite. No es sobremuestreo (eso, en F6): es el apaño digno.
    inline float polyblep (double t, double dt)
    {
        if (t < dt)       { const double x = t / dt;         return (float) (x + x - x * x - 1.0); }
        if (t > 1.0 - dt) { const double x = (t - 1.0) / dt; return (float) (x * x + x + x + 1.0); }
        return 0.0f;
    }
}

/* ================================================================= Bruma */

const char* BrumaPlugin::xmlTypeName = "bruma";
const char* BrumaPlugin::NOMBRE = "Bruma";
const std::vector<EspecParametro> BrumaPlugin::PARAMETROS = {
    { "forma", "Forma (sierra/cuadr/tri)", 0.0f, 2.0f, 0.0f, 1.0f },
    { "desafinar", "Desafinar", 0.0f, 25.0f, 7.0f },
    { "corte", "Corte", 100.0f, 12000.0f, 2200.0f, 0.0f, 0.5f },
    { "resonancia", "Resonancia", 0.0f, 0.95f, 0.3f },
    { "envFiltro", "Env \xe2\x86\x92 filtro", 0.0f, 1.0f, 0.5f },
    { "ataque", "Ataque", 1.0f, 2000.0f, 4.0f, 0.0f, 0.5f },
    { "caida", "Ca\xc3\xad""da", 10.0f, 4000.0f, 400.0f, 0.0f, 0.5f },
    { "sostenido", "Sostenido", 0.0f, 1.0f, 0.6f },
    { "relajacion", "Relajaci\xc3\xb3n", 5.0f, 4000.0f, 300.0f, 0.0f, 0.5f },
    { "nivel", "Nivel", -24.0f, 6.0f, -6.0f },
};

void BrumaPlugin::initialise (const te::PluginInitialisationInfo& info)
{
    fs = info.sampleRate;
    for (auto& voz : voces) voz = {};
    reloj = 0;
}

void BrumaPlugin::applyToBuffer (const te::PluginRenderContext& fc)
{
    if (fc.destBuffer == nullptr) return;
    auto io = abrirLimpio (fc);
    if (! isEnabled()) return;

    if (fc.bufferForMidiMessages != nullptr)
    {
        if (fc.bufferForMidiMessages->isAllNotesOff)
            for (auto& voz : voces) voz = {};

        for (const auto& m : *fc.bufferForMidiMessages)
        {
            if (m.isNoteOn())
            {
                // Voz libre o, si no hay, la más vieja: el robo de toda la vida.
                Voz* elegida = nullptr;
                for (auto& voz : voces) if (! voz.viva) { elegida = &voz; break; }
                if (elegida == nullptr)
                {
                    elegida = &voces[0];
                    for (auto& voz : voces) if (voz.edad < elegida->edad) elegida = &voz;
                }
                *elegida = {};
                elegida->viva = true;
                elegida->nota = m.getNoteNumber();
                elegida->velocidad = 0.25f + 0.75f * m.getFloatVelocity();
                elegida->edad = ++reloj;
            }
            else if (m.isNoteOff())
            {
                for (auto& voz : voces)
                    if (voz.viva && voz.nota == m.getNoteNumber())
                        voz.soltada = true;
            }
        }
    }

    const int forma = juce::jlimit (0, 2, (int) std::lround (P (0)));
    const double desafinar = std::pow (2.0, P (1) / 1200.0);
    const float corte = P (2);
    const float resonancia = P (3);
    const float aFiltroEnv = P (4);
    const float subidaAtaque = 1.0f / (float) juce::jmax (1.0, P (5) * 0.001 * fs);
    const float aCaida = std::exp (-1.0f / (float) (P (6) * 0.001 * fs));
    const float sostenido = P (7);
    const float aRelaja = std::exp (-1.0f / (float) (P (8) * 0.001 * fs));
    const float nivel = dbAGanancia (P (9)) * 0.3f;

    for (auto& voz : voces)
    {
        if (! voz.viva) continue;

        const double hz = hzDeNota (voz.nota);
        const double paso1 = hz / fs;
        const double paso2 = hz * desafinar / fs;
        const float q = 1.0f - resonancia;

        for (int i = 0; i < io.n; ++i)
        {
            // La envolvente: ataque lineal, caída y relajación exponenciales.
            if (voz.soltada)
                voz.envolvente *= aRelaja;
            else if (voz.envolvente < 1.0f && voz.edad == reloj)
                voz.envolvente = juce::jmin (1.0f, voz.envolvente + subidaAtaque);
            else
                voz.envolvente = sostenido + (voz.envolvente - sostenido) * aCaida;

            if (voz.envolvente < 0.001f && voz.soltada) { voz.viva = false; break; }

            auto oscilar = [&] (double& t, double dt) -> float
            {
                t += dt; if (t >= 1.0) t -= 1.0;
                switch (forma)
                {
                    case 0: return (float) (2.0 * t - 1.0) - polyblep (t, dt);
                    case 1: return (t < 0.5 ? 1.0f : -1.0f)
                                 + polyblep (t, dt) - polyblep (std::fmod (t + 0.5, 1.0), dt);
                    default: return (float) (2.0 * std::abs (2.0 * t - 1.0) - 1.0);
                }
            };

            const float cruda = 0.5f * (oscilar (voz.fase1, paso1) + oscilar (voz.fase2, paso2));

            // SVF Chamberlin, con la envolvente empujando el corte hacia arriba.
            const float corteVoz = juce::jlimit (60.0f, (float) (fs * 0.22),
                                                 corte * (1.0f + aFiltroEnv * 3.0f * voz.envolvente));
            const float f = 2.0f * std::sin (juce::MathConstants<float>::pi * corteVoz / (float) fs);
            voz.pasoBajo += f * voz.pasoBanda;
            const float alto = cruda - voz.pasoBajo - q * voz.pasoBanda;
            voz.pasoBanda += f * alto;

            const float v = voz.pasoBajo * voz.envolvente * voz.velocidad * nivel;
            io.datos[0][i] += v;
            if (io.datos[1] != nullptr) io.datos[1][i] += v;
        }

        if (voz.envolvente < 0.001f && voz.soltada) voz.viva = false;
    }
}

/* ================================================================= Cinta */

const char* CintaPlugin::xmlTypeName = "cinta";
const char* CintaPlugin::NOMBRE = "Cinta";
const std::vector<EspecParametro> CintaPlugin::PARAMETROS = {
    { "raiz", "Nota ra\xc3\xadz", 24.0f, 96.0f, 60.0f, 1.0f },
    { "ataque", "Ataque", 0.0f, 500.0f, 2.0f, 0.0f, 0.5f },
    { "relajacion", "Relajaci\xc3\xb3n", 5.0f, 2000.0f, 160.0f, 0.0f, 0.5f },
    { "bucle", "Bucle", 0.0f, 1.0f, 0.0f, 1.0f },
    { "nivel", "Nivel", -24.0f, 6.0f, -3.0f },
};

CintaPlugin::CintaPlugin (te::PluginCreationInfo info) : InstrumentoSuite (info)
{
    rutaMuestra.referTo (state, juce::Identifier ("rutaMuestra"), getUndoManager(), {});
}

CintaPlugin::~CintaPlugin() = default;

void CintaPlugin::initialise (const te::PluginInitialisationInfo& info)
{
    fs = info.sampleRate;
    for (auto& voz : voces) voz = {};
    asegurarMuestra();
}

void CintaPlugin::restorePluginStateFromValueTree (const juce::ValueTree& v)
{
    if (v.hasProperty (rutaMuestra.getPropertyID()))
        rutaMuestra.setValue (v[rutaMuestra.getPropertyID()].toString(), getUndoManager());
    InstrumentoSuite::restorePluginStateFromValueTree (v);
    asegurarMuestra();
}

void CintaPlugin::asegurarMuestra()
{
    if (rutaCargada != rutaMuestra.get())
        cargarMuestra (rutaMuestra.get());
}

void CintaPlugin::cargarMuestra (const juce::String& ruta)
{
    juce::AudioBuffer<float> nueva;
    double fsNueva = 48000.0;

    if (ruta.isNotEmpty() && juce::File (ruta).existsAsFile())
    {
        static juce::AudioFormatManager formatos;
        static bool formatosListos = false;
        if (! formatosListos) { formatos.registerBasicFormats(); formatosListos = true; }

        if (std::unique_ptr<juce::AudioFormatReader> lector { formatos.createReaderFor (juce::File (ruta)) })
        {
            const int n = (int) juce::jmin ((juce::int64) (lector->sampleRate * 20), lector->lengthInSamples);
            nueva.setSize (juce::jmin (2, (int) lector->numChannels), n);
            lector->read (&nueva, 0, n, 0, true, lector->numChannels > 1);
            fsNueva = lector->sampleRate;
        }
    }

    if (nueva.getNumSamples() == 0)
    {
        // La muestra de fábrica: una cuerda pulsada (Karplus-Strong) en Do4.
        // Sin archivos externos: el sampler suena nada más insertarse.
        fsNueva = 48000.0;
        const int n = (int) (0.9 * fsNueva);
        nueva.setSize (1, n);
        const int largo = (int) (fsNueva / 261.63);
        std::vector<float> cuerda ((size_t) largo);
        juce::Random azar (7331);
        for (auto& c : cuerda) c = azar.nextFloat() * 2.0f - 1.0f;
        int p = 0;
        for (int i = 0; i < n; ++i)
        {
            const float v = cuerda[(size_t) p];
            const float siguiente = cuerda[(size_t) ((p + 1) % largo)];
            cuerda[(size_t) p] = (v + siguiente) * 0.4985f;
            nueva.setSample (0, i, v * 0.8f);
            p = (p + 1) % largo;
        }
    }

    const juce::ScopedLock candadoCarga (candado);
    muestra = std::move (nueva);
    fsMuestra = fsNueva;
    rutaCargada = ruta;
}

void CintaPlugin::applyToBuffer (const te::PluginRenderContext& fc)
{
    if (fc.destBuffer == nullptr) return;
    auto io = abrirLimpio (fc);
    if (! isEnabled()) return;

    const juce::ScopedTryLock intento (candado);
    if (! intento.isLocked()) return;   // están cambiando la muestra: un bloque de silencio
    if (muestra.getNumSamples() == 0) return;

    if (fc.bufferForMidiMessages != nullptr)
    {
        if (fc.bufferForMidiMessages->isAllNotesOff)
            for (auto& voz : voces) voz = {};

        for (const auto& m : *fc.bufferForMidiMessages)
        {
            if (m.isNoteOn())
            {
                Voz* elegida = nullptr;
                for (auto& voz : voces) if (! voz.viva) { elegida = &voz; break; }
                if (elegida == nullptr)
                {
                    elegida = &voces[0];
                    for (auto& voz : voces) if (voz.edad < elegida->edad) elegida = &voz;
                }
                *elegida = {};
                elegida->viva = true;
                elegida->nota = m.getNoteNumber();
                elegida->velocidad = 0.25f + 0.75f * m.getFloatVelocity();
                static int relojCinta = 0;
                elegida->edad = ++relojCinta;
            }
            else if (m.isNoteOff())
            {
                for (auto& voz : voces)
                    if (voz.viva && voz.nota == m.getNoteNumber())
                        voz.soltada = true;
            }
        }
    }

    const double raiz = P (0);
    const float subida = 1.0f / (float) juce::jmax (1.0, P (1) * 0.001 * fs);
    const float aRelaja = std::exp (-1.0f / (float) (P (2) * 0.001 * fs));
    const bool bucle = P (3) >= 0.5f;
    const float nivel = dbAGanancia (P (4));
    const int largo = muestra.getNumSamples();
    const int canalesMuestra = muestra.getNumChannels();

    for (auto& voz : voces)
    {
        if (! voz.viva) continue;
        const double paso = std::pow (2.0, ((double) voz.nota - raiz) / 12.0) * fsMuestra / fs;

        for (int i = 0; i < io.n; ++i)
        {
            if (voz.soltada) voz.envolvente *= aRelaja;
            else voz.envolvente = juce::jmin (1.0f, voz.envolvente + subida);

            if (voz.posicion >= largo - 1)
            {
                if (bucle) voz.posicion -= largo - 1;
                else { voz.viva = false; break; }
            }
            if (voz.envolvente < 0.001f && voz.soltada) { voz.viva = false; break; }

            const int a = (int) voz.posicion;
            const float fraccion = (float) (voz.posicion - a);
            const float g = voz.envolvente * voz.velocidad * nivel;

            for (int c = 0; c < io.canales; ++c)
            {
                const int cm = juce::jmin (c, canalesMuestra - 1);
                const float v = muestra.getSample (cm, a) * (1.0f - fraccion)
                              + muestra.getSample (cm, juce::jmin (a + 1, largo - 1)) * fraccion;
                io.datos[c][i] += v * g;
            }
            voz.posicion += paso;
        }
    }
}

/* ================================================================== Pads */

const char* PadsPlugin::xmlTypeName = "pads";
const char* PadsPlugin::NOMBRE = "Pads";
const std::vector<EspecParametro> PadsPlugin::PARAMETROS = {
    { "nivel", "Nivel", -24.0f, 6.0f, -3.0f },
    { "tono", "Tono", -12.0f, 12.0f, 0.0f },
    { "decaimiento", "Decaimiento", 0.3f, 3.0f, 1.0f },
};

namespace
{
    // Cada pad: familia de síntesis, dos frecuencias y duración base.
    enum FamiliaPad { BOMBO, TONO_CAIDA, RUIDO_ALTO, RUIDO_BANDA, CLAP, METAL, ZAP };
    struct EspecPad { FamiliaPad familia; float f1, f2, durMs; };

    const EspecPad ESPEC_PADS[PadsPlugin::PADS] = {
        { BOMBO,       110.0f,   42.0f,  380.0f },  // 36 bombo
        { TONO_CAIDA,  195.0f,  165.0f,  190.0f },  // 37 caja (le entra ruido aparte)
        { RUIDO_ALTO, 6000.0f,    0.0f,   60.0f },  // 38 hat cerrado
        { RUIDO_ALTO, 6000.0f,    0.0f,  420.0f },  // 39 hat abierto
        { CLAP,       1200.0f,    0.0f,  220.0f },  // 40 clap
        { TONO_CAIDA,  160.0f,   95.0f,  320.0f },  // 41 tom bajo
        { TONO_CAIDA,  220.0f,  130.0f,  300.0f },  // 42 tom medio
        { TONO_CAIDA,  300.0f,  185.0f,  280.0f },  // 43 tom alto
        { METAL,       800.0f, 1218.0f,   70.0f },  // 44 rim
        { METAL,       562.0f,  845.0f,  260.0f },  // 45 cencerro
        { TONO_CAIDA,  190.0f,  175.0f,  210.0f },  // 46 conga
        { RUIDO_BANDA, 8000.0f,   0.0f,  130.0f },  // 47 shaker
        { ZAP,        1200.0f,   60.0f,  160.0f },  // 48 zap
        { RUIDO_BANDA, 2500.0f,   0.0f,   40.0f },  // 49 snap
        { METAL,      3157.0f, 4731.0f, 1200.0f },  // 50 crash
        { METAL,      2437.0f, 3665.0f,  800.0f },  // 51 ride
    };
}

void PadsPlugin::initialise (const te::PluginInitialisationInfo& info)
{
    fs = info.sampleRate;
    for (auto& golpe : golpes) golpe.vivo = false;
}

void PadsPlugin::applyToBuffer (const te::PluginRenderContext& fc)
{
    if (fc.destBuffer == nullptr) return;
    auto io = abrirLimpio (fc);
    if (! isEnabled()) return;

    if (fc.bufferForMidiMessages != nullptr)
    {
        if (fc.bufferForMidiMessages->isAllNotesOff)
            for (auto& golpe : golpes) golpe.vivo = false;

        for (const auto& m : *fc.bufferForMidiMessages)
            if (m.isNoteOn())
            {
                const int pad = m.getNoteNumber() - PRIMER_PAD;
                if (pad >= 0 && pad < PADS)
                {
                    auto& golpe = golpes[pad];   // retrigger: el golpe nuevo corta al viejo
                    golpe.vivo = true;
                    golpe.pad = pad;
                    golpe.velocidad = 0.25f + 0.75f * m.getFloatVelocity();
                    golpe.fase = golpe.fase2 = 0.0;
                    golpe.envolvente = 1.0f;
                    golpe.muestras = 0;
                }
            }
    }

    const float nivel = dbAGanancia (P (0)) * 0.8f;
    const double afinar = std::pow (2.0, P (1) / 12.0);
    const float vDecaimiento = P (2);

    for (auto& golpe : golpes)
    {
        if (! golpe.vivo) continue;
        const auto& espec = ESPEC_PADS[golpe.pad];
        const float durMuestras = (float) (espec.durMs * 0.001f * vDecaimiento * fs);
        const float caida = std::exp (-4.0f / durMuestras);

        for (int i = 0; i < io.n; ++i)
        {
            golpe.envolvente *= caida;
            if (golpe.envolvente < 0.001f) { golpe.vivo = false; break; }

            const float avance = (float) golpe.muestras / durMuestras;   // 0..1 de la vida del golpe
            float v = 0.0f;

            switch (espec.familia)
            {
                case BOMBO:
                {
                    const double hz = (espec.f2 + (espec.f1 - espec.f2) * std::exp (-avance * 9.0f)) * afinar;
                    golpe.fase += 2.0 * juce::MathConstants<double>::pi * hz / fs;
                    v = (float) std::sin (golpe.fase);
                    if (golpe.muestras < (int) (0.003 * fs)) v += golpe.ruido.nextFloat() * 0.5f - 0.25f;
                    break;
                }
                case TONO_CAIDA:
                {
                    const double hz = (espec.f2 + (espec.f1 - espec.f2) * std::exp (-avance * 5.0f)) * afinar;
                    golpe.fase += 2.0 * juce::MathConstants<double>::pi * hz / fs;
                    v = (float) std::sin (golpe.fase) * 0.8f;
                    if (golpe.pad == 1)   // la caja lleva su nieve encima
                        v = v * 0.5f + (golpe.ruido.nextFloat() * 2.0f - 1.0f) * 0.6f;
                    break;
                }
                case RUIDO_ALTO:
                {
                    // Paso alto de un polo sobre ruido blanco: el hierro del hat.
                    const float blanco = golpe.ruido.nextFloat() * 2.0f - 1.0f;
                    golpe.fase2 += (blanco - golpe.fase2) * 0.25;
                    v = (blanco - (float) golpe.fase2) * 0.9f;
                    break;
                }
                case RUIDO_BANDA:
                {
                    const float blanco = golpe.ruido.nextFloat() * 2.0f - 1.0f;
                    golpe.fase2 += (blanco - golpe.fase2) * 0.5;
                    v = (float) golpe.fase2 * 1.2f;
                    break;
                }
                case CLAP:
                {
                    // Tres ráfagas separadas 12 ms y la cola: la palmada de grupo.
                    const int retrig = (int) (0.012 * fs);
                    const int cual = golpe.muestras / retrig;
                    const float ventana = cual < 3 ? 1.0f - (float) (golpe.muestras % retrig) / (float) retrig : 1.0f;
                    const float blanco = golpe.ruido.nextFloat() * 2.0f - 1.0f;
                    golpe.fase2 += (blanco - golpe.fase2) * 0.35;
                    v = (float) golpe.fase2 * ventana * 1.5f;
                    break;
                }
                case METAL:
                {
                    golpe.fase += 2.0 * juce::MathConstants<double>::pi * espec.f1 * afinar / fs;
                    golpe.fase2 += 2.0 * juce::MathConstants<double>::pi * espec.f2 * afinar / fs;
                    const float ruidillo = espec.durMs > 500.0f ? (golpe.ruido.nextFloat() * 2.0f - 1.0f) * 0.5f : 0.0f;
                    v = 0.45f * ((golpe.fase - std::floor (golpe.fase / (2.0 * juce::MathConstants<double>::pi)) * 2.0 * juce::MathConstants<double>::pi) > juce::MathConstants<double>::pi ? 1.0f : -1.0f)
                      + 0.35f * (std::sin (golpe.fase2) > 0 ? 1.0f : -1.0f) + ruidillo;
                    break;
                }
                case ZAP:
                {
                    const double hz = (espec.f2 + (espec.f1 - espec.f2) * std::exp (-avance * 14.0f)) * afinar;
                    golpe.fase += 2.0 * juce::MathConstants<double>::pi * hz / fs;
                    v = (float) std::sin (golpe.fase) * 1.1f;
                    break;
                }
            }

            const float salida = v * golpe.envolvente * golpe.velocidad * nivel;
            io.datos[0][i] += salida;
            if (io.datos[1] != nullptr) io.datos[1][i] += salida;
            ++golpe.muestras;
        }
    }
}

/* ============================================================== registro */

void registrarInstrumentos (te::Engine& engine)
{
    auto& plugins = engine.getPluginManager();
    plugins.createBuiltInType<BrumaPlugin>();
    plugins.createBuiltInType<CintaPlugin>();
    plugins.createBuiltInType<PadsPlugin>();
}
