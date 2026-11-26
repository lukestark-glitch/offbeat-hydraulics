// Parameter-Liste von Offbeat Hydraulics. Neue Parameter immer hinten anhängen: die Nummer ist die
// Parameter-ID in LV2/VST3/LADSPA, gespeicherte Projekte hängen daran.
#pragma once

#include "ParamInfo.hpp"

namespace offbeat {

enum Source { kSourceTrigger, kSourceHost, kSourceBpm, kSourceMidi, kSourceCount };

enum ParamId {
    kSource,
    kTrigger, // Flanke über 0,5 löst aus (in LMMS: Peak Controller der Kick-Spur)
    kDepth,
    kShape,
    kLength,
    kRate,
    kSmooth,
    kPhase,
    kBpm,
    // Anzeige-Wert vom DSP an die Oberfläche
    kDuck,
    // Vorhören des Demo-Pads aus der Oberfläche (versteckt, nicht automatisierbar). preview: Bit 0–2 =
    // 1 ohne, 2 mit Pumpen, 0 Stopp; Bit 3 wechselt bei jedem Klick, damit jede Änderung auslöst.
    kPreview,
    kPreviewLoop,
    kPreviewBar,    // Ausschnitt: aus = 1 Schlag, an = 1 Takt
    kPreviewActive, // Anzeige: was gerade vorgehört wird (0 = nichts)

    kParamCount
};

extern const ParamInfo kParams[kParamCount];
extern const char* const kSourceNames[kSourceCount];

// Pump-Abstand in Schlägen (Index = Parameterwert): 1/1, 1/2, 1/4, 1/8, 1/16.
extern const float kRateBeats[5];

struct Preset {
    const char* name;
    struct Value { ParamId id; float value; };
    const Value* values;
    int count;
};

extern const Preset kPresets[];
extern const int kPresetCount;

void applyPreset(int index, float out[kParamCount]);
void applyDefaults(float out[kParamCount]);

// Parameter, die KOPIEREN (und topline/offbeat-render --preset-json) als JSON ausgibt.
int jsonParams(const float values[kParamCount], int* out);

} // namespace offbeat
