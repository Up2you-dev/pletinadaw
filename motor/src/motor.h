/*  Pletina DAW · motor
    Copyright (C) 2026 Up2you — GPLv3: ver el LICENSE de la raíz.

    Envoltorio de Tracktion Engine: el proyecto (Edit), sus pistas y clips,
    el transporte, la mezcla, la suite de efectos y los medidores, con dos
    modos de audio. Con dispositivo real, JUCE abre lo que haya (WASAPI/ASIO
    en Windows, ALSA en Linux). Con --sin-audio se usa la interfaz hospedada
    de T.E. y una bomba propia que empuja bloques a ritmo de reloj: así el
    transporte avanza y los medidores miden aunque no exista tarjeta de
    sonido, que es el caso del CI y de los contenedores.

    Tras cada orden que muta el proyecto se emite el evento `modelo` con la
    foto completa: la interfaz no adivina, replica.
*/

#pragma once

#include <tracktion_engine/tracktion_engine.h>

#include <atomic>
#include <functional>
#include <thread>

namespace te = tracktion;

class MedidorPlugin;

/** Los límites de fábrica de T.E. se quedan cortos para un DAW de verdad
    (4 plugins en el máster contando el volumen y el VU de serie): aquí se
    ensanchan a algo que nadie debería rozar. */
struct ComportamientoPletina : public te::EngineBehaviour
{
    te::EditLimits getEditLimits() override
    {
        return { 400, 3000, 16, 64, 64 };
    }
};

class Motor : private juce::Timer
{
public:
    struct Opciones
    {
        bool sinAudio = false;
        double frecuencia = 48000.0;
        int bloque = 512;
    };

    // emitir recibe líneas ya formateadas (eventos NDJSON) listas para stdout.
    Motor (Opciones, std::function<void (const juce::String&)> emitir);
    ~Motor() override;

    // Órdenes del protocolo. Todas se llaman desde el hilo de mensajes.
    juce::var hola() const;
    juce::var listarDispositivos();
    juce::var tonoDePrueba (const juce::var& params);   // solo --sin-audio

    // VST3: carpetas de búsqueda, escaneo (en procesos hijo) y catálogo.
    juce::var carpetasVst (const juce::var& params);
    juce::var escanearVst();
    juce::var listaVst() const;

    // Audición previa: un archivo suena en su edit aparte, sin rozar el proyecto.
    juce::var tocarPrevia (const juce::String& ruta);
    juce::var pararPrevia();

    // Proyecto: una carpeta con proyecto.tracktionedit y media/.
    juce::var nuevoProyecto (const juce::String& carpeta);
    juce::var abrirProyecto (const juce::String& carpeta);
    juce::var guardarProyecto();
    juce::var listarPistas();

    // Pistas.
    juce::var crearPista();
    juce::var borrarPista (int indice);
    juce::var renombrarPista (int indice, const juce::String& nombre);
    juce::var mezclaPista (int indice, const juce::var& params);
    juce::var envioPista (int indice, int bus, double nivelDb);
    juce::var congelarPista (int indice, bool activo);
    juce::var armarPista (int indice, bool activo, int entrada, bool midi);

    // Clips.
    juce::var importarClip (int pista, const juce::String& ruta, double inicio);
    juce::var moverClip (const juce::String& id, double inicio, int pista);
    juce::var recortarClip (const juce::String& id, double inicio, double fin);
    juce::var dividirClip (const juce::String& id, double segundos);
    juce::var duplicarClip (const juce::String& id);
    juce::var fundidosClip (const juce::String& id, double entrada, double salida);
    juce::var borrarClip (const juce::String& id);
    juce::var picosClip (const juce::String& id, int porSegundo);
    juce::var warpClip (const juce::String& id, const juce::var& params);
    double detectarBpm (const juce::File& archivo);

    // Clips MIDI: el respaldo del piano roll.
    juce::var crearClipMidi (int pista, double inicio, double compases);
    juce::var notasClipMidi (const juce::String& id, const juce::var& notas);
    juce::var cuantizarClipMidi (const juce::String& id, const juce::String& division);

    // Session View: escenas × pistas con lanzamiento cuantizado.
    juce::var escenasSesion (int numero);
    juce::var ponerEnSesion (int pista, int escena, const juce::String& desdeClip);
    juce::var lanzarSesion (int pista, int escena);      // pista -1 = la escena entera
    juce::var pararSesion (int pista);                   // -1 = todas
    juce::var cuantizacionSesion (const juce::String& nombre);

