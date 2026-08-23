/*  Pletina DAW · motor
    Copyright (C) 2026 Up2you — GPLv3: ver el LICENSE de la raíz.

    Implementación del envoltorio del motor. Los "porqués" de cada modo de
    audio están en motor.h; aquí solo hay mecánica.
*/

#include "motor.h"
#include "efectos.h"
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

    juce::AudioFormatManager& formatos()
    {
        static juce::AudioFormatManager fm;
        static bool listo = false;
        if (! listo) { fm.registerBasicFormats(); listo = true; }
        return fm;
    }

    const char* TIPOS_SUITE[] = { UtilidadPlugin::xmlTypeName, CompresorPlugin::xmlTypeName,
                                  TechoPlugin::xmlTypeName, EQOchoPlugin::xmlTypeName,
                                  MedidorPlugin::xmlTypeName, PlacaPlugin::xmlTypeName,
                                  DelayPlugin::xmlTypeName, PuertaPlugin::xmlTypeName };
}

Motor::Motor (Opciones o, std::function<void (const juce::String&)> e)
    : opciones (o), emitir (std::move (e))
{
    registrarEfectos (engine);

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
        medidorMaestro->measurer.removeClient (clienteMaestro);
    for (size_t i = 0; i < medidoresPista.size(); ++i)
        medidoresPista[i]->measurer.removeClient (*clientesPista[i]);

    edit.reset();
}

