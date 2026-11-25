// DPF-Hülle von Offbeat Hydraulics: reicht Parameter, Programme, MIDI-Noten (Auslöser) und
// Host-Takt an Pump weiter. Im LADSPA-Build (OFFBEAT_LADSPA) ohne MIDI und Zeitinfo.
#include "DistrhoPlugin.hpp"
#include "OffbeatParam.hpp"
#include "Pump.hpp"

START_NAMESPACE_DISTRHO

using namespace offbeat;

class HydraulicsPlugin : public Plugin {
public:
    HydraulicsPlugin()
        : Plugin(kParamCount, kPresetCount, 0)
    {
        pump.setSampleRate((float)getSampleRate());
    }

protected:
    const char* getLabel() const override { return "OffbeatHydraulics"; }
    const char* getDescription() const override { return "Pumpen/Sidechain: duckt im Takt der Kick"; }
    const char* getMaker() const override { return "Offbeat"; }
    const char* getHomePage() const override { return DISTRHO_PLUGIN_URI; }
    const char* getLicense() const override { return "ISC"; }
    uint32_t getVersion() const override { return d_version(0, 1, 0); }

    void initParameter(uint32_t index, Parameter& parameter) override { fillParameter(kParams[index], parameter); }

    void initProgramName(uint32_t index, String& programName) override { programName = kPresets[index].name; }

    void loadProgram(uint32_t index) override
    {
        float values[kParamCount];
        applyPreset((int)index, values);
        for (int i = 0; i < kParamCount; ++i)
            pump.setParam(i, values[i]); // Anzeige-Werte ignoriert Pump selbst
    }

    float getParameterValue(uint32_t index) const override { return pump.getParam((int)index); }
    void setParameterValue(uint32_t index, float value) override { pump.setParam((int)index, value); }

    void sampleRateChanged(double newSampleRate) override { pump.setSampleRate((float)newSampleRate); }

#if DISTRHO_PLUGIN_WANT_MIDI_INPUT
    void run(const float** inputs, float** outputs, uint32_t frames, const MidiEvent* events, uint32_t eventCount) override
#else
    void run(const float** inputs, float** outputs, uint32_t frames) override
#endif
    {
#if DISTRHO_PLUGIN_WANT_TIMEPOS
        const TimePosition& tp = getTimePosition();
        if (tp.bbt.valid) {
            const double beat = (double)(tp.bbt.bar - 1) * tp.bbt.beatsPerBar + (tp.bbt.beat - 1)
                              + tp.bbt.tick / tp.bbt.ticksPerBeat;
            pump.setTransport(true, tp.playing, beat, tp.bbt.beatsPerMinute);
        } else {
            pump.setTransport(false, false, 0.0, 0.0);
        }
#else
        pump.setTransport(false, false, 0.0, 0.0);
#endif

        uint32_t pos = 0;
#if DISTRHO_PLUGIN_WANT_MIDI_INPUT
        // Note-On löst nur in Source "MIDI" aus; der Block wird genau am Ereignis geteilt.
        const bool midi = (int)pump.getParam(kSource) == kSourceMidi;
        for (uint32_t e = 0; midi && e < eventCount; ++e) {
            const MidiEvent& ev = events[e];
            if (ev.size > 3 || ev.frame >= frames || (ev.data[0] & 0xF0) != 0x90 || ev.data[2] == 0)
                continue;
            if (ev.frame > pos) {
                pump.process(inputs[0] + pos, inputs[1] + pos, outputs[0] + pos, outputs[1] + pos, ev.frame - pos);
                pos = ev.frame;
            }
            pump.trigger();
        }
#endif
        if (pos < frames)
            pump.process(inputs[0] + pos, inputs[1] + pos, outputs[0] + pos, outputs[1] + pos, frames - pos);
    }

private:
    Pump pump;

    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(HydraulicsPlugin)
};

Plugin* createPlugin() { return new HydraulicsPlugin(); }

END_NAMESPACE_DISTRHO
