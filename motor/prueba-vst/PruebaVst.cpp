/*  Pletina DAW · motor
    Copyright (C) 2026 Up2you — GPLv3: ver el LICENSE de la raíz.

    El VST3 de prueba del hosting: un plugin de verdad, empaquetado como
    cualquier tercero, que atenúa exactamente 6 dB. La autoprueba lo escanea
    con el proceso hijo, lo inserta y comprueba que el render sale a la
    mitad: si eso pasa, el hosting existe de punta a punta. No se distribuye:
    solo se compila con -DPLETINA_VST_PRUEBA=ON.
*/

#include <juce_audio_processors/juce_audio_processors.h>

class ProcesadorPrueba final : public juce::AudioProcessor
{
public:
    ProcesadorPrueba()
        : juce::AudioProcessor (BusesProperties()
                                    .withInput ("Entrada", juce::AudioChannelSet::stereo(), true)
                                    .withOutput ("Salida", juce::AudioChannelSet::stereo(), true))
    {
    }

    const juce::String getName() const override            { return "Pletina Prueba"; }
    bool acceptsMidi() const override                      { return false; }
    bool producesMidi() const override                     { return false; }
    double getTailLengthSeconds() const override           { return 0.0; }

    int getNumPrograms() override                          { return 1; }
    int getCurrentProgram() override                       { return 0; }
    void setCurrentProgram (int) override                  {}
    const juce::String getProgramName (int) override       { return "-6 dB"; }
    void changeProgramName (int, const juce::String&) override {}

    void prepareToPlay (double, int) override              {}
    void releaseResources() override                       {}

    void processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) override
    {
        buffer.applyGain (0.5f);   // -6.02 dB exactos: la firma medible
    }

    bool hasEditor() const override                        { return false; }
    juce::AudioProcessorEditor* createEditor() override    { return nullptr; }

    void getStateInformation (juce::MemoryBlock& destino) override
    {
        const char firma[] = "pletina-prueba";
        destino.append (firma, sizeof (firma));
    }

    void setStateInformation (const void*, int) override   {}

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ProcesadorPrueba)
};

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new ProcesadorPrueba();
}