//==============================================================================
juce::var Motor::hola() const
{
    auto r = objeto();
    pon (r, "nombre", "pletina-motor");
    pon (r, "version", "0.2.0");
    pon (r, "motor", "tracktion-engine");
    pon (r, "audio", opciones.sinAudio ? "sin-audio" : "dispositivo");

    juce::Array<juce::var> capacidades { "transporte", "proyecto", "clips", "picos",
                                         "mezcla", "suite", "deshacer", "render" };
    pon (r, "capacidades", capacidades);

    juce::Array<juce::var> suite;
    for (auto tipo : TIPOS_SUITE) suite.add (tipo);
    pon (r, "suite", suite);
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

/* ============================================================== proyecto */

void Motor::adoptarEdit (std::unique_ptr<te::Edit> nuevo, const juce::File& carpeta)
{
    if (medidorMaestro != nullptr)
    {
        medidorMaestro->measurer.removeClient (clienteMaestro);
        medidorMaestro = nullptr;
    }
    for (size_t i = 0; i < medidoresPista.size(); ++i)
        medidoresPista[i]->measurer.removeClient (*clientesPista[i]);
    medidoresPista.clear();
    clientesPista.clear();

    edit = std::move (nuevo);
    carpetaProyecto = carpeta;

    if (edit == nullptr)
        return;

    // Un medidor al FINAL de la cadena del máster: VU después del fader.
    auto& maestra = edit->getMasterPluginList();
    bool hayMedidor = false;
    for (auto p : maestra.getPlugins())
        if (dynamic_cast<te::LevelMeterPlugin*> (p) != nullptr)
            { medidorMaestro = dynamic_cast<te::LevelMeterPlugin*> (p); hayMedidor = true; }

    if (! hayMedidor)
    {
        auto enchufado = maestra.insertPlugin (te::LevelMeterPlugin::create(), maestra.size());
        medidorMaestro = dynamic_cast<te::LevelMeterPlugin*> (enchufado.get());
    }

    if (medidorMaestro != nullptr)
        medidorMaestro->measurer.addClient (clienteMaestro);

    refrescarMedidoresDePista();
}

void Motor::asegurarEdit()
{
    if (edit != nullptr)
        return;

    auto carpeta = engine.getTemporaryFileManager().getTempDirectory();
    auto nuevo = te::createEmptyEdit (engine, carpeta.getChildFile ("esqueleto.tracktionedit"));
    nuevo->ensureNumberOfAudioTracks (4);
    adoptarEdit (std::move (nuevo), {});
}

juce::var Motor::nuevoProyecto (const juce::String& carpeta)
{
    const juce::File dir (carpeta);

    if (carpeta.trim().isEmpty())
        throw std::runtime_error ("falta la carpeta del proyecto");
    if (! dir.createDirectory())
        throw std::runtime_error ("no se pudo crear la carpeta: " + carpeta.toStdString());

    carpetaMedia(); // se crea junto al proyecto
    dir.getChildFile ("media").createDirectory();

    auto nuevo = te::createEmptyEdit (engine, dir.getChildFile ("proyecto.tracktionedit"));
    nuevo->ensureNumberOfAudioTracks (4);
    adoptarEdit (std::move (nuevo), dir);

    te::EditFileOperations (*edit).save (false, true, false);
    emitirModelo();
    return listarPistas();
}

juce::var Motor::abrirProyecto (const juce::String& ruta)
{
    juce::File archivo (ruta);
    if (archivo.isDirectory())
        archivo = archivo.getChildFile ("proyecto.tracktionedit");

    if (! archivo.existsAsFile())
        throw std::runtime_error ("no existe el proyecto: " + ruta.toStdString());

    auto cargado = te::loadEditFromFile (engine, archivo);
    if (cargado == nullptr)
        throw std::runtime_error ("no se pudo leer el proyecto");

    adoptarEdit (std::move (cargado), archivo.getParentDirectory());
    emitirModelo();
    return listarPistas();
}

juce::var Motor::guardarProyecto()
{
    asegurarEdit();

    if (carpetaProyecto == juce::File())
        throw std::runtime_error ("el proyecto es temporal: crea uno con proyecto.nuevo");

    if (! te::EditFileOperations (*edit).save (false, true, false))
        throw std::runtime_error ("no se pudo guardar");

    emitirModelo();
    auto r = objeto();
    pon (r, "ruta", carpetaProyecto.getFullPathName());
    return r;
}

juce::File Motor::carpetaMedia() const
{
    if (carpetaProyecto == juce::File())
        return engine.getTemporaryFileManager().getTempDirectory();
    return carpetaProyecto.getChildFile ("media");
}

/* ================================================================ modelo */

juce::var Motor::listarPistas()
{
    asegurarEdit();

    auto r = objeto();

    auto proyecto = objeto();
    pon (proyecto, "ruta", carpetaProyecto == juce::File() ? "" : carpetaProyecto.getFullPathName());
    pon (proyecto, "nombre", carpetaProyecto == juce::File() ? "sin guardar" : carpetaProyecto.getFileName());
    pon (proyecto, "modificado", edit->hasChangedSinceSaved());
    pon (r, "proyecto", proyecto);

    pon (r, "bpm", edit->tempoSequence.getTempo (0)->getBpm());
    pon (r, "metronomo", (bool) edit->clickTrackEnabled);

    auto& transporte = edit->getTransport();
    auto bucleVar = objeto();
    const auto rango = transporte.getLoopRange();
    pon (bucleVar, "activo", (bool) transporte.looping);
    pon (bucleVar, "inicio", rango.getStart().inSeconds());
    pon (bucleVar, "fin", rango.getEnd().inSeconds());
    pon (r, "bucle", bucleVar);

    auto describirCadena = [] (const juce::Array<te::Plugin*>& plugins)
    {
        juce::Array<juce::var> lista;
        int i = 0;
        for (auto p : plugins)
        {
            auto d = objeto();
            pon (d, "indice", i++);
            pon (d, "tipo", p->getPluginType());
            pon (d, "nombre", p->getName());
            pon (d, "activo", p->isEnabled());
            pon (d, "parametros", describirParametros (*p));
            lista.add (d);
        }
        return juce::var (lista);
    };

    juce::Array<juce::var> pistas;
    int indice = 0;

    for (auto pista : te::getAudioTracks (*edit))
    {
        auto p = objeto();
        pon (p, "indice", indice++);
        pon (p, "id", pista->itemID.toString());
        pon (p, "nombre", pista->getName());
        pon (p, "mute", pista->isMuted (false));
        pon (p, "solo", pista->isSolo (false));

        if (auto* volumen = pista->getVolumePlugin())
        {
            pon (p, "volumenDb", volumen->getVolumeDb());
            pon (p, "pan", volumen->getPan());
        }

        juce::Array<juce::var> clips;
        for (auto c : pista->getClips())
        {
            auto d = objeto();
            const auto posicion = c->getPosition();
            pon (d, "id", c->itemID.toString());
            pon (d, "nombre", c->getName());
            pon (d, "inicio", posicion.getStart().inSeconds());
            pon (d, "duracion", posicion.getLength().inSeconds());
            pon (d, "desfase", posicion.getOffset().inSeconds());

            if (auto* onda = dynamic_cast<te::WaveAudioClip*> (c))
            {
                pon (d, "ruta", onda->getCurrentSourceFile().getFullPathName());
                pon (d, "duracionFuente", onda->getAudioFile().getLength());
                pon (d, "entradaFundido", onda->getFadeIn().inSeconds());
                pon (d, "salidaFundido", onda->getFadeOut().inSeconds());
            }

            clips.add (d);
        }
        pon (p, "clips", clips);
        pon (p, "plugins", describirCadena (cadenaUsuario (indice - 1)));

        pistas.add (p);
    }
    pon (r, "pistas", pistas);

    auto maestro = objeto();
    if (auto volumen = edit->getMasterVolumePlugin())
    {
        pon (maestro, "volumenDb", volumen->getVolumeDb());
        pon (maestro, "pan", volumen->getPan());
    }
    pon (maestro, "plugins", describirCadena (cadenaUsuario (-1)));
    pon (r, "master", maestro);

    return r;
}

void Motor::emitirModelo()
{
    refrescarMedidoresDePista();
    emitir (protocolo::evento ("modelo", listarPistas()));
}

/* ================================================================ pistas */

te::AudioTrack* Motor::pista (int indice) const
{
    if (edit == nullptr)
        return nullptr;
    auto pistas = te::getAudioTracks (*edit);
    return indice >= 0 && indice < pistas.size() ? pistas[indice] : nullptr;
}

juce::var Motor::crearPista()
{
    asegurarEdit();
    edit->getUndoManager().beginNewTransaction ("crear pista");

    auto pistas = te::getAudioTracks (*edit);
    edit->insertNewAudioTrack (te::TrackInsertPoint (nullptr, pistas.isEmpty() ? nullptr : pistas.getLast()), nullptr);

    emitirModelo();
    return listarPistas();
}

juce::var Motor::borrarPista (int indice)
{
    asegurarEdit();
    auto* objetivo = pista (indice);
    if (objetivo == nullptr)
        throw std::runtime_error ("no existe la pista");

    edit->getUndoManager().beginNewTransaction ("borrar pista");
    edit->deleteTrack (objetivo);
    emitirModelo();
    return listarPistas();
}

juce::var Motor::renombrarPista (int indice, const juce::String& nombre)
{
    asegurarEdit();
    auto* objetivo = pista (indice);
    if (objetivo == nullptr)
        throw std::runtime_error ("no existe la pista");

    edit->getUndoManager().beginNewTransaction ("renombrar pista");
    objetivo->setName (nombre.trim().isEmpty() ? juce::String ("Pista") : nombre);
    emitirModelo();
    return listarPistas();
}

juce::var Motor::mezclaPista (int indice, const juce::var& params)
{
    asegurarEdit();

    if (indice == -1)
    {
        if (auto volumen = edit->getMasterVolumePlugin())
        {
            if (params.hasProperty ("volumenDb")) volumen->setVolumeDb ((float) (double) params["volumenDb"]);
            if (params.hasProperty ("pan")) volumen->setPan ((float) (double) params["pan"]);
        }
    }
    else
    {
        auto* objetivo = pista (indice);
        if (objetivo == nullptr)
            throw std::runtime_error ("no existe la pista");

        if (auto* volumen = objetivo->getVolumePlugin())
        {
            if (params.hasProperty ("volumenDb")) volumen->setVolumeDb ((float) (double) params["volumenDb"]);
            if (params.hasProperty ("pan")) volumen->setPan ((float) (double) params["pan"]);
        }
        if (params.hasProperty ("mute")) objetivo->setMute ((bool) params["mute"]);
        if (params.hasProperty ("solo")) objetivo->setSolo ((bool) params["solo"]);
    }

    // La mezcla cambia decenas de veces al arrastrar un fader: sin evento
    // modelo por cada pasito, que la interfaz ya sabe lo que ha pedido.
    return juce::var (true);
}

/* ================================================================= clips */

te::Clip* Motor::clip (const juce::String& id) const
{
    if (edit == nullptr)
        return nullptr;
    return te::findClipForID (*edit, te::EditItemID::fromString (id));
}

juce::var Motor::importarClip (int indicePista, const juce::String& ruta, double inicio)
{
    asegurarEdit();

    auto* objetivo = pista (indicePista);
    if (objetivo == nullptr)
        throw std::runtime_error ("no existe la pista");

    const juce::File origen (ruta);
    if (! origen.existsAsFile())
        throw std::runtime_error ("no existe el archivo: " + ruta.toStdString());

    // El audio se queda en media/ del proyecto: la carpeta ES el proyecto.
    juce::File destino = origen;
    const auto media = carpetaMedia();
    if (carpetaProyecto != juce::File() && ! origen.isAChildOf (carpetaProyecto))
    {
        media.createDirectory();
        destino = media.getNonexistentChildFile (origen.getFileNameWithoutExtension(),
                                                 origen.getFileExtension(), false);
        if (! origen.copyFileTo (destino))
            throw std::runtime_error ("no se pudo copiar a media/");
    }

    te::AudioFile audio (engine, destino);
    if (! audio.isValid())
        throw std::runtime_error ("formato de audio no reconocido: " + ruta.toStdString());

    edit->getUndoManager().beginNewTransaction ("importar audio");

    auto nuevo = objetivo->insertWaveClip (destino.getFileNameWithoutExtension(), destino,
                                           { { te::TimePosition::fromSeconds (juce::jmax (0.0, inicio)),
                                               te::TimePosition::fromSeconds (juce::jmax (0.0, inicio) + audio.getLength()) }, {} },
                                           false);
    if (nuevo == nullptr)
        throw std::runtime_error ("no se pudo crear el clip");

    emitirModelo();

    auto r = objeto();
    pon (r, "id", nuevo->itemID.toString());
    pon (r, "duracion", audio.getLength());
    return r;
}

juce::var Motor::moverClip (const juce::String& id, double inicio, int indicePista)
{
    auto* objetivo = clip (id);
    if (objetivo == nullptr)
        throw std::runtime_error ("no existe el clip");

    edit->getUndoManager().beginNewTransaction ("mover clip");

    if (indicePista >= 0)
        if (auto* destino = pista (indicePista))
            if (destino != objetivo->getClipTrack())
                objetivo->moveTo (*destino);

    objetivo->setStart (te::TimePosition::fromSeconds (juce::jmax (0.0, inicio)), false, true);
    emitirModelo();
    return juce::var (true);
}

juce::var Motor::recortarClip (const juce::String& id, double inicio, double fin)
{
    auto* objetivo = clip (id);
    if (objetivo == nullptr)
        throw std::runtime_error ("no existe el clip");
    if (fin - inicio < 0.01)
        throw std::runtime_error ("el clip quedaría vacío");

    edit->getUndoManager().beginNewTransaction ("recortar clip");
    // preserveSync = true: el material no se mueve del tiempo, solo cambia la ventana.
    objetivo->setStart (te::TimePosition::fromSeconds (juce::jmax (0.0, inicio)), true, false);
    objetivo->setEnd (te::TimePosition::fromSeconds (fin), true);
    emitirModelo();
    return juce::var (true);
}

juce::var Motor::dividirClip (const juce::String& id, double segundos)
{
    auto* objetivo = clip (id);
    if (objetivo == nullptr)
        throw std::runtime_error ("no existe el clip");

    auto* dueno = objetivo->getClipTrack();
    if (dueno == nullptr)
        throw std::runtime_error ("el clip no está en una pista");

    edit->getUndoManager().beginNewTransaction ("dividir clip");
    auto* nuevo = dueno->splitClip (*objetivo, te::TimePosition::fromSeconds (segundos));
    if (nuevo == nullptr)
        throw std::runtime_error ("ahí no se puede dividir");

    emitirModelo();
    auto r = objeto();
    pon (r, "id", nuevo->itemID.toString());
    return r;
}

juce::var Motor::duplicarClip (const juce::String& id)
{
    auto* objetivo = dynamic_cast<te::WaveAudioClip*> (clip (id));
    if (objetivo == nullptr)
        throw std::runtime_error ("no existe el clip de audio");

    auto* dueno = dynamic_cast<te::AudioTrack*> (objetivo->getClipTrack());
    if (dueno == nullptr)
        throw std::runtime_error ("el clip no está en una pista");

    edit->getUndoManager().beginNewTransaction ("duplicar clip");

    // El duplicado nace pegado detrás del original, con su misma ventana.
    const auto posicion = objetivo->getPosition();
    auto nuevo = dueno->insertWaveClip (objetivo->getName(), objetivo->getCurrentSourceFile(),
                                        { { posicion.getEnd(), posicion.getEnd() + posicion.getLength() },
                                          posicion.getOffset() },
                                        false);
    if (nuevo == nullptr)
        throw std::runtime_error ("no se pudo duplicar");

    nuevo->setFadeIn (objetivo->getFadeIn());
    nuevo->setFadeOut (objetivo->getFadeOut());

    emitirModelo();
    auto r = objeto();
    pon (r, "id", nuevo->itemID.toString());
    return r;
}

juce::var Motor::fundidosClip (const juce::String& id, double entrada, double salida)
{
    auto* objetivo = dynamic_cast<te::WaveAudioClip*> (clip (id));
    if (objetivo == nullptr)
        throw std::runtime_error ("no existe el clip de audio");

    edit->getUndoManager().beginNewTransaction ("fundidos");

    const auto duracion = objetivo->getPosition().getLength().inSeconds();
    objetivo->setFadeIn (te::TimeDuration::fromSeconds (juce::jlimit (0.0, duracion, entrada)));
    objetivo->setFadeOut (te::TimeDuration::fromSeconds (juce::jlimit (0.0, duracion, salida)));

    emitirModelo();
    return juce::var (true);
}

juce::var Motor::borrarClip (const juce::String& id)
{
    auto* objetivo = clip (id);
    if (objetivo == nullptr)
        throw std::runtime_error ("no existe el clip");

    edit->getUndoManager().beginNewTransaction ("borrar clip");
    objetivo->removeFromParent();
    emitirModelo();
    return juce::var (true);
}

juce::var Motor::picosClip (const juce::String& id, int porSegundo)
{
    auto* objetivo = dynamic_cast<te::WaveAudioClip*> (clip (id));
    if (objetivo == nullptr)
        throw std::runtime_error ("no existe el clip de audio");

    const auto archivo = objetivo->getCurrentSourceFile();
    std::unique_ptr<juce::AudioFormatReader> lector (formatos().createReaderFor (archivo));
    if (lector == nullptr)
        throw std::runtime_error ("no se pudo leer la fuente");

    porSegundo = juce::jlimit (8, 256, porSegundo);
    const double duracion = (double) lector->lengthInSamples / lector->sampleRate;
    int cubos = (int) std::ceil (duracion * porSegundo);
    cubos = juce::jlimit (1, 16384, cubos);
    const double muestrasPorCubo = (double) lector->lengthInSamples / cubos;

    // Se lee por trozos y de cada cubo queda su pico: suficiente para pintar.
    juce::Array<juce::var> picos;
    picos.ensureStorageAllocated (cubos);

    const int TROZO = 65536;
    juce::AudioBuffer<float> buffer ((int) lector->numChannels, TROZO);
    juce::int64 leidas = 0;
    int cubo = 0;
    float pico = 0.0f;

    while (leidas < lector->lengthInSamples && cubo < cubos)
    {
        const int n = (int) juce::jmin ((juce::int64) TROZO, lector->lengthInSamples - leidas);
        lector->read (&buffer, 0, n, leidas, true, true);

        for (int i = 0; i < n; ++i)
        {
            for (int c = 0; c < (int) lector->numChannels; ++c)
                pico = juce::jmax (pico, std::abs (buffer.getSample (c, i)));

            if ((double) (leidas + i + 1) >= muestrasPorCubo * (cubo + 1))
            {
                picos.add ((int) std::lround (juce::jlimit (0.0f, 1.0f, pico) * 100.0f));
                pico = 0.0f;
                cubo += 1;
                if (cubo >= cubos) break;
            }
        }
        leidas += n;
    }
    while (cubo++ < cubos)
        picos.add (0);

    auto r = objeto();
    pon (r, "id", id);
    pon (r, "porSegundo", porSegundo);
    pon (r, "duracionFuente", duracion);
    pon (r, "picos", picos);
    return r;
}

/* ================================================================ cadenas */

te::PluginList* Motor::cadena (int indice) const
{
    if (edit == nullptr)
        return nullptr;
    if (indice == -1)
        return &edit->getMasterPluginList();
    auto* p = pista (indice);
    return p != nullptr ? &p->pluginList : nullptr;
}

juce::Array<te::Plugin*> Motor::cadenaUsuario (int indice) const
{
    juce::Array<te::Plugin*> usuario;
    if (auto* lista = cadena (indice))
        for (auto p : lista->getPlugins())
            if (! esPluginDeSerie (*p))
                usuario.add (p);
    return usuario;
}

juce::var Motor::insertarPlugin (int indicePista, const juce::String& tipo, int indice)
{
    asegurarEdit();

    bool conocido = false;
    for (auto t : TIPOS_SUITE) conocido = conocido || tipo == t;
    if (! conocido)
        throw std::runtime_error ("tipo de la suite desconocido: " + tipo.toStdString());

    auto* lista = cadena (indicePista);
    if (lista == nullptr)
        throw std::runtime_error ("no existe la pista");

    edit->getUndoManager().beginNewTransaction ("insertar " + tipo);

    auto nuevo = edit->getPluginCache().createNewPlugin (tipo, {});
    if (nuevo == nullptr)
        throw std::runtime_error ("no se pudo crear el plugin");

    // La cadena del usuario vive delante de los de serie (pre-fader): el
    // índice filtrado se traduce a la posición cruda correspondiente.
    auto usuario = cadenaUsuario (indicePista);
    int crudo;
    if (usuario.isEmpty())
        crudo = 0;
    else if (indice >= 0 && indice < usuario.size())
        crudo = lista->getPlugins().indexOf (usuario[indice]);
    else
        crudo = lista->getPlugins().indexOf (usuario.getLast()) + 1;

    lista->insertPlugin (nuevo, crudo, nullptr);

    // insertPlugin calla si un límite lo rechaza: aquí se comprueba de verdad.
    if (! lista->getPlugins().contains (nuevo.get()))
        throw std::runtime_error ("el motor ha rechazado la inserción");

    emitirModelo();
    return listarPistas();
}

juce::var Motor::quitarPlugin (int indicePista, int indice)
{
    auto usuario = cadenaUsuario (indicePista);
    if (indice < 0 || indice >= usuario.size())
        throw std::runtime_error ("no existe el plugin");

    edit->getUndoManager().beginNewTransaction ("quitar plugin");
    usuario[indice]->deleteFromParent();
    emitirModelo();
    return listarPistas();
}

juce::var Motor::parametroPlugin (int indicePista, int indice, const juce::String& parametro, double valor)
{
    auto usuario = cadenaUsuario (indicePista);
    if (indice < 0 || indice >= usuario.size())
        throw std::runtime_error ("no existe el plugin");

    for (auto p : usuario[indice]->getAutomatableParameters())
    {
        if (p->paramID == parametro)
        {
            p->setParameter ((float) valor, juce::sendNotificationSync);
            return juce::var (true);
        }
    }

    throw std::runtime_error ("no existe el parámetro: " + parametro.toStdString());
}

juce::var Motor::activarPlugin (int indicePista, int indice, bool activo)
{
    auto usuario = cadenaUsuario (indicePista);
    if (indice < 0 || indice >= usuario.size())
        throw std::runtime_error ("no existe el plugin");

    usuario[indice]->setEnabled (activo);
    emitirModelo();
    return juce::var (true);
}

/* ============================================================ transporte */

juce::var Motor::tocar()
{
    asegurarEdit();

    // La sonoridad integrada mide desde que arranca la reproducción.
    for (auto p : cadenaUsuario (-1))
        if (auto* medidor = dynamic_cast<MedidorPlugin*> (p))
            medidor->reiniciar();

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

juce::var Motor::tempo (double bpm)
{
    asegurarEdit();
    edit->getUndoManager().beginNewTransaction ("cambiar tempo");
    edit->tempoSequence.getTempo (0)->setBpm (juce::jlimit (20.0, 999.0, bpm));
    emitirModelo();
    return juce::var (edit->tempoSequence.getTempo (0)->getBpm());
}

juce::var Motor::metronomo (bool activo)
{
    asegurarEdit();
    edit->clickTrackEnabled = activo;
    emitirModelo();
    return juce::var (activo);
}

juce::var Motor::bucle (bool activo, double inicio, double fin)
{
    asegurarEdit();
    auto& transporte = edit->getTransport();
    if (fin > inicio)
        transporte.setLoopRange ({ te::TimePosition::fromSeconds (inicio), te::TimePosition::fromSeconds (fin) });
    transporte.looping = activo;
    emitirModelo();
    return juce::var (true);
}

juce::var Motor::deshacer()
{
    asegurarEdit();
    edit->undo();
    emitirModelo();
    return listarPistas();
}

juce::var Motor::rehacer()
{
    asegurarEdit();
    edit->redo();
    emitirModelo();
    return listarPistas();
}

/* ================================================================ render */

juce::var Motor::exportar (const juce::String& ruta)
{
    asegurarEdit();

    if (edit->getLength().inSeconds() <= 0.0)
        throw std::runtime_error ("el proyecto está vacío: nada que exportar");

    const juce::File destino (ruta);
    if (destino.getFileExtension().toLowerCase() != ".wav")
        throw std::runtime_error ("de momento la exportación es a WAV");

    parar();
    destino.deleteFile();

    // Render bloqueante en el hilo de mensajes: para lo que dura una canción,
    // más simple y más robusto que otra maquinaria de hilos. La interfaz ya
    // avisa de que está exportando.
    const bool ok = te::Renderer::renderToFile (*edit, destino, false);

    auto r = objeto();
    pon (r, "ruta", destino.getFullPathName());
    pon (r, "ok", ok && destino.existsAsFile());
    emitir (protocolo::evento ("render.terminado", r));

    if (! (ok && destino.existsAsFile()))
        throw std::runtime_error ("el render ha fallado");

    return r;
}

/* ============================================================= medidores */

void Motor::refrescarMedidoresDePista()
{
    if (edit == nullptr)
        return;

    auto pistas = te::getAudioTracks (*edit);

    std::vector<te::LevelMeterPlugin*> actuales;
    for (auto p : pistas)
        actuales.push_back (p->getLevelMeterPlugin());

    if (actuales == medidoresPista)
        return;

    for (size_t i = 0; i < medidoresPista.size(); ++i)
        if (medidoresPista[i] != nullptr)
            medidoresPista[i]->measurer.removeClient (*clientesPista[i]);

    medidoresPista = actuales;
    clientesPista.clear();

    for (auto* medidor : medidoresPista)
    {
        auto cliente = std::make_unique<te::LevelMeasurer::Client>();
        if (medidor != nullptr)
            medidor->measurer.addClient (*cliente);
        clientesPista.push_back (std::move (cliente));
    }
}

void Motor::timerCallback()
{
    auto datos = estadoTransporte();

    float izq = picoIzq.exchange (0.0f);
    float der = picoDer.exchange (0.0f);
    auto izqDb = juce::Decibels::gainToDecibels (izq, -100.0f);
    auto derDb = juce::Decibels::gainToDecibels (der, -100.0f);

    if (medidorMaestro != nullptr)
    {
        // El medidor del grafo manda si está vivo; la bomba es la red de seguridad.
        izqDb = juce::jmax (izqDb, clienteMaestro.getAndClearAudioLevel (0).dB);
        derDb = juce::jmax (derDb, clienteMaestro.getAndClearAudioLevel (1).dB);
    }

    pon (datos, "izq", izqDb);
    pon (datos, "der", derDb);

    juce::Array<juce::var> porPista;
    for (auto& cliente : clientesPista)
    {
        auto p = objeto();
        pon (p, "izq", cliente->getAndClearAudioLevel (0).dB);
        pon (p, "der", cliente->getAndClearAudioLevel (1).dB);
        porPista.add (p);
    }
    pon (datos, "pistas", porPista);

    if (edit != nullptr)
        for (auto p : cadenaUsuario (-1))
            if (auto* medidor = dynamic_cast<MedidorPlugin*> (p))
            {
                const auto lectura = medidor->leer();
                auto l = objeto();
                pon (l, "pico", lectura.picoDb);
                pon (l, "m", lectura.lufsM);
                pon (l, "s", lectura.lufsS);
                pon (l, "i", lectura.lufsI);
                pon (datos, "lufs", l);
            }

    const bool reproduciendo = datos["reproduciendo"];

    if (reproduciendo || reproduciendoAntes)
        emitir (protocolo::evento ("medidores", datos));

    reproduciendoAntes = reproduciendo;

    // Autoguardado: cada ~2 minutos, si el proyecto tiene carpeta y cambios.
    // T.E. guarda con seguridad (temporal + rename): un corte no lo rompe.
    if (++tics >= 15 * 120)
    {
        tics = 0;
        if (edit != nullptr && carpetaProyecto != juce::File() && edit->hasChangedSinceSaved())
        {
            te::EditFileOperations (*edit).save (false, false, false);
            emitirModelo();
        }
    }
}

/* ================================================================= bomba */

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

/* ============================================================ autoprueba */

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

    auto pausa = [] (int ms) { juce::MessageManager::getInstance()->runDispatchLoopUntil (ms); };

    // Proyecto de verdad en una carpeta temporal: F1 entero de una pasada.
    const auto proyecto = carpeta.getChildFile ("autoprueba-proyecto");
    proyecto.deleteRecursively();

    nuevoProyecto (proyecto.getFullPathName());
    pausa (100);

    const auto importado = importarClip (0, wav.getFullPathName(), 0.0);
    pausa (100);
    const juce::String idClip = importado["id"].toString();

    // Editar: dividir por la mitad, apartar la segunda parte y recortarla.
    const auto division = dividirClip (idClip, 0.5);
    const juce::String idSegundo = division["id"].toString();
    moverClip (idSegundo, 2.0, 1);
    recortarClip (idSegundo, 2.0, 2.4);
    duplicarClip (idClip);
    fundidosClip (idClip, 0.1, 0.1);

    // Deshacer y rehacer, sobre una operación de clip: borrar, recuperar,
    // volver a borrar y recuperar de nuevo. Deben quedar los dos clips.
    borrarClip (idSegundo);
    deshacer();
    rehacer();
    deshacer();
    pausa (100);
    const auto trasDeshacer = listarPistas();
    const int clipsTrasDeshacer = (int) trasDeshacer["pistas"][0]["clips"].size()
                                + (int) trasDeshacer["pistas"][1]["clips"].size();

    // La suite en el máster: mezcla y mastering al completo hasta la fecha.
    insertarPlugin (-1, "eqocho", 0);
    insertarPlugin (-1, "compresor", 1);
    insertarPlugin (-1, "puerta", 2);
    insertarPlugin (-1, "placa", 3);
    insertarPlugin (-1, "delay", 4);
    insertarPlugin (-1, "techo", 5);
    insertarPlugin (-1, "medidor", 6);
    parametroPlugin (-1, 5, "techo", -3.0);
    pausa (200);

    // Reproducir con la bomba. El audio recién copiado necesita que el motor
    // le prepare su proxy en segundo plano: se espera al sonido, no a un reloj.
    tocar();
    float pico = 0.0f;
    for (int esperado = 0; esperado < 5000 && pico < 0.05f; esperado += 100)
    {
        pausa (100);
        pico = juce::jmax (picoIzq.load(), picoDer.load());
    }
    pausa (400); // que corra un poco más: la posición también se verifica
    const auto segundos = edit->getTransport().getPosition().inSeconds();
    parar();

    // Exportar y verificar que el WAV existe y trae señal.
    const auto salida = carpeta.getChildFile ("autoprueba-render.wav");
    exportar (salida.getFullPathName());

    float picoRender = 0.0f;
    {
        std::unique_ptr<juce::AudioFormatReader> lector (formatos().createReaderFor (salida));
        if (lector != nullptr)
        {
            juce::AudioBuffer<float> b ((int) lector->numChannels, (int) juce::jmin ((juce::int64) 96000, lector->lengthInSamples));
            lector->read (&b, 0, b.getNumSamples(), 0, true, true);
            picoRender = b.getMagnitude (0, b.getNumSamples());
        }
    }

    // Guardar, reabrir, y que el proyecto vuelva entero.
    guardarProyecto();
    const auto rutaProyecto = carpetaProyecto.getFullPathName();
    adoptarEdit (nullptr, {});
    abrirProyecto (rutaProyecto);
    const auto reabierto = listarPistas();
    const int clipsReabiertos = (int) reabierto["pistas"][0]["clips"].size()
                              + (int) reabierto["pistas"][1]["clips"].size();
    const int pluginsMaster = (int) reabierto["master"]["plugins"].size();

    const bool avanza = segundos > 0.25;
    const bool suena = pico > 0.05f;
    const bool deshace = clipsTrasDeshacer == 3;
    const bool renderiza = picoRender > 0.05f;
    const bool persiste = clipsReabiertos == 3 && pluginsMaster == 7;
    const bool ok = avanza && suena && deshace && renderiza && persiste;

    auto r = objeto();
    pon (r, "ok", ok);
    pon (r, "segundos", segundos);
    pon (r, "pico", pico);
    pon (r, "clipsTrasDeshacer", clipsTrasDeshacer);
    pon (r, "picoRender", picoRender);
    pon (r, "clipsReabiertos", clipsReabiertos);
    pon (r, "pluginsMaster", pluginsMaster);
    emitir (protocolo::evento ("prueba", r));

    return ok ? 0 : 1;
}
