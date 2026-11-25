// DSP-Kern von Offbeat Hydraulics: duckt das Signal im Takt der Kick (Pumpen/Sidechain).
// Auslöser je nach "Source": Trigger-Regler (Flanke), Host-Takt, freier BPM-Takt oder MIDI-Note.
// Ohne DPF, damit hydraulics-render denselben Code nutzt. Latenz 0.
#pragma once

#include "Blocks.hpp"
#include "Params.hpp"

namespace offbeat {

class Pump {
public:
    Pump();

    void setSampleRate(float sr);
    void setParam(int id, float value);
    float getParam(int id) const { return params[id]; }

    // Einmal pro Block: Host-Transport (valid = Host liefert Tempo und Takt).
    void setTransport(bool valid, bool playing, double beatPosition, double bpm);

    // Auslösen ab dem nächsten Sample (MIDI-Note bzw. Trigger-Flanke).
    void trigger();

    void process(const float* inL, const float* inR, float* outL, float* outR, uint32_t frames);

    // Verstärkung der Pump-Kurve (linear) für Position x = 0..1 der Erholung; für Anzeige und DSP.
    static float recovery(float x, float shape);

private:
    float params[kParamCount];
    float sr = 48000.0f;

    // Takt-Quellen
    double beat = 0.0, bpm = 160.0;
    bool hostValid = false, hostPlaying = false;

    // Trigger-Quellen
    bool armed = true;
    double sinceTrigger = 1e9; // Samples seit dem letzten Auslösen
    float startGain = 1.0f, gain = 1.0f;
};

} // namespace offbeat
