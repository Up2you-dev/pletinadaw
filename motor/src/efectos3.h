/*  Pletina DAW · motor
    Copyright (C) 2026 Up2you — GPLv3: ver el LICENSE de la raíz.

    Tercera ola (F3): los clásicos. Emulaciones del comportamiento que hizo
    célebre a cada aparato — no clones circuit-exact, sí sus curvas, sus
    tiempos y su carácter. Para no repetir el mismo altar de parámetros
    quince veces, la base PluginSuite lee una tabla estática por plugin.
*/

#pragma once

#include "efectos.h"

struct EspecParametro
{
    const char* id;
    const char* nombre;   // UTF-8
    float minimo, maximo, defecto;
    float salto = 0.0f, sesgo = 0.0f;
};

/** Base CRTP: el derivado declara `PARAMETROS`, `xmlTypeName`, `NOMBRE` y su
    DSP; la base fabrica CachedValues y AutomatableParameters de la tabla. */
template <typename Derivado>
class PluginSuite : public te::Plugin
{
public:
    explicit PluginSuite (te::PluginCreationInfo info) : te::Plugin (info)
    {
        auto um = getUndoManager();

        for (const auto& espec : Derivado::PARAMETROS)
        {
            auto cache = std::make_unique<juce::CachedValue<float>> ();
            cache->referTo (state, juce::Identifier (espec.id), um, espec.defecto);

            auto parametro = addParam (espec.id, juce::String::fromUTF8 (espec.nombre),
                                       { espec.minimo, espec.maximo, espec.salto, espec.sesgo == 0.0f ? 1.0f : espec.sesgo });
            parametro->attachToCurrentValue (*cache);

            caches.push_back (std::move (cache));
            parametros.push_back (parametro);
        }
    }

    ~PluginSuite() override
    {
        notifyListenersOfDeletion();
        for (auto& parametro : parametros)
            parametro->detachFromCurrentValue();
    }

    juce::String getName() const override { return juce::String::fromUTF8 (Derivado::NOMBRE); }
    juce::String getPluginType() override { return Derivado::xmlTypeName; }
    juce::String getSelectableDescription() override { return getName(); }
    void deinitialise() override {}

    void restorePluginStateFromValueTree (const juce::ValueTree& v) override
    {
        auto um = getUndoManager();
        for (auto& cache : caches)
            if (v.hasProperty (cache->getPropertyID()))
                cache->setValue ((float) (double) v[cache->getPropertyID()], um);
        for (auto p : getAutomatableParameters())
            p->updateFromAttachedValue();
    }

protected:
    /** Valor actual del parámetro i, en el orden de la tabla. */
    float P (int i) const { return parametros[(size_t) i]->getCurrentValue(); }

    std::vector<std::unique_ptr<juce::CachedValue<float>>> caches;
    std::vector<te::AutomatableParameter::Ptr> parametros;
};

// El patrón de cada clásico: tabla + estado DSP + initialise/applyToBuffer.
#define CLASICO(Clase, Tipo)                                        \
    class Clase : public PluginSuite<Clase>                         \
    {                                                               \
    public:                                                         \
        static const char* xmlTypeName;                             \
        static const char* NOMBRE;                                  \
        static const std::vector<EspecParametro> PARAMETROS;        \
        static const char* getPluginName() { return NOMBRE; }       \
        using PluginSuite::PluginSuite;                             \
        void initialise (const te::PluginInitialisationInfo&) override; \
        void applyToBuffer (const te::PluginRenderContext&) override;

CLASICO (ValvulasPlugin, "valvulas")
    private:
        double fs = 48000.0;
        juce::dsp::IIR::Filter<float> filtros[4][2];
        float cache[7] = { -1e9f };
    };

CLASICO (ConsolaPlugin, "consola")
    private:
        double fs = 48000.0;
        juce::dsp::IIR::Filter<float> filtros[5][2];
        float cache[9] = { -1e9f };
    };

CLASICO (RemachePlugin, "remache")
    private:
        double fs = 48000.0;
        float envolvente = 0.0f;
    };

CLASICO (OptoPlugin, "opto")
    private:
        double fs = 48000.0;
        float envolvente = 0.0f, celula = 0.0f;
    };

CLASICO (LamparaPlugin, "lampara")
    private:
        double fs = 48000.0;
        float envolvente = 0.0f;
    };

CLASICO (EcoPlugin, "eco")
    private:
        double fs = 48000.0;
        std::vector<float> linea[2];
        int pos = 0;
        double fases[2] = {};
        juce::dsp::IIR::Filter<float> tono[2], bump[2];
        float retardoSuavizado = 0.0f;
        float cacheTono = -1e9f;
    };

CLASICO (MuellePlugin, "muelle")
    private:
        double fs = 48000.0;
        std::vector<float> dispersion[6];  // allpasses en serie
        int posD[6] = {};
        std::vector<float> lazo[2];
        int posLazo = 0;
        juce::dsp::IIR::Filter<float> brillo[2];
        float cacheBrillo = -1e9f;
    };

CLASICO (EspejismoPlugin, "espejismo")
    private:
        double fs = 48000.0;
        std::vector<float> lineas[8];
        int posLinea[8] = {};
        float pasoBajo[8] = {};
        std::vector<float> octavador;     // lectura a 2× para el brillo
        int posOct = 0;
        double lectura = 0.0;
    };

CLASICO (MultitapPlugin, "multitap")
    private:
        double fs = 48000.0;
        std::vector<float> linea[2];
        int pos = 0;
    };

CLASICO (CoroPlugin, "coro")
    private:
        double fs = 48000.0;
        std::vector<float> linea[2];
        int pos = 0;
        double fase = 0.0;
        float allpass[4][2] = {};         // etapas del modo fase
        float re[2] = {};
    };

CLASICO (TremoloPlugin, "tremolo")
    private:
        double fs = 48000.0;
        double fase = 0.0;
    };

CLASICO (TriodoPlugin, "triodo")
    private:
        double fs = 48000.0;
        juce::dsp::IIR::Filter<float> tono[2];
        float cacheTono = -1e9f;
    };

CLASICO (SumadoraPlugin, "sumadora")
    private:
        double fs = 48000.0;
    };

CLASICO (MachacadoraPlugin, "machacadora")
    private:
        double fs = 48000.0;
        float retenida[2] = {};
        int contador = 0;
    };

CLASICO (PeinePlugin, "peine")
    private:
        double fs = 48000.0;
        juce::dsp::IIR::Filter<float> filtros[31][2];
        float cache[31] = { -1e9f };
    };

#undef CLASICO

/** Registra los quince clásicos en el motor. La llama registrarEfectos. */
void registrarClasicos (te::Engine& engine);
