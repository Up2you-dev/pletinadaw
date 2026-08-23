/*  Pletina DAW · motor
    Implementación del envoltorio del motor. Los "porqués" de cada modo de audio
    están en motor.h; aquí solo hay mecánica.
*/

#include "motor.h"
#include "protocolo.h"

#include <cmath>

namespace
{
    juce::var objeto()
    {
        return juce::var (new juce::DynamicObject());
    }

    void pon (juce::var& v, const juce::Identifier& clave, const juce::var& valor)
    {
        v.getDynamicObject()->setProperty (clave, valor);
    }
}

Motor::Motor (Opciones o, std::function<void (const juce::String&)> e)
    : opciones (o), emitir (std::move (e))
{
    if (opciones.sinAudio)
    {
        // La interfaz hospedada convierte al motor en un procesador al que
        // nosotros le empujamos bloques: no se abre ningún dispositivo real.
        auto& audioIO = engine.getDeviceManager().getHostedAudioDeviceInterface();

        te::HostedAudioDeviceInterface::Parameters parametros;
        parametros.sampleRate = opciones.frecuencia;
        parametros.blockSize = opciones.bloque;
        parametros.inputChannels = 0;
        parametros.outputChannels = 2;

        audioIO.initialise (parametros);
        audioIO.prepareToPlay (opciones.frecuencia, opciones.bloque);
        engine.getDeviceManager().dispatchPendingUpdates();

        arrancarBomba();
    }
    else
    {
        // Dispositivo por defecto del sistema: WASAPI o ASIO en Windows, ALSA en Linux.
        engine.getDeviceManager().initialise (0, 2);
    }

    startTimerHz (15);
}

Motor::~Motor()
{
    stopTimer();
    pararBomba();

    if (medidorMaestro != nullptr)
        medidorMaestro->measurer.removeClient (clienteMedidor);

    edit.reset();
}

//==============================================================================
juce::var Motor::hola() const
{
    auto r = objeto();
    pon (r, "nombre", "pletina-motor");
    pon (r, "version", "0.1.0");
    pon (r, "motor", "tracktion-engine");
    pon (r, "audio", opciones.sinAudio ? "sin-audio" : "dispositivo");

    juce::Array<juce::var> capacidades { "transporte", "edit.cargarAudio", "medidores" };
    pon (r, "capacidades", capacidades);
    return r;
}

juce::var Motor::listarDispositivos()
{
    auto r = objeto();
    juce::Array<juce::var> lista;

    if (opciones.sinAudio)
    {
        pon (r, "actual", "dispositivo nulo (bomba interna)");
        pon (r, "tipo", "virtual");
    }
    else
    {
        auto& dm = engine.getDeviceManager().deviceManager;

        if (auto* actual = dm.getCurrentAudioDevice())
        {
            pon (r, "actual", actual->getName());
            pon (r, "tipo", actual->getTypeName());
            pon (r, "frecuencia", actual->getCurrentSampleRate());
            pon (r, "bloque", actual->getCurrentBufferSizeSamples());
        }
        else
        {
            pon (r, "actual", "");
            pon (r, "tipo", "");
        }

        for (auto* tipo : dm.getAvailableDeviceTypes())
        {
            tipo->scanForDevices();

            for (const auto& nombre : tipo->getDeviceNames())
            {
                auto d = objeto();
                pon (d, "tipo", tipo->getTypeName());
                pon (d, "nombre", nombre);
                lista.add (d);
            }
        }
    }

    pon (r, "dispositivos", lista);
    return r;
}

void Motor::asegurarEdit()
{
    if (edit != nullptr)
        return;

    auto carpeta = engine.getTemporaryFileManager().getTempDirectory();
    edit = te::createEmptyEdit (engine, carpeta.getChildFile ("esqueleto.tracktionedit"));
    edit->ensureNumberOfAudioTracks (4);

    // Un medidor colgado del máster: la UI pinta sus VU con lo que diga.
    auto enchufado = edit->getMasterPluginList().insertPlugin (te::LevelMeterPlugin::create(), 0);

    if (auto* medidor = dynamic_cast<te::LevelMeterPlugin*> (enchufado.get()))
    {
        medidorMaestro = medidor;
        medidorMaestro->measurer.addClient (clienteMedidor);
    }
}

juce::var Motor::nuevoEdit()
{
    edit.reset();
    medidorMaestro = nullptr;
    asegurarEdit();

    auto r = objeto();
    pon (r, "pistas", 4);
    return r;
}

juce::var Motor::cargarAudio (const juce::String& ruta)
{
    asegurarEdit();

    const juce::File archivo (ruta);

    if (! archivo.existsAsFile())
        throw std::runtime_error ("no existe el archivo: " + ruta.toStdString());

    te::AudioFile audio (engine, archivo);

    if (! audio.isValid())
        throw std::runtime_error ("formato de audio no reconocido: " + ruta.toStdString());

    auto pistas = te::getAudioTracks (*edit);

    if (pistas.isEmpty())
        throw std::runtime_error ("el edit no tiene pistas de audio");

    auto clip = pistas[0]->insertWaveClip (archivo.getFileNameWithoutExtension(), archivo,
                                           { { {}, te::TimeDuration::fromSeconds (audio.getLength()) }, {} },
                                           false);

    if (clip == nullptr)
        throw std::runtime_error ("no se pudo crear el clip");

    auto r = objeto();
    pon (r, "duracion", audio.getLength());
    pon (r, "frecuencia", audio.getSampleRate());
    pon (r, "canales", (int) audio.getNumChannels());
    return r;
}

