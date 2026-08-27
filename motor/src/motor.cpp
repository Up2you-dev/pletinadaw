/*  Pletina DAW · motor
    Copyright (C) 2026 Up2you — GPLv3: ver el LICENSE de la raíz.

    Implementación del envoltorio del motor. Los "porqués" de cada modo de
    audio están en motor.h; aquí solo hay mecánica.
*/

#include "motor.h"
#include "efectos.h"
#include "efectos2.h"
#include "efectos3.h"
#include "efectos4.h"
#include "protocolo.h"

// TempoDetect no viaja en el header público del módulo: se incluye a pelo.
// Envuelve el BPMDetect de SoundTouch, que ya va compilado dentro del motor.
#include <tracktion_engine/timestretch/tracktion_TempoDetect.h>

#include <cmath>
#include <cstdlib>

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
                                  DelayPlugin::xmlTypeName, PuertaPlugin::xmlTypeName,
                                  MultibandaPlugin::xmlTypeName, AnchuraPlugin::xmlTypeName,
                                  ChispaPlugin::xmlTypeName, OxidoPlugin::xmlTypeName,
                                  DitherPlugin::xmlTypeName, OsciladorPlugin::xmlTypeName,
                                  SalaPlugin::xmlTypeName, PegamentoPlugin::xmlTypeName,
                                  DeeserPlugin::xmlTypeName, EQDinamicoPlugin::xmlTypeName,
                                  BalancinPlugin::xmlTypeName, ConvolucionPlugin::xmlTypeName,
                                  BrumaPlugin::xmlTypeName, CintaPlugin::xmlTypeName,
                                  PadsPlugin::xmlTypeName,
                                  ValvulasPlugin::xmlTypeName, ConsolaPlugin::xmlTypeName,
                                  RemachePlugin::xmlTypeName, OptoPlugin::xmlTypeName,
                                  LamparaPlugin::xmlTypeName, EcoPlugin::xmlTypeName,
                                  MuellePlugin::xmlTypeName, EspejismoPlugin::xmlTypeName,
                                  MultitapPlugin::xmlTypeName, CoroPlugin::xmlTypeName,
                                  TremoloPlugin::xmlTypeName, TriodoPlugin::xmlTypeName,
                                  SumadoraPlugin::xmlTypeName, MachacadoraPlugin::xmlTypeName,
                                  PeinePlugin::xmlTypeName };
}

Motor::Motor (Opciones o, std::function<void (const juce::String&)> e)
    : opciones (o), emitir (std::move (e))
{
    registrarEfectos (engine);

    // El catálogo VST3 del último escaneo, si lo hay: escanear es caro.
    if (auto xml = engine.getPropertyStorage().getXmlProperty (te::SettingID::knownPluginList))
        engine.getPluginManager().knownPluginList.recreateFromXml (*xml);

    if (opciones.sinAudio)
    {
        // La interfaz hospedada convierte al motor en un procesador al que
        // nosotros le empujamos bloques: no se abre ningún dispositivo real.
        auto& audioIO = engine.getDeviceManager().getHostedAudioDeviceInterface();

        te::HostedAudioDeviceInterface::Parameters parametros;
        parametros.sampleRate = opciones.frecuencia;
        parametros.blockSize = opciones.bloque;
        parametros.inputChannels = 2;   // la bomba también es la "entrada": se graba de ella
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
    for (size_t i = 0; i < medidoresGrupo.size(); ++i)
        medidoresGrupo[i]->measurer.removeClient (*clientesGrupo[i]);

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
                                         "mezcla", "suite", "deshacer", "render",
                                         "grupos", "racks" };
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
    for (size_t i = 0; i < medidoresGrupo.size(); ++i)
        medidoresGrupo[i]->measurer.removeClient (*clientesGrupo[i]);
    medidoresGrupo.clear();
    clientesGrupo.clear();

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
    pon (r, "escenas", edit->getSceneList().getNumScenes());
    pon (r, "cuantizacionLanzamiento", te::getName (edit->getLaunchQuantisation().type));

    auto& transporte = edit->getTransport();
    auto bucleVar = objeto();
    const auto rango = transporte.getLoopRange();
    pon (bucleVar, "activo", (bool) transporte.looping);
    pon (bucleVar, "inicio", rango.getStart().inSeconds());
    pon (bucleVar, "fin", rango.getEnd().inSeconds());
    pon (r, "bucle", bucleVar);

    auto describirCadena = [this] (const juce::Array<te::Plugin*>& plugins)
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

            // La entrada lateral, si el plugin la tiene: la pista de origen o -1.
            if (p->canSidechain())
            {
                pon (d, "admiteLateral", true);
                int fuenteLateral = -1;
                const auto idFuente = p->getSidechainSourceID();
                if (idFuente.isValid())
                {
                    int j = 0;
                    for (auto t : te::getAudioTracks (*edit))
                    {
                        if (t->itemID == idFuente) { fuenteLateral = j; break; }
                        ++j;
                    }
                }
                pon (d, "lateral", fuenteLateral);
            }

            // Un rack: sus macros (con asignaciones) y la subcadena envuelta,
            // para que la interfaz pinte la tarjeta entera. No hay recursión
            // posible: el motor veta racks dentro de racks.
            if (auto* rack = dynamic_cast<te::RackInstance*> (p))
                if (rack->type != nullptr)
                {
                    auto contenidos = rack->type->getPlugins();

                    juce::Array<juce::var> cadenaVar;
                    int k = 0;
                    for (auto* contenido : contenidos)
                    {
                        auto c = objeto();
                        pon (c, "indice", k++);
                        pon (c, "tipo", contenido->getPluginType());
                        pon (c, "nombre", contenido->getName());
                        pon (c, "activo", contenido->isEnabled());
                        pon (c, "parametros", describirParametros (*contenido));
                        cadenaVar.add (c);
                    }
                    pon (d, "cadena", cadenaVar);

                    juce::Array<juce::var> macrosVar;
                    int m = 0;
                    for (auto* mp : rack->type->getMacroParameters())
                    {
                        auto mv = objeto();
                        pon (mv, "indice", m++);
                        pon (mv, "nombre", mp->macroName.get());
                        pon (mv, "valor", mp->getCurrentValue());

                        juce::Array<juce::var> asignaciones;
                        int contenidoIdx = 0;
                        for (auto* contenido : contenidos)
                        {
                            for (auto par : contenido->getAutomatableParameters())
                                for (auto* a : par->getAssignments())
                                    if (auto* am = dynamic_cast<te::MacroParameter::Assignment*> (a))
                                        if (am->macroParamID.toString() == mp->paramID)
                                        {
                                            auto av = objeto();
                                            pon (av, "plugin", contenidoIdx);
                                            pon (av, "parametro", par->paramID);
                                            pon (av, "cantidad", a->value.get());
                                            asignaciones.add (av);
                                        }
                            ++contenidoIdx;
                        }
                        pon (mv, "asignaciones", asignaciones);
                        macrosVar.add (mv);
                    }
                    pon (d, "macros", macrosVar);
                }

            lista.add (d);
        }
        return juce::var (lista);
    };

    auto carpetas = te::getTracksOfType<te::FolderTrack> (*edit, true);

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
        pon (p, "grupo", carpetas.indexOf (pista->getParentFolderTrack()));

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
                pon (d, "tipo", "audio");
                pon (d, "ruta", onda->getCurrentSourceFile().getFullPathName());
                pon (d, "duracionFuente", onda->getAudioFile().getLength());
                pon (d, "entradaFundido", onda->getFadeIn().inSeconds());
                pon (d, "salidaFundido", onda->getFadeOut().inSeconds());
                pon (d, "autoTempo", onda->getAutoTempo());
                pon (d, "transposicion", onda->getPitchChange());
                pon (d, "bpmFuente", onda->getLoopInfo().getBpm (onda->getAudioFile().getInfo()));
            }
            else if (auto* midi = dynamic_cast<te::MidiClip*> (c))
            {
                pon (d, "tipo", "midi");
                pon (d, "cuantizacion", midi->getQuantisation().getType (false));

                juce::Array<juce::var> notas;
                for (auto* n : midi->getSequence().getNotes())
                {
                    auto nota = objeto();
                    pon (nota, "nota", n->getNoteNumber());
                    pon (nota, "inicio", n->getStartBeat().inBeats());
                    pon (nota, "duracion", n->getLengthBeats().inBeats());
                    pon (nota, "velocidad", n->getVelocity());
                    notas.add (nota);
                }
                pon (d, "notas", notas);
            }

            clips.add (d);
        }
        pon (p, "clips", clips);
        pon (p, "plugins", describirCadena (cadenaUsuario (indice - 1)));

        // Envíos a los dos buses, si los hay; retorno y congelada, si lo son.
        double envios[2] = { -100.0, -100.0 };
        bool esRetorno = false;
        for (auto enchufado : pista->pluginList.getPlugins())
        {
            if (auto* e = dynamic_cast<te::AuxSendPlugin*> (enchufado))
                if (e->busNumber >= 0 && e->busNumber < 2)
                    envios[e->busNumber] = e->getGainDb();
            if (dynamic_cast<te::AuxReturnPlugin*> (enchufado) != nullptr)
                esRetorno = true;
        }
        juce::Array<juce::var> enviosVar { envios[0], envios[1] };
        pon (p, "envios", enviosVar);
        pon (p, "retorno", esRetorno);
        pon (p, "congelada", pista->isFrozen (te::Track::individualFreeze));

        bool armada = false;
        if (auto* contexto = edit->getCurrentPlaybackContext())
            for (auto* instancia : contexto->getAllInputs())
                armada = armada || instancia->isRecordingEnabled (pista->itemID);
        pon (p, "armada", armada);

        // Las ranuras de la Session View, si hay escenas creadas.
        juce::Array<juce::var> ranuras;
        for (auto* ranura : pista->getClipSlotList().getClipSlots())
        {
            auto s = objeto();
            if (auto* c = ranura->getClip())
            {
                pon (s, "clip", c->itemID.toString());
                pon (s, "nombre", c->getName());
                pon (s, "tipo", dynamic_cast<te::MidiClip*> (c) != nullptr ? "midi" : "audio");

                juce::String lanzamiento = "parado";
                if (auto asa = c->getLaunchHandle())
                {
                    if (asa->getQueuedStatus().has_value())
                        lanzamiento = "encolado";
                    else if (asa->getPlayingStatus() == te::LaunchHandle::PlayState::playing)
                        lanzamiento = "tocando";
                }
                pon (s, "estado", lanzamiento);
            }
            ranuras.add (s);
        }
        pon (p, "ranuras", ranuras);

        // La curva de volumen, para el carril de automatización de la interfaz.
        juce::Array<juce::var> curvaVolumen;
        if (auto* volumen = pista->getVolumePlugin())
        {
            auto& curva = volumen->volParam->getCurve();
            for (int j = 0; j < curva.getNumPoints(); ++j)
            {
                auto d = objeto();
                pon (d, "t", curva.getPointTime (j).inSeconds());
                pon (d, "v", te::volumeFaderPositionToDB (curva.getPointValue (j)));
                curvaVolumen.add (d);
            }
        }
        pon (p, "automatizacionVolumen", curvaVolumen);

        pistas.add (p);
    }
    pon (r, "pistas", pistas);

    // Los grupos (carpetas de submezcla), con su mezcla, sus miembros por
    // índice plano y su propia cadena de usuario.
    juce::Array<juce::var> grupos;
    {
        auto pistasAudio = te::getAudioTracks (*edit);
        for (int g = 0; g < carpetas.size(); ++g)
        {
            auto* carpeta = carpetas[g];
            auto gv = objeto();
            pon (gv, "indice", g);
            pon (gv, "nombre", carpeta->getName());
            pon (gv, "mute", carpeta->isMuted (false));
            pon (gv, "solo", carpeta->isSolo (false));

            if (auto* volumen = carpeta->getVolumePlugin())
            {
                pon (gv, "volumenDb", volumen->getVolumeDb());
                pon (gv, "pan", volumen->getPan());
            }

            juce::Array<juce::var> miembros;
            for (int i = 0; i < pistasAudio.size(); ++i)
                if (pistasAudio[i]->getParentFolderTrack() == carpeta)
                    miembros.add (i);
            pon (gv, "pistas", miembros);

            pon (gv, "plugins", describirCadena (cadenaUsuario (-2 - g)));
            grupos.add (gv);
        }
    }
    pon (r, "grupos", grupos);

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

/* ================================================================== VST3 */

juce::StringArray Motor::rutasVst() const
{
    auto& almacen = engine.getPropertyStorage();
    juce::StringArray rutas;
    rutas.addTokens (almacen.getPropertyItem (te::SettingID::knownPluginList, "carpetasVst3", "").toString(), ";", {});
    rutas.removeEmptyStrings();

    if (rutas.isEmpty())
    {
        // Las carpetas estándar del sistema, que es donde vive casi todo.
        juce::VST3PluginFormat formato;
        for (int i = 0; i < formato.getDefaultLocationsToSearch().getNumPaths(); ++i)
            rutas.add (formato.getDefaultLocationsToSearch()[i].getFullPathName());
    }
    return rutas;
}

void Motor::guardarCatalogoVst()
{
    if (auto xml = engine.getPluginManager().knownPluginList.createXml())
        engine.getPropertyStorage().setXmlProperty (te::SettingID::knownPluginList, *xml);
}

juce::var Motor::carpetasVst (const juce::var& params)
{
    if (params.hasProperty ("rutas"))
    {
        juce::StringArray rutas;
        if (auto* lista = params["rutas"].getArray())
            for (const auto& r : *lista)
                rutas.add (r.toString());
        engine.getPropertyStorage().setPropertyItem (te::SettingID::knownPluginList, "carpetasVst3", rutas.joinIntoString (";"));
    }

    juce::Array<juce::var> lista;
    for (const auto& r : rutasVst()) lista.add (r);
    auto resultado = objeto();
    pon (resultado, "rutas", lista);
    return resultado;
}

