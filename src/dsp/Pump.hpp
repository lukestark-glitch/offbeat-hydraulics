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

    // Host-Audio pumpen. Läuft das Vorhören (Parameter preview), kommt stattdessen das Demo-Pad heraus
    // (ohne bzw. mit Pumpen); Host-Audio am Eingang hat Vorrang und beendet das Vorhören.
    void process(const float* inL, const float* inR, float* outL, float* outR, uint32_t frames);

    // Vorschau: pumpt auf jedem Schlag des Demos (160 BPM, ab Schlag 0), egal welche Quelle eingestellt ist.
    void setPreviewClock(bool on);

    // Verstärkung der Pump-Kurve (linear) für Position x = 0..1 der Erholung; für Anzeige und DSP.
    static float recovery(float x, float shape);

private:
    void processChain(const float* inL, const float* inR, float* outL, float* outR, uint32_t frames);
    void updatePreview(const float* inL, const float* inR, uint32_t frames);
    void startPreview(int code);

    float params[kParamCount];
    float sr = 48000.0f;

    // Takt-Quellen
    double beat = 0.0, bpm = 160.0;
    bool hostValid = false, hostPlaying = false;

    // Trigger-Quellen
    bool armed = true;
    double sinceTrigger = 1e9; // Samples seit dem letzten Auslösen
    float startGain = 1.0f, gain = 1.0f;

    // Vorhören
    bool previewClock = false, previewSeen = false, transportRunning = false; // transportRunning: Host-Transport läuft
    int lastPreview = 0, previewActive = 0, previewPending = 0;
    uint32_t previewPos = 0, previewLen = 0;
    float previewFade = 0.0f, previewFadeStep = 0.0f; // weiche Ein-/Ausblendung (3 ms)
    float demoL[256], demoR[256];
};

} // namespace offbeat