juce::var Motor::tocar()
{
    asegurarEdit();

    auto& transporte = edit->getTransport();
    transporte.ensureContextAllocated();
    transporte.play (false);

    return estadoTransporte();
}

juce::var Motor::parar()
{
    asegurarEdit();
    edit->getTransport().stop (false, false);
    return estadoTransporte();
}

juce::var Motor::irA (double segundos)
{
    asegurarEdit();
    edit->getTransport().setPosition (te::TimePosition::fromSeconds (segundos));
    return estadoTransporte();
}

juce::var Motor::estadoTransporte() const
{
    auto r = objeto();

    if (edit != nullptr)
    {
        auto& transporte = edit->getTransport();
        pon (r, "reproduciendo", transporte.isPlaying());
        pon (r, "segundos", transporte.getPosition().inSeconds());
    }
    else
    {
        pon (r, "reproduciendo", false);
        pon (r, "segundos", 0.0);
    }

    return r;
}

//==============================================================================
void Motor::timerCallback()
{
    // Un solo evento con posición y niveles: menos líneas por segundo en el
    // canal y la UI pinta todo de una vez.
    auto datos = estadoTransporte();

    float izq = picoIzq.exchange (0.0f);
    float der = picoDer.exchange (0.0f);
    auto izqDb = juce::Decibels::gainToDecibels (izq, -100.0f);
    auto derDb = juce::Decibels::gainToDecibels (der, -100.0f);

    if (medidorMaestro != nullptr)
    {
        // El medidor del grafo manda si está vivo; la bomba es la red de seguridad.
        izqDb = juce::jmax (izqDb, clienteMedidor.getAndClearAudioLevel (0).dB);
        derDb = juce::jmax (derDb, clienteMedidor.getAndClearAudioLevel (1).dB);
    }

    pon (datos, "izq", izqDb);
    pon (datos, "der", derDb);

    const bool reproduciendo = datos["reproduciendo"];

    if (reproduciendo || reproduciendoAntes)
        emitir (protocolo::evento ("medidores", datos));

    reproduciendoAntes = reproduciendo;
}

//==============================================================================
void Motor::arrancarBomba()
{
    bombaViva = true;

    bomba = std::thread ([this]
    {
        // Este hilo hace de tarjeta de sonido: pide bloques al motor al ritmo
        // que marcaría el hardware y apunta el pico de cada canal.
        auto& audioIO = engine.getDeviceManager().getHostedAudioDeviceInterface();

        juce::AudioBuffer<float> buffer (2, opciones.bloque);
        juce::MidiBuffer midi;

        const auto periodo = std::chrono::nanoseconds (
            (long long) (1.0e9 * opciones.bloque / opciones.frecuencia));
        auto siguiente = std::chrono::steady_clock::now();

        while (bombaViva.load())
        {
            buffer.clear();
            midi.clear();
            audioIO.processBlock (buffer, midi);

            const auto n = buffer.getNumSamples();
            picoIzq.store (juce::jmax (picoIzq.load(), buffer.getMagnitude (0, 0, n)));
            picoDer.store (juce::jmax (picoDer.load(), buffer.getMagnitude (buffer.getNumChannels() > 1 ? 1 : 0, 0, n)));

            siguiente += periodo;
            std::this_thread::sleep_until (siguiente);
        }
    });
}

void Motor::pararBomba()
{
    if (bomba.joinable())
    {
        bombaViva = false;
        bomba.join();
    }
}

//==============================================================================
int Motor::autoprueba()
{
    // Un segundo de seno a 440 Hz y -6 dB, escrito a un WAV temporal.
    auto carpeta = engine.getTemporaryFileManager().getTempDirectory();
    auto wav = carpeta.getChildFile ("autoprueba.wav");
    wav.deleteFile();

    {
        juce::WavAudioFormat formato;
        auto flujo = wav.createOutputStream();

        if (flujo == nullptr)
            return 1;

        std::unique_ptr<juce::AudioFormatWriter> escritor (
            formato.createWriterFor (flujo.release(), 44100.0, 2, 16, {}, 0));

        if (escritor == nullptr)
            return 1;

        juce::AudioBuffer<float> b (2, 44100);

        for (int i = 0; i < 44100; ++i)
        {
            auto v = 0.5f * (float) std::sin (2.0 * juce::MathConstants<double>::pi * 440.0 * i / 44100.0);
            b.setSample (0, i, v);
            b.setSample (1, i, v);
        }

        escritor->writeFromAudioSampleBuffer (b, 0, 44100);
    }

    // Entre orden y orden se deja respirar al bucle de mensajes, igual que
    // pasa en una sesión real por stdio: el grafo se construye con mensajes
    // asíncronos y encadenarlo todo en seco lo deja a medio montar.
    nuevoEdit();
    juce::MessageManager::getInstance()->runDispatchLoopUntil (100);
    cargarAudio (wav.getFullPathName());
    juce::MessageManager::getInstance()->runDispatchLoopUntil (200);
    tocar();

    // Casi un segundo de reloj real: la bomba empuja bloques mientras tanto.
    juce::MessageManager::getInstance()->runDispatchLoopUntil (800);

    const auto segundos = edit->getTransport().getPosition().inSeconds();
    const auto pico = juce::jmax (picoIzq.load(), picoDer.load());
    parar();

    const bool avanza = segundos > 0.25;
    const bool suena = pico > 0.05f;

    auto r = objeto();
    pon (r, "ok", avanza && suena);
    pon (r, "segundos", segundos);
    pon (r, "pico", pico);
    emitir (protocolo::evento ("prueba", r));

    return (avanza && suena) ? 0 : 1;
}