juce::var Motor::escanearVst()
{
    juce::VST3PluginFormat formato;
    auto& conocidos = engine.getPluginManager().knownPluginList;

    juce::FileSearchPath busqueda;
    for (const auto& r : rutasVst()) busqueda.add (juce::File (r));

    // Enumerar candidatos es barato y seguro; CARGARLOS no: cada uno se abre
    // en un proceso hijo (este mismo binario) y el que reviente o se cuelgue
    // va a la lista negra sin llevarse el motor por delante.
    const auto candidatos = formato.searchPathsForPlugins (busqueda, true, true);
    const auto binario = juce::File::getSpecialLocation (juce::File::currentExecutableFile);

    int nuevos = 0, vetados = 0;
    for (const auto& candidato : candidatos)
    {
        if (conocidos.getBlacklistedFiles().contains (candidato))
            { ++vetados; continue; }

        bool yaConocido = false;
        for (const auto& tipo : conocidos.getTypes())
            if (tipo.fileOrIdentifier == candidato)
                yaConocido = true;
        if (yaConocido) continue;

        juce::ChildProcess hijo;
        juce::StringArray orden { binario.getFullPathName(), "--escanear-vst3", candidato };
        bool bien = hijo.start (orden, juce::ChildProcess::wantStdOut);
        juce::String salida;
        if (bien)
        {
            salida = hijo.readAllProcessOutput();          // espera al hijo
            bien = hijo.waitForProcessToFinish (20000) && hijo.getExitCode() == 0;
        }

        if (! bien)
        {
            conocidos.addToBlacklist (candidato);
            ++vetados;
            continue;
        }

        juce::StringArray lineas;
        lineas.addLines (salida);
        for (const auto& linea : lineas)
        {
            if (linea.trim().isEmpty()) continue;
            if (auto xml = juce::parseXML (linea))
            {
                juce::PluginDescription descripcion;
                if (descripcion.loadFromXml (*xml))
                {
                    conocidos.addType (descripcion);
                    ++nuevos;
                }
            }
        }
    }

    guardarCatalogoVst();

    auto r = objeto();
    pon (r, "candidatos", (int) candidatos.size());
    pon (r, "nuevos", nuevos);
    pon (r, "vetados", vetados);
    pon (r, "total", conocidos.getNumTypes());
    return r;
}

juce::var Motor::listaVst() const
{
    juce::Array<juce::var> lista;
    for (const auto& tipo : engine.getPluginManager().knownPluginList.getTypes())
    {
        auto d = objeto();
        pon (d, "id", tipo.createIdentifierString());
        pon (d, "nombre", tipo.name);
        pon (d, "fabricante", tipo.manufacturerName);
        pon (d, "instrumento", tipo.isInstrument);
        lista.add (d);
    }
    auto r = objeto();
    pon (r, "plugins", lista);
    return r;
}

/* ================================================================ previa */

juce::var Motor::tocarPrevia (const juce::String& ruta)
{
    const juce::File archivo (ruta);
    if (! archivo.existsAsFile())
        throw std::runtime_error ("no existe el archivo: " + ruta.toStdString());

    te::AudioFile audio (engine, archivo);
    if (! audio.isValid())
        throw std::runtime_error ("formato de audio no reconocido");

    if (editPrevia == nullptr)
    {
        auto carpeta = engine.getTemporaryFileManager().getTempDirectory();
        editPrevia = te::createEmptyEdit (engine, carpeta.getChildFile ("previa.tracktionedit"));
        editPrevia->ensureNumberOfAudioTracks (1);
        if (auto volumen = editPrevia->getMasterVolumePlugin())
            volumen->setVolumeDb (-3.0f);
    }

    auto pistas = te::getAudioTracks (*editPrevia);
    if (pistas.isEmpty())
        throw std::runtime_error ("la previa no tiene pista");

    auto& transporte = editPrevia->getTransport();
    transporte.stop (false, false);

    for (auto* clipViejo : pistas[0]->getClips())
        clipViejo->removeFromParent();

    if (pistas[0]->insertWaveClip (archivo.getFileNameWithoutExtension(), archivo,
                                   { { te::TimePosition(), te::TimePosition::fromSeconds (audio.getLength()) }, {} },
                                   false) == nullptr)
        throw std::runtime_error ("no se pudo preparar la audición");

    transporte.setPosition (te::TimePosition());
    transporte.ensureContextAllocated();
    transporte.play (false);

    auto r = objeto();
    pon (r, "duracion", audio.getLength());
    return r;
}

juce::var Motor::pararPrevia()
{
    if (editPrevia != nullptr)
        editPrevia->getTransport().stop (false, false);
    return juce::var (true);
}

/* ================================================================ pistas */

te::AudioTrack* Motor::pista (int indice) const
{
    if (edit == nullptr)
        return nullptr;
    auto pistas = te::getAudioTracks (*edit);
    return indice >= 0 && indice < pistas.size() ? pistas[indice] : nullptr;
}

te::FolderTrack* Motor::grupo (int indice) const
{
    if (edit == nullptr)
        return nullptr;
    auto carpetas = te::getTracksOfType<te::FolderTrack> (*edit, true);
    return indice >= 0 && indice < carpetas.size() ? carpetas[indice] : nullptr;
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
    recogerRacksHuerfanos();
    emitirModelo();
    return listarPistas();
}

juce::var Motor::renombrarPista (int indice, const juce::String& nombre)
{
    asegurarEdit();

    if (indice <= -2)
    {
        auto* g = grupo (-2 - indice);
        if (g == nullptr)
            throw std::runtime_error ("no existe el grupo");
        edit->getUndoManager().beginNewTransaction ("renombrar grupo");
        g->setName (nombre.trim().isEmpty() ? juce::String ("Grupo") : nombre);
        emitirModelo();
        return listarPistas();
    }

    auto* objetivo = pista (indice);
    if (objetivo == nullptr)
        throw std::runtime_error ("no existe la pista");

    edit->getUndoManager().beginNewTransaction ("renombrar pista");
    objetivo->setName (nombre.trim().isEmpty() ? juce::String ("Pista") : nombre);
    emitirModelo();
    return listarPistas();
}