    // Cadenas de la suite (pista -1 = máster).
    juce::var insertarPlugin (int pista, const juce::String& tipo, int indice);
    juce::var quitarPlugin (int pista, int indice);
    juce::var parametroPlugin (int pista, int indice, const juce::String& parametro, double valor);
    juce::var activarPlugin (int pista, int indice, bool activo);
    juce::var lateralPlugin (int pista, int indice, int fuente);   // fuente -1 = quitar
    juce::var listarPresets (const juce::String& tipo);
    juce::var guardarPreset (int pista, int indice, const juce::String& nombre);
    juce::var cargarPreset (int pista, int indice, const juce::String& nombre);

    // Automatización: sustituye entera la curva de un parámetro.
    // params: {pista, parametro: "volumen"|"pan"|id, plugin?: indice, puntos: [{t, v}...]}
    juce::var puntosAutomatizacion (const juce::var& params);

    // Transporte y compañía.
    juce::var tocar();
    juce::var grabar (const juce::var& params);
    juce::var parar();
    juce::var irA (double segundos);
    juce::var estadoTransporte() const;
    juce::var tempo (double bpm);
    juce::var metronomo (bool activo);
    juce::var bucle (bool activo, double inicio, double fin);

    juce::var deshacer();
    juce::var rehacer();

    // Render offline a WAV: máster o stems por pista, con normalización de
    // sonoridad opcional. Bloquea el hilo de mensajes lo que dure.
    juce::var exportar (const juce::String& ruta, bool stems = false, double lufsObjetivo = -1000.0);

    // Autoprueba para CI y contenedores: reproduce, edita, exporta y verifica.
    int autoprueba();

    // Humo de la suite entera: cada efecto renderiza un segundo de material y
    // no puede salir mudo, desbocado ni con NaN. (Los renders dorados con
    // tolerancia fina siguen pendientes; esto caza lo gordo en cada commit.)
    int pruebaEfectos();

    // Carga: 100 pistas con clip y mezcla sonando a la vez; falla si el
    // motor no aguanta el ritmo del reloj o el modelo revienta.
    int pruebaCarga();

    // Hostilidad: basura por el protocolo (JSON roto, métodos falsos, índices
    // imposibles, cadenas kilométricas). Todo debe responderse con un error
    // limpio y el motor debe seguir vivo y cuerdo al final.
    int pruebaProtocolo();

private:
    void timerCallback() override;
    void asegurarEdit();
    void adoptarEdit (std::unique_ptr<te::Edit>, const juce::File& carpeta);
    void arrancarBomba();
    void pararBomba();
    void emitirModelo();

    te::AudioTrack* pista (int indice) const;
    te::Clip* clip (const juce::String& id) const;
    juce::StringArray rutasVst() const;
    void guardarCatalogoVst();
    te::PluginList* cadena (int indice) const;      // -1 = máster
    juce::Array<te::Plugin*> cadenaUsuario (int indice) const;
    juce::File carpetaMedia() const;
    void refrescarMedidoresDePista();

    Opciones opciones;
    std::function<void (const juce::String&)> emitir;

    te::Engine engine { std::make_unique<te::PropertyStorage> ("PletinaMotor"),
                        std::make_unique<te::UIBehaviour>(),
                        std::make_unique<ComportamientoPletina>() };
    std::unique_ptr<te::Edit> edit;
    std::unique_ptr<te::Edit> editPrevia;           // el tocadiscos de la audición
    juce::File carpetaProyecto;                     // vacía = proyecto temporal

    // Medidores: máster (F0) y uno por pista (F1), leídos por el timer.
    te::LevelMeterPlugin* medidorMaestro = nullptr;
    te::LevelMeasurer::Client clienteMaestro;
    std::vector<std::unique_ptr<te::LevelMeasurer::Client>> clientesPista;
    std::vector<te::LevelMeterPlugin*> medidoresPista;

    // Bomba del modo sin audio.
    std::thread bomba;
    std::atomic<bool> bombaViva { false };
    std::atomic<float> picoIzq { 0.0f }, picoDer { 0.0f };

    // Señal de prueba en la ENTRADA de la bomba (Hz; 0 = silencio): con ella
    // la autoprueba y el CI graban de verdad sin micrófono. dispositivos.tono.
    std::atomic<float> tonoEntrada { 0.0f };

    // Nota de prueba en la entrada MIDI de la bomba (-1 = ninguna): la bomba
    // la toca a corcheas para que la grabación MIDI también se pruebe sola.
    std::atomic<int> notaEntrada { -1 };

    bool reproduciendoAntes = false;
    int tics = 0; // del temporizador de 15 Hz: cada ~2 min toca autoguardado

    JUCE_DECLARE_NON_COPYABLE (Motor)
};