juce::var Motor::moverPista (int indice, int tras)
{
    asegurarEdit();

    auto* objetivo = pista (indice);
    if (objetivo == nullptr)
        throw std::runtime_error ("no existe la pista");

    // La pista aterriza justo detrás de `tras` y ADOPTA su grupo (si el
    // vecino de arriba vive en una carpeta, ella entra; si no, queda suelta).
    // tras = -1 la pone la primera, al nivel raíz.
    te::Track* delante = nullptr;
    te::FolderTrack* padre = nullptr;
    if (tras >= 0)
    {
        auto* vecino = pista (tras);
        if (vecino == nullptr)
            throw std::runtime_error ("no existe la pista de destino");
        if (vecino == objetivo)
            return listarPistas();
        delante = vecino;
        padre = vecino->getParentFolderTrack();
    }

    edit->getUndoManager().beginNewTransaction ("mover pista");
    edit->moveTrack (objetivo, te::TrackInsertPoint (padre, delante));

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
    else if (indice <= -2)
    {
        auto* g = grupo (-2 - indice);
        if (g == nullptr)
            throw std::runtime_error ("no existe el grupo");

        if (auto* volumen = g->getVolumePlugin())
        {
            if (params.hasProperty ("volumenDb")) volumen->setVolumeDb ((float) (double) params["volumenDb"]);
            if (params.hasProperty ("pan")) volumen->setPan ((float) (double) params["pan"]);
        }
        if (params.hasProperty ("mute")) g->setMute ((bool) params["mute"]);
        if (params.hasProperty ("solo")) g->setSolo ((bool) params["solo"]);
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

/* ================================================================ grupos */

juce::var Motor::crearGrupo (const juce::var& params)
{
    asegurarEdit();

    if (! params["pistas"].isArray() || params["pistas"].size() == 0)
        throw std::runtime_error ("falta la lista de pistas del grupo");

    // Índices ordenados y sin repetir: el grupo respeta el orden del arreglo.
    juce::Array<int> indices;
    for (const auto& v : *params["pistas"].getArray())
    {
        const int i = (int) v;
        if (! indices.contains (i))
            indices.add (i);
    }
    indices.sort();

    juce::Array<te::AudioTrack*> miembros;
    for (int i : indices)
    {
        auto* m = pista (i);
        if (m == nullptr)
            throw std::runtime_error ("no existe la pista " + std::to_string (i));
        if (m->getParentFolderTrack() != nullptr)
            throw std::runtime_error ("la pista " + std::to_string (i) + " ya está en un grupo");
        miembros.add (m);
    }

    edit->getUndoManager().beginNewTransaction ("crear grupo");

    // La carpeta nace de submezcla (fader + VU de serie) en el hueco del
    // primer miembro; los miembros entran encadenando el preceding, porque
    // moveTrack con preceding nulo aterriza en el índice crudo 1 e invierte.
    auto carpeta = edit->insertNewFolderTrack (te::TrackInsertPoint (nullptr, miembros.getFirst()), nullptr, true);
    if (carpeta == nullptr)
        throw std::runtime_error ("el motor no ha podido crear el grupo");

    const auto nombre = params["nombre"].toString().trim();
    carpeta->setName (nombre.isEmpty()
                          ? "Grupo " + juce::String (te::getTracksOfType<te::FolderTrack> (*edit, true).size())
                          : nombre);

    te::Track* previo = nullptr;
    for (auto* m : miembros)
    {
        edit->moveTrack (m, te::TrackInsertPoint (carpeta.get(), previo));
        previo = m;
    }

    emitirModelo();
    return listarPistas();
}

juce::var Motor::meterEnGrupo (int indicePista, int indiceGrupo)
{
    asegurarEdit();

    auto* objetivo = pista (indicePista);
    if (objetivo == nullptr)
        throw std::runtime_error ("no existe la pista");
    if (objetivo->getParentFolderTrack() != nullptr)
        throw std::runtime_error ("la pista ya está en un grupo");

    auto* g = grupo (indiceGrupo);
    if (g == nullptr)
        throw std::runtime_error ("no existe el grupo");

    edit->getUndoManager().beginNewTransaction ("meter en grupo");

    auto hijas = g->getAllSubTracks (false);
    edit->moveTrack (objetivo, te::TrackInsertPoint (g, hijas.isEmpty() ? nullptr : hijas.getLast()));

    emitirModelo();
    return listarPistas();
}

juce::var Motor::sacarDeGrupo (int indicePista)
{
    asegurarEdit();

    auto* objetivo = pista (indicePista);
    if (objetivo == nullptr)
        throw std::runtime_error ("no existe la pista");

    auto* carpeta = objetivo->getParentFolderTrack();
    if (carpeta == nullptr)
        throw std::runtime_error ("la pista no está en ningún grupo");

    edit->getUndoManager().beginNewTransaction ("sacar del grupo");
    edit->moveTrack (objetivo, te::TrackInsertPoint (nullptr, carpeta));

    // Un grupo vacío no genera nodo de audio: mejor disolverlo que dejarlo
    // de zombi. Invariante: todo grupo tiene al menos una pista.
    if (carpeta->getAllSubTracks (false).isEmpty())
    {
        edit->deleteTrack (carpeta);
        recogerRacksHuerfanos();
    }

    emitirModelo();
    return listarPistas();
}

juce::var Motor::deshacerGrupo (int indiceGrupo)
{
    asegurarEdit();

    auto* carpeta = grupo (indiceGrupo);
    if (carpeta == nullptr)
        throw std::runtime_error ("no existe el grupo");

    edit->getUndoManager().beginNewTransaction ("deshacer grupo");

    // Sacar a las hijas ANTES de borrar la carpeta (borrarla se las lleva);
    // salen en orden, aterrizando donde estaba el grupo.
    te::Track* previo = carpeta;
    for (auto* hija : carpeta->getAllSubTracks (false))
    {
        edit->moveTrack (hija, te::TrackInsertPoint (nullptr, previo));
        previo = hija;
    }

    edit->deleteTrack (carpeta);
    recogerRacksHuerfanos();

    emitirModelo();
    return listarPistas();
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

    // Detección de tempo orientativa (SoundTouch), sobre el primer minuto como
    // mucho. Si no sale un BPM sensato no se apunta nada: mejor callar que mentir.
    const double bpmFuente = detectarBpm (destino);
    if (bpmFuente > 0.0)
        nuevo->getLoopInfo().setBpm (bpmFuente, audio.getInfo());

    emitirModelo();

    auto r = objeto();
    pon (r, "id", nuevo->itemID.toString());
    pon (r, "duracion", audio.getLength());
    pon (r, "bpmFuente", bpmFuente);
    return r;
}

double Motor::detectarBpm (const juce::File& archivo)
{
    std::unique_ptr<juce::AudioFormatReader> lector (formatos().createReaderFor (archivo));
    if (lector == nullptr || lector->sampleRate <= 0 || lector->numChannels < 1)
        return 0.0;

    const int canales = juce::jmin (2, (int) lector->numChannels);
    te::TempoDetect detector (canales, lector->sampleRate);

    const juce::int64 tope = juce::jmin (lector->lengthInSamples, (juce::int64) (60.0 * lector->sampleRate));
    const int bloque = 65536;
    juce::AudioBuffer<float> buffer (canales, bloque);

    for (juce::int64 hecho = 0; hecho < tope;)
    {
        const int ahora = (int) juce::jmin ((juce::int64) bloque, tope - hecho);
        lector->read (&buffer, 0, ahora, hecho, true, canales > 1);
        detector.processSection (buffer, ahora);
        hecho += ahora;
    }

    detector.finishAndDetect();
    return detector.isBpmSensible() ? (double) detector.getBpm() : 0.0;
}

juce::var Motor::warpClip (const juce::String& id, const juce::var& params)
{
    auto* objetivo = dynamic_cast<te::WaveAudioClip*> (clip (id));
    if (objetivo == nullptr)
        throw std::runtime_error ("no existe el clip de audio");

    edit->getUndoManager().beginNewTransaction ("warp");

    if (params.hasProperty ("bpmFuente"))
    {
        const double bpm = params["bpmFuente"];
        if (bpm < 20.0 || bpm > 999.0)
            throw std::runtime_error ("bpmFuente fuera de rango");
        objetivo->getLoopInfo().setBpm (bpm, objetivo->getAudioFile().getInfo());
    }

    if (params.hasProperty ("modo"))
        objetivo->setTimeStretchMode (params["modo"].toString() == "normal"
                                          ? te::TimeStretcher::soundtouchNormal
                                          : te::TimeStretcher::soundtouchBetter);

    if (params.hasProperty ("transposicion"))
    {
        if (objetivo->getTimeStretchMode() == te::TimeStretcher::disabled)
            objetivo->setTimeStretchMode (te::TimeStretcher::soundtouchBetter);
        objetivo->setPitchChange ((float) (double) params["transposicion"]);
    }

    if (params.hasProperty ("autoTempo"))
    {
        const bool activo = params["autoTempo"];
        if (activo)
        {
            if (objetivo->getLoopInfo().getBpm (objetivo->getAudioFile().getInfo()) <= 0.0)
                throw std::runtime_error ("el clip no tiene tempo de origen: manda bpmFuente");
            if (objetivo->getTimeStretchMode() == te::TimeStretcher::disabled)
                objetivo->setTimeStretchMode (te::TimeStretcher::soundtouchBetter);
        }
        objetivo->setAutoTempo (activo);
    }

    emitirModelo();
    return juce::var (true);
}

juce::var Motor::crearClipMidi (int indicePista, double inicio, double compases)
{
    asegurarEdit();
    auto* objetivo = pista (indicePista);
    if (objetivo == nullptr)
        throw std::runtime_error ("no existe la pista");

    edit->getUndoManager().beginNewTransaction ("crear clip MIDI");

    // La duración va en compases del proyecto: el clip nace cuadrado.
    auto& secuenciaTempo = edit->tempoSequence;
    const auto desde = te::TimePosition::fromSeconds (juce::jmax (0.0, inicio));
    const double pulsos = secuenciaTempo.getTimeSig (0)->numerator.get() * juce::jlimit (0.25, 64.0, compases);
    const auto fin = secuenciaTempo.toTime (secuenciaTempo.toBeats (desde) + te::BeatDuration::fromBeats (pulsos));

    auto nuevo = objetivo->insertMIDIClip ({ desde, fin }, nullptr);
    if (nuevo == nullptr)
        throw std::runtime_error ("no se pudo crear el clip MIDI");
    nuevo->setName ("Clip MIDI");

    emitirModelo();
    auto r = objeto();
    pon (r, "id", nuevo->itemID.toString());
    return r;
}

juce::var Motor::notasClipMidi (const juce::String& id, const juce::var& notas)
{
    auto* objetivo = dynamic_cast<te::MidiClip*> (clip (id));
    if (objetivo == nullptr)
        throw std::runtime_error ("no existe el clip MIDI");
    if (notas.getArray() == nullptr)
        throw std::runtime_error ("faltan las notas");

    // El piano roll manda la lista entera: una transacción, un deshacer.
    edit->getUndoManager().beginNewTransaction ("editar notas");
    auto* um = &edit->getUndoManager();
    auto& secuencia = objetivo->getSequence();
    secuencia.clear (um);

    for (const auto& n : *notas.getArray())
        secuencia.addNote (juce::jlimit (0, 127, (int) n["nota"]),
                           te::BeatPosition::fromBeats (juce::jmax (0.0, (double) n["inicio"])),
                           te::BeatDuration::fromBeats (juce::jmax (0.0625, (double) n["duracion"])),
                           juce::jlimit (1, 127, n.hasProperty ("velocidad") ? (int) n["velocidad"] : 100),
                           0, um);

    emitirModelo();
    return juce::var (true);
}

juce::var Motor::cuantizarClipMidi (const juce::String& id, const juce::String& division)
{
    auto* objetivo = dynamic_cast<te::MidiClip*> (clip (id));
    if (objetivo == nullptr)
        throw std::runtime_error ("no existe el clip MIDI");

    if (! te::QuantisationType::getAvailableQuantiseTypes (false).contains (division))
        throw std::runtime_error ("división desconocida: " + division.toStdString()
                                  + " (usa las de QuantisationType, p. ej. \"1/4 beat\")");

    edit->getUndoManager().beginNewTransaction ("cuantizar");
    te::QuantisationType tipo;
    tipo.setType (division);
    objetivo->setQuantisation (tipo);
    emitirModelo();
    return juce::var (true);
}

/* ================================================================ sesión */

juce::var Motor::escenasSesion (int numero)
{
    asegurarEdit();
    numero = juce::jlimit (0, 64, numero);

    edit->getUndoManager().beginNewTransaction ("escenas");
    edit->getSceneList().ensureNumberOfScenes (numero);
    for (auto pista : te::getAudioTracks (*edit))
        pista->getClipSlotList().ensureNumberOfSlots (numero);

    emitirModelo();
    return listarPistas();
}

juce::var Motor::ponerEnSesion (int indicePista, int escena, const juce::String& desdeClip)
{
    asegurarEdit();
    auto* objetivo = pista (indicePista);
    if (objetivo == nullptr)
        throw std::runtime_error ("no existe la pista");

    auto* origen = clip (desdeClip);
    if (origen == nullptr)
        throw std::runtime_error ("no existe el clip de origen");

    auto ranuras = objetivo->getClipSlotList().getClipSlots();
    if (escena < 0 || escena >= ranuras.size())
        throw std::runtime_error ("no existe esa escena: pide más con sesion.escenas");
    auto* ranura = ranuras[escena];

    edit->getUndoManager().beginNewTransaction ("poner en sesión");

    if (auto* anterior = ranura->getClip())
        anterior->removeFromParent();

    // Copia del clip del arrangement, con IDs nuevos y arrancando en 0:
    // el lanzamiento manda desde dónde suena, no el tiempo del arreglo.
    auto copia = origen->state.createCopy();
    te::EditItemID::remapIDs (copia, nullptr, *edit);
    copia.setProperty (te::IDs::start, 0.0, nullptr);
    copia.setProperty (te::IDs::offset, origen->getPosition().getOffset().inSeconds(), nullptr);

    auto* nuevo = te::insertClipWithState (*ranura, copia);
    if (nuevo == nullptr)
        throw std::runtime_error ("no se pudo poner el clip en la escena");

    emitirModelo();
    auto r = objeto();
    pon (r, "id", nuevo->itemID.toString());
    return r;
}

juce::var Motor::lanzarSesion (int indicePista, int escena)
{
    asegurarEdit();

    auto lanzar = [this, escena] (te::AudioTrack* objetivo)
    {
        auto ranuras = objetivo->getClipSlotList().getClipSlots();
        if (escena >= 0 && escena < ranuras.size())
            if (auto* c = ranuras[escena]->getClip())
                if (auto asa = c->getLaunchHandle())
                    asa->play ({});
    };

    if (indicePista < 0)
        for (auto p : te::getAudioTracks (*edit)) lanzar (p);
    else if (auto* objetivo = pista (indicePista))
        lanzar (objetivo);
    else
        throw std::runtime_error ("no existe la pista");

    // Lanzar arranca el transporte, como en cualquier sesión en directo.
    auto& transporte = edit->getTransport();
    if (! transporte.isPlaying())
    {
        transporte.ensureContextAllocated();
        transporte.play (false);
    }

    return estadoTransporte();
}

juce::var Motor::pararSesion (int indicePista)
{
    asegurarEdit();

    auto parar = [] (te::AudioTrack* objetivo)
    {
        for (auto* ranura : objetivo->getClipSlotList().getClipSlots())
            if (auto* c = ranura->getClip())
                if (auto asa = c->getLaunchHandle())
                    asa->stop ({});
    };

    if (indicePista < 0)
        for (auto p : te::getAudioTracks (*edit)) parar (p);
    else if (auto* objetivo = pista (indicePista))
        parar (objetivo);
    else
        throw std::runtime_error ("no existe la pista");

    return juce::var (true);
}

juce::var Motor::cuantizacionSesion (const juce::String& nombre)
{
    asegurarEdit();
    const auto tipo = te::launchQTypeFromName (nombre);
    if (! tipo.has_value())
        throw std::runtime_error ("cuantización de lanzamiento desconocida: " + nombre.toStdString());

    edit->getLaunchQuantisation().type = *tipo;
    emitirModelo();
    return juce::var (true);
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
    if (indice <= -2)
    {
        auto* g = grupo (-2 - indice);
        return g != nullptr ? &g->pluginList : nullptr;
    }
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

    // "vst:<identificador>" inserta un plugin externo del catálogo escaneado;
    // cualquier otro tipo tiene que ser de la suite propia.
    juce::PluginDescription descripcionVst;
    const bool esVst = tipo.startsWith ("vst:");
    if (esVst)
    {
        bool encontrado = false;
        for (const auto& candidato : engine.getPluginManager().knownPluginList.getTypes())
            if (candidato.createIdentifierString() == tipo.fromFirstOccurrenceOf ("vst:", false, false))
                { descripcionVst = candidato; encontrado = true; break; }
        if (! encontrado)
            throw std::runtime_error ("ese VST3 no está en el catálogo: escanea primero (vst.escanear)");
    }
    else
    {
        bool conocido = false;
        for (auto t : TIPOS_SUITE) conocido = conocido || tipo == t;
        if (! conocido)
            throw std::runtime_error ("tipo de la suite desconocido: " + tipo.toStdString());
    }

    auto* lista = cadena (indicePista);
    if (lista == nullptr)
        throw std::runtime_error ("no existe la pista");

    edit->getUndoManager().beginNewTransaction ("insertar " + tipo);

    auto nuevo = esVst ? edit->getPluginCache().createNewPlugin (te::ExternalPlugin::xmlTypeName, descripcionVst)
                       : edit->getPluginCache().createNewPlugin (tipo, {});
    if (nuevo == nullptr)
        throw std::runtime_error ("no se pudo crear el plugin");

    if (esVst)
        if (auto* externo = dynamic_cast<te::ExternalPlugin*> (nuevo.get()))
            if (! externo->isEnabled() || externo->getAudioPluginInstance() == nullptr)
                throw std::runtime_error ("el VST3 no ha cargado: " + descripcionVst.name.toStdString());

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
    recogerRacksHuerfanos();
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

juce::var Motor::lateralPlugin (int indicePista, int indice, int fuente)
{
    auto usuario = cadenaUsuario (indicePista);
    if (indice < 0 || indice >= usuario.size())
        throw std::runtime_error ("no existe el plugin");

    auto* plugin = usuario[indice];
    if (! plugin->canSidechain())
        throw std::runtime_error ("este plugin no tiene entrada lateral");

    edit->getUndoManager().beginNewTransaction ("entrada lateral");

    if (fuente < 0)
    {
        plugin->setSidechainSourceID ({});
    }
    else
    {
        auto* origen = pista (fuente);
        if (origen == nullptr)
            throw std::runtime_error ("no existe la pista de origen");
        plugin->setSidechainSourceID (origen->itemID);
        if (plugin->getNumWires() == 0)
            plugin->guessSidechainRouting();
    }

    emitirModelo();
    return listarPistas();
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

/* ================================================================= racks */

te::RackInstance* Motor::rackEn (int indicePista, int indice) const
{
    auto usuario = cadenaUsuario (indicePista);
    if (indice < 0 || indice >= usuario.size())
        throw std::runtime_error ("no existe el plugin");

    auto* rack = dynamic_cast<te::RackInstance*> (usuario[indice]);
    if (rack == nullptr)
        throw std::runtime_error ("ese plugin no es un rack");
    if (rack->type == nullptr)
        throw std::runtime_error ("el rack ha perdido su tipo");
    return rack;
}

void Motor::recogerRacksHuerfanos()
{
    if (edit == nullptr)
        return;

    // Quitar una instancia (la ✕ de la tarjeta, borrar su pista, deshacer un
    // grupo) deja el tipo huérfano en RACKS y nadie más lo limpia. Se barre
    // sobre una copia: removeRackType muta la lista viva de getTypes().
    juce::Array<te::RackType::Ptr> tipos;
    for (auto* rt : edit->getRackList().getTypes())
        tipos.add (rt);

    for (auto& rt : tipos)
        if (te::getRackInstancesInEditForType (*rt).isEmpty())
            edit->getRackList().removeRackType (rt);
}

juce::var Motor::crearRack (int indicePista, int desde, int hasta, const juce::String& nombre)
{
    asegurarEdit();

    auto* lista = cadena (indicePista);
    if (lista == nullptr)
        throw std::runtime_error (indicePista <= -2 ? "no existe el grupo" : "no existe la pista");

    auto usuario = cadenaUsuario (indicePista);
    if (usuario.isEmpty())
        throw std::runtime_error ("no hay plugins que envolver");

    if (desde < 0) desde = 0;
    if (hasta < 0) hasta = usuario.size() - 1;
    if (desde > hasta || hasta >= usuario.size())
        throw std::runtime_error ("tramo de plugins imposible");

    te::Plugin::Array seleccion;
    for (int i = desde; i <= hasta; ++i)
    {
        auto* p = usuario[i];
        if (dynamic_cast<te::RackInstance*> (p) != nullptr)
            throw std::runtime_error ("un rack no entra en otro rack");
        if (p->getSidechainSourceID().isValid())
            throw std::runtime_error ("quita antes la entrada lateral de " + p->getName().toStdString());
        seleccion.add (p);
    }

    // La posición cruda del primer envuelto se captura ANTES del envoltorio,
    // porque createTypeToWrapPlugins saca los plugins de la cadena.
    const int crudo = lista->getPlugins().indexOf (usuario[desde]);

    edit->getUndoManager().beginNewTransaction ("crear rack");

    auto rt = te::RackType::createTypeToWrapPlugins (seleccion, *edit);
    if (rt == nullptr)
        throw std::runtime_error ("el motor no ha podido crear el rack");

    rt->rackName = nombre.trim().isEmpty()
                       ? "Rack " + juce::String (edit->getRackList().size())
                       : nombre.trim();

    // Las 8 macros nacen con el rack; las asignaciones vienen después.
    auto& macros = rt->getMacroParameterListForWriting();
    for (int m = 1; m <= 8; ++m)
        if (auto* mp = macros.createMacroParameter())
            mp->macroName = "Macro " + juce::String (m);

    auto instancia = lista->insertPlugin (te::RackInstance::create (*rt), crudo);
    if (instancia == nullptr)
        throw std::runtime_error ("el motor ha rechazado la instancia del rack");

    emitirModelo();
    return listarPistas();
}

juce::var Motor::deshacerRack (int indicePista, int indice)
{
    asegurarEdit();
    auto* rack = rackEn (indicePista, indice);

    edit->getUndoManager().beginNewTransaction ("deshacer rack");

    // Devuelve los plugins en línea a la cadena; si era la última instancia,
    // el propio motor borra el tipo. El barrido caza cualquier otro resto.
    rack->replaceRackWithPluginSequence (nullptr);
    recogerRacksHuerfanos();

    emitirModelo();
    return listarPistas();
}

juce::var Motor::macroRack (int indicePista, int indice, int macro, const juce::var& params)
{
    asegurarEdit();
    auto* rack = rackEn (indicePista, indice);

    auto macros = rack->type->getMacroParameters();
    if (macro < 0 || macro >= macros.size())
        throw std::runtime_error ("no existe esa macro");
    auto mp = macros[macro];

    if (params.hasProperty ("nombre"))
    {
        edit->getUndoManager().beginNewTransaction ("renombrar macro");
        mp->macroName = params["nombre"].toString();
    }

    if (params.hasProperty ("valor"))
        mp->setParameter (juce::jlimit (0.0f, 1.0f, (float) (double) params["valor"]), juce::sendNotificationSync);

    // El valor cambia a chorro al girar el mando: el modelo solo se reemite
    // si ha cambiado el nombre, que eso sí lo pinta todo el mundo.
    if (params.hasProperty ("nombre"))
    {
        emitirModelo();
        return listarPistas();
    }
    return juce::var (true);
}

juce::var Motor::asignarMacroRack (int indicePista, int indice, int macro, const juce::var& params)
{
    asegurarEdit();
    auto* rack = rackEn (indicePista, indice);

    auto macros = rack->type->getMacroParameters();
    if (macro < 0 || macro >= macros.size())
        throw std::runtime_error ("no existe esa macro");
    auto mp = macros[macro];

    const int indicePlugin = (int) params["plugin"];
    auto contenidos = rack->type->getPlugins();
    if (indicePlugin < 0 || indicePlugin >= contenidos.size())
        throw std::runtime_error ("no existe ese plugin dentro del rack");

    const auto idParametro = params["parametro"].toString();
    te::AutomatableParameter* destino = nullptr;
    for (auto p : contenidos[indicePlugin]->getAutomatableParameters())
        if (p->paramID == idParametro)
            { destino = p; break; }
    if (destino == nullptr)
        throw std::runtime_error ("no existe el parámetro: " + idParametro.toStdString());

    // La asignación existente de ESTA macro, si la hay: addModifier es
    // idempotente por fuente e ignoraría una cantidad nueva.
    auto asignaciones = destino->getAssignments();
    te::AutomatableParameter::ModifierAssignment* existente = nullptr;
    for (auto* a : asignaciones)
        if (auto* am = dynamic_cast<te::MacroParameter::Assignment*> (a))
            if (am->macroParamID.toString() == mp->paramID)
                { existente = a; break; }

    edit->getUndoManager().beginNewTransaction ("asignar macro");

    if (params.hasProperty ("quitar") && (bool) params["quitar"])
    {
        if (existente == nullptr)
            throw std::runtime_error ("no había esa asignación");
        destino->removeModifier (*existente);
    }
    else
    {
        const float cantidad = juce::jlimit (-1.0f, 1.0f,
            params.hasProperty ("cantidad") ? (float) (double) params["cantidad"] : 1.0f);

        if (existente != nullptr)
            existente->value = cantidad;
        else
            destino->addModifier (*mp, cantidad, 0.0f, 0.5f);
    }

    // Que el valor asiente ya, sin esperar al siguiente bloque de audio.
    destino->updateFromAutomationSources (edit->getTransport().getPosition());

    emitirModelo();
    return listarPistas();
}

juce::var Motor::parametroRack (int indicePista, int indice, int plugin, const juce::String& parametro, double valor)
{
    asegurarEdit();
    auto* rack = rackEn (indicePista, indice);

    auto contenidos = rack->type->getPlugins();
    if (plugin < 0 || plugin >= contenidos.size())
        throw std::runtime_error ("no existe ese plugin dentro del rack");

    // El gemelo de plugin.parametro para la subcadena: escribe el valor BASE
    // (las macros asignadas suman encima) y no emite modelo, que es de mando.
    for (auto p : contenidos[plugin]->getAutomatableParameters())
    {
        if (p->paramID == parametro)
        {
            p->setParameter ((float) valor, juce::sendNotificationSync);
            return juce::var (true);
        }
    }

    throw std::runtime_error ("no existe el parámetro: " + parametro.toStdString());
}

/* ===================================== envíos, congelar, presets, curvas */

juce::var Motor::envioPista (int indice, int bus, double nivelDb)
{
    asegurarEdit();
    bus = juce::jlimit (0, 1, bus);

    auto* objetivo = pista (indice);
    if (objetivo == nullptr)
        throw std::runtime_error ("no existe la pista");

    // El retorno del bus se crea la primera vez que alguien envía a él: una
    // pista normal con un AuxReturn delante, que aparece en la mesa como otra.
    bool hayRetorno = false;
    for (auto p : te::getAudioTracks (*edit))
        for (auto enchufado : p->pluginList.getPlugins())
            if (auto* retorno = dynamic_cast<te::AuxReturnPlugin*> (enchufado))
                if (retorno->busNumber == bus)
                    hayRetorno = true;

    if (! hayRetorno)
    {
        auto pistas = te::getAudioTracks (*edit);
        auto nueva = edit->insertNewAudioTrack (te::TrackInsertPoint (nullptr, pistas.isEmpty() ? nullptr : pistas.getLast()), nullptr);
        nueva->setName (juce::String ("Retorno ") + (bus == 0 ? "A" : "B"));
        auto retorno = edit->getPluginCache().createNewPlugin (te::AuxReturnPlugin::xmlTypeName, {});
        if (auto* r = dynamic_cast<te::AuxReturnPlugin*> (retorno.get()))
            r->busNumber = bus;
        nueva->pluginList.insertPlugin (retorno, 0, nullptr);
    }

    te::AuxSendPlugin* envio = nullptr;
    for (auto enchufado : objetivo->pluginList.getPlugins())
        if (auto* e = dynamic_cast<te::AuxSendPlugin*> (enchufado))
            if (e->busNumber == bus)
                envio = e;

    if (envio == nullptr)
    {
        auto nuevo = edit->getPluginCache().createNewPlugin (te::AuxSendPlugin::xmlTypeName, {});
        envio = dynamic_cast<te::AuxSendPlugin*> (nuevo.get());
        if (envio == nullptr)
            throw std::runtime_error ("no se pudo crear el envío");
        envio->busNumber = bus;
        objetivo->pluginList.insertPlugin (nuevo, objetivo->pluginList.size(), nullptr);
    }

    envio->setGainDb ((float) juce::jlimit (-100.0, 6.0, nivelDb));
    emitirModelo();
    return juce::var (true);
}

juce::var Motor::congelarPista (int indice, bool activo)
{
    asegurarEdit();
    auto* objetivo = pista (indice);
    if (objetivo == nullptr)
        throw std::runtime_error ("no existe la pista");

    objetivo->setFrozen (activo, te::Track::individualFreeze);
    emitirModelo();
    return juce::var (objetivo->isFrozen (te::Track::individualFreeze));
}

juce::var Motor::armarPista (int indice, bool activo, int entrada, bool midi)
{
    asegurarEdit();
    auto* objetivo = pista (indice);
    if (objetivo == nullptr)
        throw std::runtime_error ("no existe la pista");

    // Las entradas viven en el contexto de reproducción: se crea si no está.
    edit->getTransport().ensureContextAllocated();
    auto* contexto = edit->getCurrentPlaybackContext();
    if (contexto == nullptr)
        throw std::runtime_error ("no hay contexto de reproducción");

    juce::Array<te::InputDeviceInstance*> entradas;
    for (auto* instancia : contexto->getAllInputs())
    {
        const auto tipo = instancia->getInputDevice().getDeviceType();
        if (midi ? (tipo != te::InputDevice::waveDevice) : (tipo == te::InputDevice::waveDevice))
            entradas.add (instancia);
    }

    if (entradas.isEmpty())
        throw std::runtime_error (midi ? "no hay entradas MIDI que armar" : "no hay entradas de audio que armar");

    auto* instancia = entradas[juce::jlimit (0, entradas.size() - 1, entrada)];

    if (activo)
    {
        const auto destino = instancia->setTarget (objetivo->itemID, true, &edit->getUndoManager(), 0);
        if (! destino.has_value())
            throw std::runtime_error ("no se pudo asignar la entrada: " + destino.error().toStdString());
    }
    instancia->setRecordingEnabled (objetivo->itemID, activo);

    edit->dispatchPendingUpdatesSynchronously();
    emitirModelo();
    return listarPistas();
}

namespace
{
    juce::File carpetaPresets (const juce::String& tipo)
    {
        return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
                 .getChildFile ("PletinaDAW").getChildFile ("presets").getChildFile (tipo);
    }

    // Presets de fábrica: puntos de partida con nombre honesto, no magia.
    struct PresetFabrica { const char* tipo; const char* nombre; const char* parametros; };
    const PresetFabrica PRESETS_FABRICA[] = {
        { "compresor", "Voz suave",       "umbral=-24;ratio=2.5;ataque=8;relajacion=150;ganancia=4" },
        { "compresor", "Pegada bateria",  "umbral=-15;ratio=4;ataque=25;relajacion=80;ganancia=3" },
        { "techo",     "Master -1 dB",    "techo=-1;relajacion=60" },
        { "techo",     "Aplastar",        "techo=-6;relajacion=150" },
        { "placa",     "Placa de voz",    "predelay=25;decaimiento=1.8;amortiguacion=6000;mezcla=0.28" },
        { "placa",     "Placa larga",     "predelay=40;decaimiento=4.5;amortiguacion=4500;mezcla=0.35" },
        { "sala",      "Habitacion",      "tamano=0.7;decaimiento=0.8;tempranas=0.8;mezcla=0.22" },
        { "sala",      "Catedral",        "tamano=1.5;decaimiento=8;tempranas=0.4;mezcla=0.35" },
        { "delay",     "Negra ping-pong", "tiempo=6;pingpong=1;realimentacion=0.45;mezcla=0.3" },
        { "delay",     "Slapback",        "tiempo=1;pingpong=0;realimentacion=0.05;mezcla=0.25" },
        { "multibanda","Master 3 bandas", "cruceBajo=180;cruceAlto=3200;ratio=2.5;ataque=20;relajacion=200;umbralGrave=-24;umbralMedio=-22;umbralAgudo=-24" },
        { "anchura",   "Graves al centro","cruce=200;anchoGraves=0.2;anchoAgudos=1.15" },
    };
}

juce::var Motor::listarPresets (const juce::String& tipo)
{
    juce::Array<juce::var> lista;

    // La convolución no tiene presets: tiene respuestas de impulso. Las de
    // fábrica (sintéticas) y las que el usuario deje en su carpeta de IRs.
    if (tipo == ConvolucionPlugin::xmlTypeName)
    {
        for (const auto& archivo : carpetaIRsDeFabrica().findChildFiles (juce::File::findFiles, false, "*.wav"))
        {
            auto d = objeto();
            pon (d, "nombre", archivo.getFileNameWithoutExtension());
            pon (d, "fabrica", true);
            lista.add (d);
        }
        auto r = objeto();
        pon (r, "tipo", tipo);
        pon (r, "presets", lista);
        return r;
    }

    for (const auto& p : PRESETS_FABRICA)
        if (tipo == p.tipo)
        {
            auto d = objeto();
            pon (d, "nombre", p.nombre);
            pon (d, "fabrica", true);
            lista.add (d);
        }

    auto carpeta = carpetaPresets (tipo);
    for (const auto& archivo : carpeta.findChildFiles (juce::File::findFiles, false, "*.xml"))
    {
        auto d = objeto();
        pon (d, "nombre", archivo.getFileNameWithoutExtension());
        pon (d, "fabrica", false);
        lista.add (d);
    }

    auto r = objeto();
    pon (r, "tipo", tipo);
    pon (r, "presets", lista);
    return r;
}

juce::var Motor::guardarPreset (int indicePista, int indice, const juce::String& nombre)
{
    auto usuario = cadenaUsuario (indicePista);
    if (indice < 0 || indice >= usuario.size())
        throw std::runtime_error ("no existe el plugin");
    if (nombre.trim().isEmpty())
        throw std::runtime_error ("falta el nombre del preset");

    auto carpeta = carpetaPresets (usuario[indice]->getPluginType());
    carpeta.createDirectory();
    const auto archivo = carpeta.getChildFile (juce::File::createLegalFileName (nombre) + ".xml");

    if (auto xml = usuario[indice]->state.createXml())
        if (xml->writeTo (archivo))
            return juce::var (true);

    throw std::runtime_error ("no se pudo guardar el preset");
}

juce::var Motor::cargarPreset (int indicePista, int indice, const juce::String& nombre)
{
    auto usuario = cadenaUsuario (indicePista);
    if (indice < 0 || indice >= usuario.size())
        throw std::runtime_error ("no existe el plugin");

    auto* plugin = usuario[indice];
    edit->getUndoManager().beginNewTransaction ("cargar preset");

    // Convolución: el "preset" es la IR con ese nombre.
    if (auto* conv = dynamic_cast<ConvolucionPlugin*> (plugin))
    {
        const auto archivo = carpetaIRsDeFabrica().getChildFile (nombre + ".wav");
        if (! archivo.existsAsFile())
            throw std::runtime_error ("no existe la IR: " + nombre.toStdString());
        conv->rutaIR = archivo.getFullPathName();
        emitirModelo();
        return juce::var (true);
    }

    // Primero los de fábrica: parámetro=valor separados por punto y coma.
    for (const auto& p : PRESETS_FABRICA)
    {
        if (plugin->getPluginType() != p.tipo || nombre != p.nombre)
            continue;

        for (const auto& par : juce::StringArray::fromTokens (juce::String (p.parametros), ";", ""))
        {
            const auto id = par.upToFirstOccurrenceOf ("=", false, false);
            const auto valor = par.fromFirstOccurrenceOf ("=", false, false).getFloatValue();
            for (auto parametro : plugin->getAutomatableParameters())
                if (parametro->paramID == id)
                    parametro->setParameter (valor, juce::sendNotificationSync);
        }

        emitirModelo();
        return juce::var (true);
    }

    const auto archivo = carpetaPresets (plugin->getPluginType())
                           .getChildFile (juce::File::createLegalFileName (nombre) + ".xml");
    if (! archivo.existsAsFile())
        throw std::runtime_error ("no existe el preset: " + nombre.toStdString());

    if (auto xml = juce::parseXML (archivo))
    {
        plugin->restorePluginStateFromValueTree (juce::ValueTree::fromXml (*xml));
        emitirModelo();
        return juce::var (true);
    }

    throw std::runtime_error ("el preset no se pudo leer");
}

juce::var Motor::puntosAutomatizacion (const juce::var& params)
{
    asegurarEdit();

    const int indicePista = params.hasProperty ("pista") ? (int) params["pista"] : -1;
    const auto objetivo = params["parametro"].toString();

    te::AutomatableParameter::Ptr parametro;
    const bool esVolumen = objetivo == "volumen";

    if (esVolumen || objetivo == "pan")
    {
        te::VolumeAndPanPlugin* volumen = indicePista == -1 ? edit->getMasterVolumePlugin().get()
                                                            : (pista (indicePista) != nullptr ? pista (indicePista)->getVolumePlugin() : nullptr);
        if (volumen == nullptr)
            throw std::runtime_error ("no existe la pista");
        parametro = esVolumen ? volumen->volParam : volumen->panParam;
    }
    else
    {
        auto usuario = cadenaUsuario (indicePista);
        const int indice = params.hasProperty ("plugin") ? (int) params["plugin"] : -1;
        if (indice < 0 || indice >= usuario.size())
            throw std::runtime_error ("no existe el plugin");
        for (auto p : usuario[indice]->getAutomatableParameters())
            if (p->paramID == objetivo)
                parametro = p;
        if (parametro == nullptr)
            throw std::runtime_error ("no existe el parámetro: " + objetivo.toStdString());
    }

    edit->getUndoManager().beginNewTransaction ("automatizar");

    auto& curva = parametro->getCurve();
    curva.clear();

    if (auto* puntos = params["puntos"].getArray())
    {
        for (const auto& punto : *puntos)
        {
            float v = (float) (double) punto["v"];
            if (esVolumen)
                v = te::decibelsToVolumeFaderPosition (v);
            curva.addPoint (te::TimePosition::fromSeconds ((double) punto["t"]), v, 0.0f);
        }
    }

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
        pon (r, "grabando", transporte.isRecording());
        pon (r, "segundos", transporte.getPosition().inSeconds());
    }
    else
    {
        pon (r, "reproduciendo", false);
        pon (r, "grabando", false);
        pon (r, "segundos", 0.0);
    }

    return r;
}

juce::var Motor::grabar (const juce::var& params)
{
    asegurarEdit();

    const bool cuenta = params.hasProperty ("cuenta") && (bool) params["cuenta"];
    edit->setCountInMode (cuenta ? te::Edit::CountIn::oneBar : te::Edit::CountIn::none);

    // Armar una pista deja el rearmado del grafo EN DIFERIDO; si el punch
    // cae con el grafo en obras, la toma puede salir de un bloque. Aquí se
    // despacha lo pendiente antes de pinchar.
    edit->dispatchPendingUpdatesSynchronously();

    auto& transporte = edit->getTransport();
    transporte.ensureContextAllocated();
    transporte.record (false);
    return estadoTransporte();
}

juce::var Motor::tonoDePrueba (const juce::var& params)
{
    if (! opciones.sinAudio)
        throw std::runtime_error ("la señal de prueba solo existe en el modo sin audio");

    const double frecuencia = params.hasProperty ("frecuencia") ? (double) params["frecuencia"] : 0.0;
    tonoEntrada.store (frecuencia > 0.0 ? (float) juce::jlimit (20.0, 20000.0, frecuencia) : 0.0f);

    const int nota = params.hasProperty ("nota") ? (int) params["nota"] : -1;
    notaEntrada.store (nota >= 0 && nota <= 127 ? nota : -1);
    return juce::var (true);
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

namespace
{
    /** Sonoridad integrada de un archivo, con el mismo prefiltro K y las
        mismas puertas que el Medidor: el número que enseña es el que aplica. */
    double medirLufsArchivo (juce::AudioFormatManager& formatos, const juce::File& archivo)
    {
        std::unique_ptr<juce::AudioFormatReader> lector (formatos.createReaderFor (archivo));
        if (lector == nullptr)
            return -1000.0;

        const double fs = lector->sampleRate;
        const int canales = juce::jmin (2, (int) lector->numChannels);

        juce::dsp::IIR::Filter<float> k1[2], k2[2];
        auto shelf = juce::dsp::IIR::Coefficients<float>::makeHighShelf (fs, 1681.97, 0.7071752f,
                                                                         juce::Decibels::decibelsToGain (3.99966f));
        auto alto = juce::dsp::IIR::Coefficients<float>::makeHighPass (fs, 38.1354f, 0.5003270f);
        for (int c = 0; c < 2; ++c) { k1[c].coefficients = shelf; k2[c].coefficients = alto; }

        const int porBloque = (int) std::lround (fs / 10.0);
        std::vector<double> bloques100;
        juce::AudioBuffer<float> buffer (canales, 65536);
        double acumulada = 0.0; int acumuladas = 0;

        for (juce::int64 leidas = 0; leidas < lector->lengthInSamples;)
        {
            const int n = (int) juce::jmin ((juce::int64) buffer.getNumSamples(), lector->lengthInSamples - leidas);
            lector->read (&buffer, 0, n, leidas, true, true);

            for (int i = 0; i < n; ++i)
            {
                double energia = 0.0;
                for (int c = 0; c < canales; ++c)
                {
                    const float v = k2[c].processSample (k1[c].processSample (buffer.getSample (c, i)));
                    energia += (double) v * v;
                }
                acumulada += energia;
                if (++acumuladas >= porBloque)
                {
                    bloques100.push_back (acumulada / acumuladas);
                    acumulada = 0.0; acumuladas = 0;
                }
            }
            leidas += n;
        }

        // Bloques de 400 ms solapados al 75 %, puerta absoluta y relativa.
        auto aLufs = [] (double e) { return -0.691 + 10.0 * std::log10 (juce::jmax (1e-12, e)); };
        std::vector<double> ventanas;
        for (size_t i = 3; i < bloques100.size(); ++i)
        {
            const double media = (bloques100[i] + bloques100[i-1] + bloques100[i-2] + bloques100[i-3]) / 4.0;
            if (aLufs (media) > -70.0) ventanas.push_back (media);
        }
        if (ventanas.empty()) return -1000.0;

        double media = 0.0; for (auto e : ventanas) media += e; media /= (double) ventanas.size();
        const double umbral = aLufs (media) - 10.0;
        double suma = 0.0; size_t cuenta = 0;
        for (auto e : ventanas) if (aLufs (e) > umbral) { suma += e; cuenta += 1; }
        return cuenta > 0 ? aLufs (suma / (double) cuenta) : -1000.0;
    }

    /** Reescribe un WAV aplicando una ganancia, a 24 bits, con archivo temporal. */
    bool aplicarGananciaWav (juce::AudioFormatManager& formatos, const juce::File& archivo, float ganancia)
    {
        std::unique_ptr<juce::AudioFormatReader> lector (formatos.createReaderFor (archivo));
        if (lector == nullptr) return false;

        const auto temporal = archivo.getSiblingFile (".~" + archivo.getFileName());
        temporal.deleteFile();
        juce::WavAudioFormat wav;
        auto flujo = temporal.createOutputStream();
        if (flujo == nullptr) return false;
        std::unique_ptr<juce::AudioFormatWriter> escritor (
            wav.createWriterFor (flujo.release(), lector->sampleRate, lector->numChannels, 24, {}, 0));
        if (escritor == nullptr) return false;

        juce::AudioBuffer<float> buffer ((int) lector->numChannels, 65536);
        for (juce::int64 leidas = 0; leidas < lector->lengthInSamples;)
        {
            const int n = (int) juce::jmin ((juce::int64) buffer.getNumSamples(), lector->lengthInSamples - leidas);
            lector->read (&buffer, 0, n, leidas, true, true);
            buffer.applyGain (0, n, ganancia);
            escritor->writeFromAudioSampleBuffer (buffer, 0, n);
            leidas += n;
        }
        escritor.reset();
        lector.reset();
        return temporal.moveFileTo (archivo);
    }
}

juce::var Motor::exportar (const juce::String& ruta, bool stems, double lufsObjetivo)
{
    asegurarEdit();

    if (edit->getLength().inSeconds() <= 0.0)
        throw std::runtime_error ("el proyecto está vacío: nada que exportar");

    const juce::File destino (ruta);
    const auto extension = destino.getFileExtension().toLowerCase();
    if (extension != ".wav" && extension != ".flac")
        throw std::runtime_error ("la exportación es a WAV o FLAC");

    parar();

    // Render bloqueante en el hilo de mensajes: para lo que dura una canción,
    // más simple y más robusto que otra maquinaria de hilos. La interfaz ya
    // avisa de que está exportando.
    bool ok = true;
    juce::Array<juce::var> archivos;

    if (stems)
    {
        // Un stem por pista, en solo, con toda la cadena puesta. Los estados
        // de solo y mute se devuelven tal cual estaban.
        auto pistasAudio = te::getAudioTracks (*edit);
        std::vector<std::pair<bool, bool>> estados;
        for (auto p : pistasAudio) estados.push_back ({ p->isSolo (false), p->isMuted (false) });

        int numero = 1;
        for (int i = 0; i < pistasAudio.size(); ++i)
        {
            if (pistasAudio[i]->getClips().isEmpty() && cadenaUsuario (i).isEmpty())
                { numero += 1; continue; }

            for (int j = 0; j < pistasAudio.size(); ++j)
                pistasAudio[j]->setSolo (j == i);

            const auto nombreLimpio = pistasAudio[i]->getName().replaceCharacters ("/\\:*?\"<>|", "---------");
            auto archivo = destino.getSiblingFile (destino.getFileNameWithoutExtension()
                                                   + juce::String::formatted ("-%02d-", numero++) + nombreLimpio + ".wav");
            archivo.deleteFile();
            ok = te::Renderer::renderToFile (*edit, archivo, false) && archivo.existsAsFile() && ok;
            archivos.add (archivo.getFullPathName());
        }

        for (int j = 0; j < pistasAudio.size(); ++j)
        {
            pistasAudio[j]->setSolo (estados[(size_t) j].first);
            pistasAudio[j]->setMute (estados[(size_t) j].second);
        }
    }
    else
    {
        destino.deleteFile();
        const bool esFlac = extension == ".flac";
        const auto wavIntermedio = esFlac ? destino.getSiblingFile (".~render.wav") : destino;
        wavIntermedio.deleteFile();
        ok = te::Renderer::renderToFile (*edit, wavIntermedio, false) && wavIntermedio.existsAsFile();

        if (ok && esFlac)
        {
            std::unique_ptr<juce::AudioFormatReader> lector (formatos().createReaderFor (wavIntermedio));
            juce::FlacAudioFormat flac;
            auto flujo = destino.createOutputStream();
            if (lector != nullptr && flujo != nullptr)
            {
                std::unique_ptr<juce::AudioFormatWriter> escritor (
                    flac.createWriterFor (flujo.release(), lector->sampleRate, lector->numChannels, 24, {}, 5));
                ok = escritor != nullptr && escritor->writeFromAudioReader (*lector, 0, lector->lengthInSamples);
            }
            else
                ok = false;
            lector.reset();
            wavIntermedio.deleteFile();
        }

        archivos.add (destino.getFullPathName());

        if (ok && extension == ".wav" && lufsObjetivo > -100.0)
        {
            // Normalización de sonoridad: se mide con el mismo código que el
            // Medidor y se aplica la diferencia. Dos pasadas, cero sorpresas.
            const double medida = medirLufsArchivo (formatos(), destino);
            if (medida > -900.0)
            {
                const float ganancia = juce::Decibels::decibelsToGain ((float) (lufsObjetivo - medida));
                ok = aplicarGananciaWav (formatos(), destino, ganancia) && ok;
            }
        }
    }

    auto r = objeto();
    pon (r, "ruta", destino.getFullPathName());
    pon (r, "archivos", archivos);
    pon (r, "ok", ok);
    emitir (protocolo::evento ("render.terminado", r));

    if (! ok)
        throw std::runtime_error ("el render ha fallado");

    return r;
}

/* ============================================================= medidores */

void Motor::refrescarMedidoresDePista()
{
    if (edit == nullptr)
        return;

    // Los de grupo van aparte: crear una carpeta no cambia la lista de
    // pistas y el corte temprano de abajo se los comería.
    refrescarMedidoresDeGrupo();

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

void Motor::refrescarMedidoresDeGrupo()
{
    if (edit == nullptr)
        return;

    // El mismo baile que el de las pistas, sobre el VU de serie de cada
    // carpeta de submezcla (mismo orden que grupos[] del modelo).
    std::vector<te::LevelMeterPlugin*> actuales;
    for (auto* carpeta : te::getTracksOfType<te::FolderTrack> (*edit, true))
        actuales.push_back (carpeta->pluginList.findFirstPluginOfType<te::LevelMeterPlugin>());

    if (actuales == medidoresGrupo)
        return;

    for (size_t i = 0; i < medidoresGrupo.size(); ++i)
        if (medidoresGrupo[i] != nullptr)
            medidoresGrupo[i]->measurer.removeClient (*clientesGrupo[i]);

    medidoresGrupo = actuales;
    clientesGrupo.clear();

    for (auto* medidor : medidoresGrupo)
    {
        auto cliente = std::make_unique<te::LevelMeasurer::Client>();
        if (medidor != nullptr)
            medidor->measurer.addClient (*cliente);
        clientesGrupo.push_back (std::move (cliente));
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

    // Uso de CPU del hilo de audio (0..1), medido por JUCE en el callback:
    // también con la bomba interna, que empuja por el mismo camino.
    pon (datos, "cpu", engine.getDeviceManager().deviceManager.getCpuUsage());

    juce::Array<juce::var> porPista;
    for (auto& cliente : clientesPista)
    {
        auto p = objeto();
        pon (p, "izq", cliente->getAndClearAudioLevel (0).dB);
        pon (p, "der", cliente->getAndClearAudioLevel (1).dB);
        porPista.add (p);
    }
    pon (datos, "pistas", porPista);

    juce::Array<juce::var> porGrupo;
    for (auto& cliente : clientesGrupo)
    {
        auto g = objeto();
        pon (g, "izq", cliente->getAndClearAudioLevel (0).dB);
        pon (g, "der", cliente->getAndClearAudioLevel (1).dB);
        porGrupo.add (g);
    }
    pon (datos, "grupos", porGrupo);

    if (edit != nullptr)
        for (auto p : cadenaUsuario (-1))
            if (auto* medidor = dynamic_cast<MedidorPlugin*> (p))
            {
                const auto lectura = medidor->leer();
                auto l = objeto();
                pon (l, "pico", lectura.picoDb);
                pon (l, "picoVerdadero", lectura.picoVerdaderoDb);
                pon (l, "m", lectura.lufsM);
                pon (l, "s", lectura.lufsS);
                pon (l, "i", lectura.lufsI);
                pon (l, "lra", lectura.lra);
                pon (l, "correlacion", lectura.correlacion);
                pon (datos, "lufs", l);

                juce::Array<juce::var> bandasEspectro;
                for (auto v : lectura.espectro) bandasEspectro.add ((double) v);
                pon (datos, "espectro", bandasEspectro);

                juce::Array<juce::var> puntosXY;
                for (auto v : lectura.xy) puntosXY.add ((double) v);
                pon (datos, "xy", puntosXY);
            }

    // Los estados de lanzamiento cambian solos (en el límite de cuantización):
    // viajan con los medidores para que la rejilla de sesión respire en vivo.
    if (edit != nullptr && edit->getSceneList().getNumScenes() > 0)
    {
        juce::Array<juce::var> sesion;
        for (auto pista : te::getAudioTracks (*edit))
        {
            juce::Array<juce::var> fila;
            for (auto* ranura : pista->getClipSlotList().getClipSlots())
            {
                juce::String estadoRanura = "";
                if (auto* c = ranura->getClip())
                {
                    estadoRanura = "parado";
                    if (auto asa = c->getLaunchHandle())
                    {
                        if (asa->getQueuedStatus().has_value()) estadoRanura = "encolado";
                        else if (asa->getPlayingStatus() == te::LaunchHandle::PlayState::playing) estadoRanura = "tocando";
                    }
                }
                fila.add (estadoRanura);
            }
            sesion.add (juce::var (fila));
        }
        pon (datos, "sesion", juce::var (sesion));
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

        double faseEntrada = 0.0;
        int notaSonando = -1;
        juce::int64 cuentaNota = (juce::int64) 1e9;   // "lista": la primera nota no espera

        while (bombaViva.load())
        {
            buffer.clear();

            // La señal de prueba entra por donde entraría un micrófono: en el
            // buffer ANTES de processBlock, que lo lee como entrada.
            const float frecuenciaTono = tonoEntrada.load();
            if (frecuenciaTono > 0.0f)
            {
                const double paso = 2.0 * juce::MathConstants<double>::pi * frecuenciaTono / opciones.frecuencia;
                for (int i = 0; i < buffer.getNumSamples(); ++i)
                {
                    const float v = 0.25f * (float) std::sin (faseEntrada);
                    faseEntrada += paso;
                    if (faseEntrada > 2.0 * juce::MathConstants<double>::pi)
                        faseEntrada -= 2.0 * juce::MathConstants<double>::pi;
                    for (int c = 0; c < buffer.getNumChannels(); ++c)
                        buffer.setSample (c, i, v);
                }
            }

            midi.clear();

            // La nota de prueba: pulsos de 250 ms con su note-off, como si
            // alguien marcase corcheas en un teclado MIDI enchufado.
            {
                const int pedida = notaEntrada.load();
                const juce::int64 pulso = (juce::int64) (0.25 * opciones.frecuencia);

                if (notaSonando >= 0 && (pedida < 0 || cuentaNota >= pulso))
                {
                    midi.addEvent (juce::MidiMessage::noteOff (1, notaSonando), 0);
                    notaSonando = -1;
                    cuentaNota = 0;
                }
                else if (pedida >= 0 && notaSonando < 0 && cuentaNota >= pulso)
                {
                    notaSonando = pedida;
                    cuentaNota = 0;
                    midi.addEvent (juce::MidiMessage::noteOn (1, notaSonando, (juce::uint8) 100), 0);
                }
                cuentaNota += buffer.getNumSamples();
            }

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

/* ===================================================== humo de la suite */

namespace
{
    // Renders dorados de la suite: el RMS (canal izquierdo, dB) del render de
    // referencia de cada efecto con sus valores de fábrica, medido el día que
    // se dio por bueno. La tolerancia es generosa donde hay ruido o azar
    // (dither, cinta) y prieta donde el DSP es determinista.
    struct RenderDorado { const char* tipo; float rmsDb; float tolerancia; };
    constexpr RenderDorado RENDERS_DORADOS[] = {
        { "utilidad", -20.42f, 0.5f },
        { "compresor", -22.40f, 0.5f },
        { "techo", -20.43f, 0.5f },
        { "eqocho", -20.42f, 0.5f },
        { "medidor", -20.42f, 0.5f },
        { "placa", -22.97f, 0.9f },
        { "delay", -16.57f, 0.9f },
        { "puerta", -20.43f, 0.5f },
        { "multibanda", -20.43f, 0.5f },
        { "anchura", -21.16f, 0.5f },
        { "chispa", -20.46f, 0.9f },
        { "oxido", -18.47f, 0.9f },
        { "dither", -20.42f, 0.9f },
        { "oscilador", -24.01f, 0.5f },
        { "sala", -22.95f, 0.9f },
        { "pegamento", -23.24f, 0.5f },
        { "deeser", -20.42f, 0.5f },
        { "eqdinamico", -20.42f, 0.5f },
        { "balancin", -20.42f, 0.5f },
        { "convolucion", -23.70f, 0.9f },
        { "valvulas", -20.29f, 0.5f },
        { "consola", -20.42f, 0.5f },
        { "remache", -17.79f, 0.5f },
        { "opto", -16.42f, 0.5f },
        { "lampara", -18.85f, 0.5f },
        { "eco", -23.46f, 0.9f },
        { "muelle", -23.59f, 0.9f },
        { "espejismo", -24.64f, 0.9f },
        { "multitap", -24.16f, 0.9f },
        { "coro", -23.11f, 0.9f },
        { "tremolo", -23.13f, 0.5f },
        { "triodo", -17.43f, 0.5f },
        { "sumadora", -19.42f, 0.5f },
        { "machacadora", -20.43f, 0.5f },
        { "peine", -20.42f, 0.5f },
    };
}

int Motor::pruebaEfectos()
{
    auto carpeta = engine.getTemporaryFileManager().getTempDirectory();

    // Un segundo de material con enjundia: seno a 220 y a 3k, más un pelín de
    // ruido, a -12 dB. Con eso hasta un de-eser o una puerta tienen trabajo.
    auto wav = carpeta.getChildFile ("humo-suite.wav");
    wav.deleteFile();
    {
        juce::WavAudioFormat formato;
        auto flujo = wav.createOutputStream();
        if (flujo == nullptr) return 1;
        std::unique_ptr<juce::AudioFormatWriter> escritor (
            formato.createWriterFor (flujo.release(), 44100.0, 2, 16, {}, 0));
        if (escritor == nullptr) return 1;

        juce::AudioBuffer<float> b (2, 44100);
        juce::Random azar (42);
        for (int i = 0; i < 44100; ++i)
        {
            const double t = i / 44100.0;
            const float v = 0.18f * (float) std::sin (2.0 * juce::MathConstants<double>::pi * 220.0 * t)
                          + 0.06f * (float) std::sin (2.0 * juce::MathConstants<double>::pi * 3000.0 * t)
                          + 0.02f * (azar.nextFloat() * 2.0f - 1.0f);
            b.setSample (0, i, v);
            b.setSample (1, i, v * 0.9f);
        }
        escritor->writeFromAudioSampleBuffer (b, 0, 44100);
    }

    auto pausa = [] (int ms) { juce::MessageManager::getInstance()->runDispatchLoopUntil (ms); };

    int fallos = 0;
    for (auto tipo : TIPOS_SUITE)
    {
        const auto proyecto = carpeta.getChildFile ("humo-suite-proyecto");
        proyecto.deleteRecursively();
        nuevoProyecto (proyecto.getFullPathName());
        importarClip (0, wav.getFullPathName(), 0.0);
        insertarPlugin (-1, tipo, 0);
        pausa (60);

        const auto salida = carpeta.getChildFile ("humo-" + juce::String (tipo) + ".wav");
        exportar (salida.getFullPathName());

        float pico = -1.0f, rmsDb = -100.0f;
        bool finito = true;
        {
            std::unique_ptr<juce::AudioFormatReader> lector (formatos().createReaderFor (salida));
            if (lector != nullptr && lector->lengthInSamples > 0)
            {
                juce::AudioBuffer<float> b ((int) lector->numChannels,
                                            (int) juce::jmin ((juce::int64) 88200, lector->lengthInSamples));
                lector->read (&b, 0, b.getNumSamples(), 0, true, true);
                pico = b.getMagnitude (0, b.getNumSamples());
                rmsDb = juce::Decibels::gainToDecibels (b.getRMSLevel (0, 0, b.getNumSamples()), -100.0f);
                for (int c = 0; c < b.getNumChannels() && finito; ++c)
                    for (int i = 0; i < b.getNumSamples(); ++i)
                        if (! std::isfinite (b.getSample (c, i))) { finito = false; break; }
            }
        }

        // Los instrumentos callan sin MIDI: solo se les exige no romper nada.
        const bool instrumento = juce::String (tipo) == "bruma" || juce::String (tipo) == "cinta"
                              || juce::String (tipo) == "pads";
        bool bien = finito && pico < 4.0f && (instrumento || pico > 0.005f);

        // El render dorado: el RMS del render tiene que caer donde cayó el día
        // que se dio por bueno. Un cambio de sonido no pasa desapercibido: si
        // es deliberado, esta tabla se actualiza en el mismo commit y en paz.
        juce::String dorado = "sin dorado";
        for (const auto& d : RENDERS_DORADOS)
            if (juce::String (tipo) == d.tipo)
            {
                const float delta = rmsDb - d.rmsDb;
                dorado = "delta " + juce::String (delta, 2) + " dB";
                if (std::abs (delta) > d.tolerancia)
                {
                    dorado += " FUERA (esperado " + juce::String (d.rmsDb, 2)
                            + " +-" + juce::String (d.tolerancia, 2) + ")";
                    bien = false;
                }
                break;
            }

        std::cerr << (bien ? "  ok    " : "  MAL   ") << tipo << "  pico " << pico
                  << "  rms " << juce::String (rmsDb, 2) << " dB  [" << dorado << "]\n";
        if (! bien) ++fallos;
        adoptarEdit (nullptr, {});
    }

    std::cerr << (fallos == 0 ? "humo de la suite: todo en orden\n"
                              : "humo de la suite: efectos rotos\n");
    return fallos == 0 ? 0 : 1;
}

/* ================================================================ carga */

int Motor::pruebaCarga()
{
    auto carpeta = engine.getTemporaryFileManager().getTempDirectory();

    // Dos segundos de seno suave, compartidos por las cien pistas. A la
    // frecuencia del motor: aquí se mide la mezcla, no la cola de proxies.
    auto wav = carpeta.getChildFile ("carga.wav");
    wav.deleteFile();
    {
        juce::WavAudioFormat formato;
        auto flujo = wav.createOutputStream();
        if (flujo == nullptr) return 1;
        std::unique_ptr<juce::AudioFormatWriter> escritor (
            formato.createWriterFor (flujo.release(), opciones.frecuencia, 2, 16, {}, 0));
        if (escritor == nullptr) return 1;
        const int n = (int) (2.0 * opciones.frecuencia);
        juce::AudioBuffer<float> b (2, n);
        for (int i = 0; i < n; ++i)
        {
            const float v = 0.05f * (float) std::sin (2.0 * juce::MathConstants<double>::pi * 330.0 * i / opciones.frecuencia);
            b.setSample (0, i, v);
            b.setSample (1, i, v);
        }
        escritor->writeFromAudioSampleBuffer (b, 0, n);
    }

    auto pausa = [] (int ms) { juce::MessageManager::getInstance()->runDispatchLoopUntil (ms); };

    const auto proyecto = carpeta.getChildFile ("carga-proyecto");
    proyecto.deleteRecursively();
    nuevoProyecto (proyecto.getFullPathName());

    // Dentro de media/, como cualquier importación de verdad.
    const auto wavProyecto = carpetaMedia().getChildFile ("carga.wav");
    wav.copyFileTo (wavProyecto);
    wav = wavProyecto;

    constexpr int PISTAS = 100;
    const auto desdeCrear = juce::Time::getMillisecondCounterHiRes();
    while (te::getAudioTracks (*edit).size() < PISTAS)
        edit->insertNewAudioTrack (te::TrackInsertPoint (nullptr, te::getAudioTracks (*edit).getLast()), nullptr);
    for (int i = 0; i < PISTAS; ++i)
        if (auto* objetivo = pista (i))
            objetivo->insertWaveClip ("carga", wav,
                                      { { te::TimePosition(), te::TimePosition::fromSeconds (2.0) }, {} }, false);
    const double msCrear = juce::Time::getMillisecondCounterHiRes() - desdeCrear;

    // La foto del modelo con 100 pistas también tiene que salir con soltura.
    const auto desdeModelo = juce::Time::getMillisecondCounterHiRes();
    const auto modelo = listarPistas();
    const double msModelo = juce::Time::getMillisecondCounterHiRes() - desdeModelo;
    const int pistasModelo = (int) modelo["pistas"].size();

    // Sonar: tres segundos de reloj de pared; el transporte debe seguirlos.
    // Primero se espera al sonido: cien proxies tardan en arrancar.
    picoIzq.store (0.0f);
    picoDer.store (0.0f);
    tocar();
    for (int esperado = 0; esperado < 8000 && juce::jmax (picoIzq.load(), picoDer.load()) < 0.02f; esperado += 100)
        pausa (100);

    edit->getTransport().setPosition (te::TimePosition());
    pausa (300);

    // El temporizador de medidores pone los picos a cero 15 veces por segundo:
    // el máximo se acumula muestreando, no leyendo una vez al final.
    const double t0 = edit->getTransport().getPosition().inSeconds();
    const auto pared0 = juce::Time::getMillisecondCounterHiRes();
    float pico = 0.0f;
    while (juce::Time::getMillisecondCounterHiRes() - pared0 < 3000.0)
    {
        pausa (50);
        pico = juce::jmax (pico, picoIzq.load(), picoDer.load());
    }
    const double avanceTransporte = edit->getTransport().getPosition().inSeconds() - t0;
    const double avancePared = (juce::Time::getMillisecondCounterHiRes() - pared0) / 1000.0;
    parar();

    const bool alDia = avanceTransporte > avancePared * 0.9;
    const bool suena = pico > 0.02f;
    const bool modeloBien = pistasModelo == PISTAS && msModelo < 2000.0;
    const bool ok = alDia && suena && modeloBien;

    std::cerr << "carga: " << PISTAS << " pistas | crear " << (int) msCrear << " ms | modelo "
              << (int) msModelo << " ms | transporte " << avanceTransporte << " s en "
              << avancePared << " s de pared | pico " << pico << "\n"
              << (ok ? "carga: el motor aguanta\n" : "carga: NO aguanta\n");
    return ok ? 0 : 1;
}

/* ============================================================ hostilidad */

int Motor::pruebaProtocolo()
{
    // Cada línea hostil tiene que producir o bien una respuesta de error del
    // protocolo, o bien silencio (si ni id traía), pero jamás una excepción
    // sin atrapar ni un estado roto. Al final, una orden legítima confirma
    // que el motor sigue entero.
    const juce::String kilometrica = juce::String::repeatedString ("a", 300000);
    const juce::StringArray hostiles = {
        "",
        "esto no es json",
        "{",
        "[]",
        "42",
        "null",
        "{}",
        "{\"id\": 1}",
        "{\"metodo\": \"hola\"}",                                     // sin id: evento imposible, silencio
        "{\"id\": 2, \"metodo\": \"no.existe\"}",
        "{\"id\": 3, \"metodo\": \"\"}",
        "{\"id\": 4, \"metodo\": 42}",
        "{\"id\": 5, \"metodo\": \"pista.borrar\"}",                  // sin params
        "{\"id\": 6, \"metodo\": \"pista.borrar\", \"params\": {\"pista\": -7}}",
        "{\"id\": 7, \"metodo\": \"pista.borrar\", \"params\": {\"pista\": 99999}}",
        "{\"id\": 8, \"metodo\": \"clip.importar\", \"params\": {\"pista\": 0, \"ruta\": \"/no/existe.wav\", \"inicio\": 0}}",
        "{\"id\": 9, \"metodo\": \"clip.mover\", \"params\": {\"id\": \"nadie\", \"inicio\": -5}}",
        "{\"id\": 10, \"metodo\": \"clip.warp\", \"params\": {\"id\": \"nadie\"}}",
        "{\"id\": 11, \"metodo\": \"plugin.insertar\", \"params\": {\"pista\": 0, \"tipo\": \"troyano\"}}",
        "{\"id\": 12, \"metodo\": \"plugin.insertar\", \"params\": {\"pista\": 0, \"tipo\": \"vst:falso\"}}",
        "{\"id\": 13, \"metodo\": \"plugin.parametro\", \"params\": {\"pista\": 0, \"indice\": 55, \"parametro\": \"x\", \"valor\": 1e308}}",
        "{\"id\": 14, \"metodo\": \"plugin.lateral\", \"params\": {\"pista\": 0, \"indice\": 0, \"fuente\": 3}}",
        "{\"id\": 15, \"metodo\": \"transporte.tempo\", \"params\": {\"bpm\": -1}}",
        "{\"id\": 16, \"metodo\": \"transporte.tempo\", \"params\": {\"bpm\": 1e100}}",
        "{\"id\": 17, \"metodo\": \"transporte.irA\", \"params\": {\"segundos\": -1e18}}",
        "{\"id\": 18, \"metodo\": \"sesion.lanzar\", \"params\": {\"escena\": 99}}",
        "{\"id\": 19, \"metodo\": \"sesion.poner\", \"params\": {\"pista\": 0, \"escena\": 0, \"desdeClip\": \"nadie\"}}",
        "{\"id\": 20, \"metodo\": \"clip.midi.notas\", \"params\": {\"id\": \"nadie\", \"notas\": 7}}",
        "{\"id\": 21, \"metodo\": \"clip.midi.cuantizar\", \"params\": {\"id\": \"nadie\", \"division\": \"1/pi\"}}",
        "{\"id\": 22, \"metodo\": \"render.exportar\", \"params\": {\"ruta\": \"/carpeta/que/no/existe/x.wav\"}}",
        "{\"id\": 23, \"metodo\": \"previa.tocar\", \"params\": {\"ruta\": \"/no/existe.flac\"}}",
        "{\"id\": 24, \"metodo\": \"proyecto.abrir\", \"params\": {\"ruta\": \"" + kilometrica + "\"}}",
        "{\"id\": 25, \"metodo\": \"pista.renombrar\", \"params\": {\"pista\": 0, \"nombre\": \"" + kilometrica + "\"}}",
        "{\"id\": 26, \"metodo\": \"dispositivos.tono\", \"params\": {\"frecuencia\": \"hola\", \"nota\": 900}}",
        "{\"id\": 27, \"metodo\": \"pista.armar\", \"params\": {\"pista\": 0, \"activo\": true, \"entrada\": -99}}",
        "{\"id\": 28, \"metodo\": \"automatizacion.puntos\", \"params\": {\"pista\": 0, \"parametro\": \"fantasma\", \"puntos\": []}}",
        "{\"id\": 29, \"metodo\": \"grupo.crear\", \"params\": {\"pistas\": []}}",
        "{\"id\": 30, \"metodo\": \"grupo.crear\", \"params\": {\"pistas\": [0, 0, 99]}}",
        "{\"id\": 31, \"metodo\": \"grupo.meter\", \"params\": {\"pista\": 0, \"grupo\": 42}}",
        "{\"id\": 32, \"metodo\": \"grupo.deshacer\", \"params\": {\"grupo\": -3}}",
        "{\"id\": 33, \"metodo\": \"rack.crear\", \"params\": {\"pista\": 99}}",
        "{\"id\": 34, \"metodo\": \"rack.crear\", \"params\": {\"pista\": 0, \"desde\": 5, \"hasta\": 1}}",
        "{\"id\": 35, \"metodo\": \"rack.macro\", \"params\": {\"pista\": 0, \"indice\": 0, \"macro\": 99, \"valor\": 2}}",
        "{\"id\": 36, \"metodo\": \"rack.asignar\", \"params\": {\"pista\": 0, \"indice\": 0, \"macro\": 0, \"plugin\": 7, \"parametro\": \"nada\"}}",
        "{\"id\": 37, \"metodo\": \"pista.mezcla\", \"params\": {\"pista\": -42, \"volumenDb\": 0}}",
        "{\"id\": 38, \"metodo\": \"pista.mover\", \"params\": {\"pista\": 0, \"tras\": 99}}",
        "{\"id\": 39, \"metodo\": \"pista.mover\", \"params\": {\"pista\": 42}}",
        "{\"id\": 40, \"metodo\": \"rack.parametro\", \"params\": {\"pista\": 0, \"indice\": 0, \"plugin\": 9, \"parametro\": \"x\", \"valor\": 1}}",
    };

    int fallos = 0;
    for (const auto& linea : hostiles)
    {
        bool salir = false;
        juce::String respuesta;
        try
        {
            respuesta = protocolo::procesarLinea (*this, linea, salir);
        }
        catch (...)
        {
            std::cerr << "  MAL   excepción sin atrapar con: " << linea.substring (0, 80) << "\n";
            ++fallos;
            continue;
        }
        if (salir)
        {
            std::cerr << "  MAL   una línea hostil ha apagado el motor: " << linea.substring (0, 80) << "\n";
            ++fallos;
        }
    }

    // Y tras el chaparrón, la vida sigue: una orden legítima con respuesta sana.
    bool salir = false;
    const auto viva = protocolo::procesarLinea (*this, "{\"id\": 900, \"metodo\": \"pistas.listar\"}", salir);
    if (! viva.contains ("\"resultado\"") || viva.contains ("\"error\""))
    {
        std::cerr << "  MAL   el motor no responde sano tras la tormenta\n";
        ++fallos;
    }

    std::cerr << (fallos == 0 ? "hostilidad: el protocolo aguanta la basura\n"
                              : "hostilidad: el protocolo se rompe\n");
    return fallos == 0 ? 0 : 1;
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

    // Warp: el seno no tiene ritmo que detectar, así que el tempo de origen
    // se declara a mano (120), se enciende el autoTempo con transposición, y
    // el proyecto sube a 150: el clip debe encoger en la proporción 120/150.
    {
        auto peticionWarp = objeto();
        pon (peticionWarp, "bpmFuente", 120.0);
        pon (peticionWarp, "autoTempo", true);
        pon (peticionWarp, "transposicion", 5.0);
        warpClip (idClip, peticionWarp);
    }
    tempo (150.0);
    pausa (100);
    double duracionWarp = -1.0;
    {
        const auto trasWarp = listarPistas();
        for (const auto& c : *trasWarp["pistas"][0]["clips"].getArray())
            if ((bool) c["autoTempo"])
                duracionWarp = c["duracion"];
    }

    // La suite en el máster: mezcla y mastering al completo hasta la fecha.
    insertarPlugin (-1, "eqocho", 0);
    insertarPlugin (-1, "compresor", 1);
    insertarPlugin (-1, "puerta", 2);
    insertarPlugin (-1, "placa", 3);
    insertarPlugin (-1, "delay", 4);
    insertarPlugin (-1, "techo", 5);
    insertarPlugin (-1, "medidor", 6);
    parametroPlugin (-1, 5, "techo", -3.0);

    // Y dos clásicos en la primera pista, que suenan durante la reproducción.
    insertarPlugin (0, "valvulas", 0);
    insertarPlugin (0, "eco", 1);
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

    // Un envío al bus A (crea su retorno), una curva de volumen, y un preset.
    envioPista (0, 0, -12.0);
    {
        auto puntos = juce::var (juce::Array<juce::var>());
        auto p1 = objeto(); pon (p1, "t", 0.0); pon (p1, "v", 0.0);
        auto p2 = objeto(); pon (p2, "t", 1.0); pon (p2, "v", -18.0);
        puntos.getArray()->add (p1); puntos.getArray()->add (p2);
        auto peticion = objeto();
        pon (peticion, "pista", 0); pon (peticion, "parametro", "volumen"); pon (peticion, "puntos", puntos);
        puntosAutomatizacion (peticion);
    }
    cargarPreset (-1, 5, "Master -1 dB");
    guardarPreset (-1, 5, "prueba-mia");
    cargarPreset (-1, 5, "prueba-mia");

    // Exportar normalizado a -16 LUFS y comprobar que el archivo MIDE -16:
    // la validación de toda la cadena de sonoridad, de la medición al render.
    const auto normalizada = carpeta.getChildFile ("autoprueba-normalizada.wav");
    exportar (normalizada.getFullPathName(), false, -16.0);
    const double lufsMedida = medirLufsArchivo (formatos(), normalizada);

    // MIDI: un compás en la pista 4 con un arpegio de tres notas y la Bruma
    // delante para que aquello suene de verdad.
    const auto clipMidi = crearClipMidi (3, 0.0, 1.0);
    {
        auto lista = juce::var (juce::Array<juce::var>());
        const int arpegio[3] = { 60, 64, 67 };
        for (int i = 0; i < 3; ++i)
        {
            auto n = objeto();
            pon (n, "nota", arpegio[i]);
            pon (n, "inicio", i * 1.0);
            pon (n, "duracion", 0.9);
            pon (n, "velocidad", 100);
            lista.getArray()->add (n);
        }
        notasClipMidi (clipMidi["id"].toString(), lista);
    }
    insertarPlugin (3, "bruma", 0);

    // Guardar, reabrir, y que el proyecto vuelva entero.
    guardarProyecto();
    const auto rutaProyecto = carpetaProyecto.getFullPathName();
    adoptarEdit (nullptr, {});
    abrirProyecto (rutaProyecto);
    const auto reabierto = listarPistas();
    const int clipsReabiertos = (int) reabierto["pistas"][0]["clips"].size()
                              + (int) reabierto["pistas"][1]["clips"].size();
    const int pluginsMaster = (int) reabierto["master"]["plugins"].size();
    const int pluginsPista = (int) reabierto["pistas"][0]["plugins"].size();

    // El clip warpeado tiene que volver con su autoTempo, su transposición y
    // su tempo de origen intactos.
    double transposicionVuelta = 0.0, bpmVuelta = 0.0;
    for (const auto& c : *reabierto["pistas"][0]["clips"].getArray())
        if ((bool) c["autoTempo"])
        {
            transposicionVuelta = c["transposicion"];
            bpmVuelta = c["bpmFuente"];
        }

    // Y el clip MIDI, con sus tres notas y la Bruma en la cadena.
    int notasReabiertas = 0;
    for (const auto& c : *reabierto["pistas"][3]["clips"].getArray())
        if (c["tipo"].toString() == "midi")
            notasReabiertas += (int) c["notas"].size();
    const int pluginsPistaMidi = (int) reabierto["pistas"][3]["plugins"].size();

    // La pista 3 se arma para grabar YA, mucho antes del punch: el rearmado
    // del grafo que dispara el armado queda asentado durante el arpegio.
    armarPista (2, true, 0, false);

    // Que la Bruma SUENE: la pista 4 en solo, desde el principio, y el pico
    // del máster tiene que moverse con el arpegio.
    {
        auto soloPuesto = objeto(); pon (soloPuesto, "solo", true);
        mezclaPista (3, soloPuesto);
    }
    irA (0.0);
    picoIzq.store (0.0f);
    picoDer.store (0.0f);
    tocar();
    float picoMidi = 0.0f;
    for (int esperado = 0; esperado < 4000 && picoMidi < 0.03f; esperado += 100)
    {
        pausa (100);
        picoMidi = juce::jmax (picoIzq.load(), picoDer.load());
    }
    parar();
    {
        auto soloQuitado = objeto(); pon (soloQuitado, "solo", false);
        mezclaPista (3, soloQuitado);
    }

    // Grabar de verdad: tono de prueba en la entrada de la bomba, la pista 3
    // armada, y unas décimas de transporte en marcha. La toma tiene que
    // aparecer como clip con archivo y con el tono dentro. La espera es por
    // CONDICIÓN, no un sueño fijo: en un runner cargado, rearmar el grafo
    // tras el punch puede comerse una espera entera (0,02 s grabados de
    // 0,8) — se espera a que el transporte ruede grabando lo pedido.
    auto esperarGrabando = [this, &pausa] (double segundosRodando)
    {
        double inicioRodando = -1.0;
        for (int esperado = 0; esperado < 8000; esperado += 100)
        {
            pausa (100);
            auto& transporte = edit->getTransport();
            if (! transporte.isRecording())
                continue;
            const double ahora = transporte.getPosition().inSeconds();
            if (inicioRodando < 0.0)
                inicioRodando = ahora;
            else if (ahora - inicioRodando > segundosRodando)
                break;
        }
    };

    // En el runner de CI, el PRIMER punch tras armar pierde a veces el chorro
    // de entrada y la toma sale de un bloque (512 muestras con el tono dentro,
    // con el transporte rodando bien); el segundo punch de la misma sesión
    // nunca falla. Si la toma sale corta, se reintenta el punch entero: la
    // exigencia no cambia (una toma real de más de 0,4 s con el tono dentro).
    tonoEntrada.store (330.0f);
    double duracionGrabada = 0.0;
    float picoGrabado = 0.0f;
    for (int intento = 0; intento < 3 && duracionGrabada < 0.4; ++intento)
    {
        if (intento > 0)
        {
            std::cerr << "autoprueba: la toma salio corta (" << duracionGrabada << " s), reintento del punch\n";
            pausa (400);
        }

        grabar (objeto());
        esperarGrabando (0.6);
        parar();
        pausa (300);

        if (auto* pistaGrabada = pista (2))
            if (auto* toma = dynamic_cast<te::WaveAudioClip*> (pistaGrabada->getClips().getLast()))
            {
                duracionGrabada = toma->getPosition().getLength().inSeconds();
                std::unique_ptr<juce::AudioFormatReader> lector (formatos().createReaderFor (toma->getCurrentSourceFile()));
                if (lector != nullptr && lector->lengthInSamples > 0)
                {
                    juce::AudioBuffer<float> b ((int) lector->numChannels,
                                                (int) juce::jmin ((juce::int64) 96000, lector->lengthInSamples));
                    lector->read (&b, 0, b.getNumSamples(), 0, true, true);
                    picoGrabado = b.getMagnitude (0, b.getNumSamples());
                }
            }
    }
    tonoEntrada.store (0.0f);
    pausa (300);

    // Y grabar MIDI: la bomba marca la nota de prueba y la pista 4, armada
    // por su entrada MIDI, tiene que acabar con más notas de las que tenía.
    notaEntrada.store (62);
    armarPista (3, true, 0, true);
    grabar (objeto());
    esperarGrabando (1.0);
    parar();
    notaEntrada.store (-1);
    pausa (300);
    armarPista (3, false, 0, true);

    int notasTrasGrabar = 0;
    {
        const auto tras = listarPistas();
        for (const auto& c : *tras["pistas"][3]["clips"].getArray())
            if (c["tipo"].toString() == "midi")
                notasTrasGrabar += (int) c["notas"].size();
    }

    // La audición previa: con el transporte del proyecto PARADO, el archivo
    // suena por su edit aparte; al pararla, el silencio vuelve.
    picoIzq.store (0.0f);
    picoDer.store (0.0f);
    tocarPrevia (wav.getFullPathName());
    float picoPrevia = 0.0f;
    for (int esperado = 0; esperado < 5000 && picoPrevia < 0.03f; esperado += 100)
    {
        pausa (100);
        picoPrevia = juce::jmax (picoPrevia, picoIzq.load(), picoDer.load());
    }
    pararPrevia();
    pausa (200);

    // Session View: dos escenas, el clip warpeado a la primera ranura, y su
    // lanzamiento (sin cuantizar, que el reloj de la prueba no espera) tiene
    // que dejar la ranura en "tocando" con el transporte en marcha.
    escenasSesion (2);
    ponerEnSesion (0, 0, idClip);
    cuantizacionSesion ("None");
    lanzarSesion (0, 0);
    pausa (700);
    juce::String estadoRanura;
    {
        const auto m = listarPistas();
        estadoRanura = m["pistas"][0]["ranuras"][0]["estado"].toString();
    }
    pararSesion (-1);
    parar();
    pausa (200);

    const bool avanza = segundos > 0.25;
    const bool suena = pico > 0.05f;
    const bool deshace = clipsTrasDeshacer == 3;
    const bool renderiza = picoRender > 0.05f;
    // La cadena de la pista 0 trae Válvulas, Eco y el AuxSend del envío A.
    const bool persiste = clipsReabiertos == 3 && pluginsMaster == 7 && pluginsPista == 3;
    const int puntosCurva = (int) reabierto["pistas"][0]["automatizacionVolumen"].size();
    const bool automatiza = puntosCurva == 2;
    const bool normaliza = std::abs (lufsMedida - (-16.0)) < 1.5;
    const bool warpea = std::abs (duracionWarp - 0.4) < 0.02
                     && std::abs (transposicionVuelta - 5.0) < 0.01
                     && std::abs (bpmVuelta - 120.0) < 0.5;
    // El hosting VST3, si el CI compiló el plugin de prueba: escanearlo con
    // el proceso hijo, insertarlo (-6 dB exactos) y medir que el render baja
    // justo a la mitad. Sin la variable, la comprobación se salta y se dice.
    bool vst = true;
    juce::String vstDetalle = "saltado (sin PLETINA_VST3_PRUEBA)";
    if (const char* carpetaVst = std::getenv ("PLETINA_VST3_PRUEBA"))
    {
        auto peticion = objeto();
        auto rutas = juce::var (juce::Array<juce::var>());
        rutas.getArray()->add (juce::String::fromUTF8 (carpetaVst));
        pon (peticion, "rutas", rutas);
        carpetasVst (peticion);

        const auto escaneo = escanearVst();
        const auto catalogo = listaVst();
        vst = (int) escaneo["total"] >= 1 && catalogo["plugins"].size() >= 1;
        vstDetalle = "escaneados " + juce::String ((int) escaneo["total"]);

        if (vst)
        {
            // Proyecto limpio: el seno de -6 dB solo, render con y sin el VST.
            const auto proyectoVst = carpeta.getChildFile ("autoprueba-vst");
            proyectoVst.deleteRecursively();
            nuevoProyecto (proyectoVst.getFullPathName());
            importarClip (0, wav.getFullPathName(), 0.0);
            pausa (100);

            const auto renderSeco = carpeta.getChildFile ("autoprueba-vst-seco.wav");
            exportar (renderSeco.getFullPathName());
            const auto renderVst = carpeta.getChildFile ("autoprueba-vst-con.wav");
            insertarPlugin (0, "vst:" + catalogo["plugins"][0]["id"].toString(), 0);
            pausa (200);

            // Y que sobreviva al proyecto: guardar, reabrir y ENTONCES rendir.
            guardarProyecto();
            adoptarEdit (nullptr, {});
            abrirProyecto (proyectoVst.getFullPathName());
            pausa (200);
            exportar (renderVst.getFullPathName());

            auto pico = [this] (const juce::File& archivo)
            {
                std::unique_ptr<juce::AudioFormatReader> lector (formatos().createReaderFor (archivo));
                if (lector == nullptr || lector->lengthInSamples == 0) return 0.0f;
                juce::AudioBuffer<float> b ((int) lector->numChannels,
                                            (int) juce::jmin ((juce::int64) 96000, lector->lengthInSamples));
                lector->read (&b, 0, b.getNumSamples(), 0, true, true);
                return b.getMagnitude (0, b.getNumSamples());
            };

            const float seco = pico (renderSeco), procesado = pico (renderVst);
            vst = seco > 0.3f && std::abs (procesado - seco * 0.5f) < 0.03f;
            vstDetalle = juce::String (seco, 3) + " -> " + juce::String (procesado, 3);
        }
    }

    // Grupos y racks, en su propio proyecto. Grupos: agrupar dos pistas,
    // colgar una utilidad a -20 dB del bus (el render cae x0.1 exacto),
    // sobrevivir a guardar/reabrir y volver al nivel inicial al deshacer.
    // Racks: envolver 2 utilidades de -6 dB (render idéntico: el cableado en
    // serie es transparente), asignar la macro 0 a la ganancia con cantidad
    // 0.25 (la cuenta aditiva normalizada da +9 dB clavados con la macro de
    // fábrica en 0.5), apagar la macro (vuelta a la base) y deshacer el rack.
    const auto proyectoGrupos = carpeta.getChildFile ("autoprueba-grupos");
    proyectoGrupos.deleteRecursively();
    nuevoProyecto (proyectoGrupos.getFullPathName());
    importarClip (0, wav.getFullPathName(), 0.0);
    pausa (100);

    auto picoDe = [this] (const juce::File& archivo)
    {
        std::unique_ptr<juce::AudioFormatReader> lector (formatos().createReaderFor (archivo));
        if (lector == nullptr || lector->lengthInSamples == 0) return 0.0f;
        juce::AudioBuffer<float> b ((int) lector->numChannels,
                                    (int) juce::jmin ((juce::int64) 96000, lector->lengthInSamples));
        lector->read (&b, 0, b.getNumSamples(), 0, true, true);
        return b.getMagnitude (0, b.getNumSamples());
    };
    auto render = [this, &carpeta, &picoDe] (const char* nombre)
    {
        const auto archivo = carpeta.getChildFile (nombre);
        archivo.deleteFile();
        exportar (archivo.getFullPathName());
        return picoDe (archivo);
    };

    const float picoGrupo0 = render ("autoprueba-grupos-0.wav");

    {
        auto peticion = objeto();
        juce::Array<juce::var> miembros { 0, 1 };
        pon (peticion, "pistas", juce::var (miembros));
        pon (peticion, "nombre", "Bus");
        crearGrupo (peticion);
    }
    insertarPlugin (-2, "utilidad", 0);
    parametroPlugin (-2, 0, "ganancia", -20.0);
    pausa (200);
    const float picoGrupo1 = render ("autoprueba-grupos-1.wav");

    bool grupoModelo;
    {
        const auto m = listarPistas();
        grupoModelo = m["grupos"].size() == 1
                   && m["grupos"][0]["pistas"].size() == 2
                   && m["pistas"].size() == 4
                   && (int) m["pistas"][0]["grupo"] == 0;
    }

    guardarProyecto();
    adoptarEdit (nullptr, {});
    abrirProyecto (proyectoGrupos.getFullPathName());
    pausa (200);
    bool grupoPersiste;
    {
        const auto m = listarPistas();
        grupoPersiste = m["grupos"].size() == 1
                     && m["grupos"][0]["pistas"].size() == 2
                     && m["grupos"][0]["plugins"].size() == 1;
    }

    deshacerGrupo (0);
    pausa (200);
    const float picoGrupo2 = render ("autoprueba-grupos-2.wav");
    bool grupoSuelto;
    {
        const auto m = listarPistas();
        grupoSuelto = m["grupos"].size() == 0 && m["pistas"].size() == 4;
    }

    const bool agrupa = grupoModelo && grupoSuelto
                     && picoGrupo0 > 0.3f
                     && std::abs (picoGrupo1 - picoGrupo0 * 0.1f) < 0.005f
                     && std::abs (picoGrupo2 - picoGrupo0) < 0.005f;

    // Reordenar: C al principio, C de vuelta tras B, y al mover una pista
    // detrás de una agrupada tiene que ADOPTAR su grupo.
    renombrarPista (0, "A");
    renombrarPista (1, "B");
    renombrarPista (2, "C");
    moverPista (2, -1);
    bool reordena;
    {
        const auto m = listarPistas();
        reordena = m["pistas"][0]["nombre"].toString() == "C"
                && m["pistas"][1]["nombre"].toString() == "A"
                && m["pistas"][2]["nombre"].toString() == "B";
    }
    moverPista (0, 2);
    {
        const auto m = listarPistas();
        reordena = reordena
                && m["pistas"][0]["nombre"].toString() == "A"
                && m["pistas"][1]["nombre"].toString() == "B"
                && m["pistas"][2]["nombre"].toString() == "C";
    }
    {
        auto peticion = objeto();
        juce::Array<juce::var> miembros { 0, 1 };
        pon (peticion, "pistas", juce::var (miembros));
        crearGrupo (peticion);
    }
    moverPista (2, 0);
    {
        const auto m = listarPistas();
        reordena = reordena
                && m["grupos"][0]["pistas"].size() == 3
                && (int) m["pistas"][1]["grupo"] == 0
                && m["pistas"][1]["nombre"].toString() == "C";
    }
    deshacerGrupo (0);
    pausa (100);

    insertarPlugin (0, "utilidad", -1);
    insertarPlugin (0, "utilidad", -1);
    parametroPlugin (0, 0, "ganancia", -6.0);
    parametroPlugin (0, 1, "ganancia", -6.0);
    pausa (200);
    const float picoRack0 = render ("autoprueba-racks-0.wav");

    crearRack (0, -1, -1, {});
    pausa (200);
    const float picoRack1 = render ("autoprueba-racks-1.wav");
    bool rackModelo;
    {
        const auto m = listarPistas();
        const auto pl = m["pistas"][0]["plugins"];
        rackModelo = pl.size() == 1
                  && pl[0]["tipo"].toString() == "rack"
                  && pl[0]["macros"].size() == 8
                  && pl[0]["cadena"].size() == 2;
    }

    // Editar DENTRO del rack: la segunda utilidad sube de -6 a 0 dB (el
    // render gana esos 6 dB exactos) y vuelve a su sitio.
    parametroRack (0, 0, 1, "ganancia", 0.0);
    pausa (200);
    const float picoRackParam = render ("autoprueba-racks-p.wav");
    parametroRack (0, 0, 1, "ganancia", -6.0);
    pausa (200);

    {
        auto peticion = objeto();
        pon (peticion, "plugin", 0);
        pon (peticion, "parametro", "ganancia");
        pon (peticion, "cantidad", 0.25);
        asignarMacroRack (0, 0, 0, peticion);
    }
    pausa (200);
    const float picoRack2 = render ("autoprueba-racks-2.wav");

    {
        auto peticion = objeto();
        pon (peticion, "valor", 0.0);
        macroRack (0, 0, 0, peticion);
    }
    pausa (200);
    const float picoRack3 = render ("autoprueba-racks-3.wav");

    guardarProyecto();
    adoptarEdit (nullptr, {});
    abrirProyecto (proyectoGrupos.getFullPathName());
    pausa (200);
    bool rackPersiste;
    {
        const auto m = listarPistas();
        const auto pl = m["pistas"][0]["plugins"];
        rackPersiste = pl.size() == 1
                    && pl[0]["tipo"].toString() == "rack"
                    && pl[0]["macros"].size() == 8
                    && pl[0]["macros"][0]["asignaciones"].size() == 1
                    && pl[0]["macros"][0]["asignaciones"][0]["parametro"].toString() == "ganancia"
                    && std::abs ((double) pl[0]["macros"][0]["valor"]) < 0.001;
    }

    deshacerRack (0, 0);
    pausa (200);
    const float picoRack4 = render ("autoprueba-racks-4.wav");
    bool rackEnLinea;
    {
        const auto m = listarPistas();
        const auto pl = m["pistas"][0]["plugins"];
        rackEnLinea = pl.size() == 2
                   && pl[0]["tipo"].toString() == "utilidad"
                   && pl[1]["tipo"].toString() == "utilidad";
    }

    const bool enracka = rackModelo && picoRack0 > 0.05f
                      && std::abs (picoRack1 - picoRack0) < 0.005f;
    const bool rackParametro = std::abs (picoRackParam - picoRack0 * 1.9953f) < 0.02f;
    const bool macroMueve = std::abs (picoRack2 - picoRack0 * 2.8184f) < 0.02f
                         && std::abs (picoRack3 - picoRack0) < 0.005f;
    const bool rackDeshecho = rackEnLinea && std::abs (picoRack4 - picoRack0) < 0.005f;

    const bool graba = duracionGrabada > 0.4 && picoGrabado > 0.15f;
    const bool midiSuena = notasReabiertas == 3 && pluginsPistaMidi == 1 && picoMidi > 0.03f;
    const bool midiGraba = notasTrasGrabar > notasReabiertas;
    const bool sesion = estadoRanura == "tocando";
    const bool previa = picoPrevia > 0.03f;
    const bool ok = avanza && suena && deshace && renderiza && persiste && automatiza && normaliza && warpea
                 && graba && midiSuena && midiGraba && sesion && previa && vst
                 && agrupa && grupoPersiste && enracka && macroMueve && rackPersiste && rackDeshecho
                 && reordena && rackParametro;

    auto r = objeto();
    pon (r, "ok", ok);
    pon (r, "segundos", segundos);
    pon (r, "pico", pico);
    pon (r, "clipsTrasDeshacer", clipsTrasDeshacer);
    pon (r, "picoRender", picoRender);
    pon (r, "clipsReabiertos", clipsReabiertos);
    pon (r, "pluginsMaster", pluginsMaster);
    pon (r, "pluginsPista", pluginsPista);
    pon (r, "puntosCurva", puntosCurva);
    pon (r, "lufsNormalizada", lufsMedida);
    pon (r, "duracionWarp", duracionWarp);
    pon (r, "transposicionVuelta", transposicionVuelta);
    pon (r, "bpmFuenteVuelta", bpmVuelta);
    pon (r, "duracionGrabada", duracionGrabada);
    pon (r, "picoGrabado", picoGrabado);
    pon (r, "notasReabiertas", notasReabiertas);
    pon (r, "pluginsPistaMidi", pluginsPistaMidi);
    pon (r, "picoMidi", picoMidi);
    pon (r, "notasTrasGrabar", notasTrasGrabar);
    pon (r, "ranuraLanzada", estadoRanura);
    pon (r, "picoPrevia", picoPrevia);
    pon (r, "vst", vstDetalle);
    pon (r, "agrupa", agrupa);
    pon (r, "grupoPersiste", grupoPersiste);
    pon (r, "picoGrupo", juce::String (picoGrupo0, 3) + " -> " + juce::String (picoGrupo1, 3) + " -> " + juce::String (picoGrupo2, 3));
    pon (r, "enracka", enracka);
    pon (r, "macroMueve", macroMueve);
    pon (r, "rackPersiste", rackPersiste);
    pon (r, "rackDeshecho", rackDeshecho);
    pon (r, "picoRack", juce::String (picoRack0, 3) + " -> " + juce::String (picoRack1, 3) + " -> " + juce::String (picoRack2, 3) + " -> " + juce::String (picoRack3, 3) + " -> " + juce::String (picoRack4, 3));
    pon (r, "reordena", reordena);
    pon (r, "rackParametro", rackParametro);
    pon (r, "picoRackParam", picoRackParam);
    emitir (protocolo::evento ("prueba", r));

    return ok ? 0 : 1;
}
