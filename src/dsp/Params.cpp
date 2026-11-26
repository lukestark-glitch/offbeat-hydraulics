#include "Params.hpp"

namespace offbeat {

const char* const kSourceNames[kSourceCount] = { "Trigger", "Host-Takt", "BPM-Takt", "MIDI" };
static const char* const kRateNames[5] = { "1/1", "1/2", "1/4", "1/8", "1/16" };
const float kRateBeats[5] = { 4, 2, 1, 0.5f, 0.25f };

// clang-format off
// Bereiche so gewählt, dass LMMS beim Einfügen der LADSPA-Version genau die Standardwerte einstellt:
// LADSPA kennt nur grobe Standard-Stufen (Minimum, 25 %, Mitte, 75 %, Maximum), siehe ladspaDefault()
// in tools/hydraulics-render.cpp. Deshalb sind die Regler linear und die Standards liegen auf diesen Stufen.
const ParamInfo kParams[kParamCount] = {
    // name           symbol      unit   min     max     default  flags
    { "Source",       "source",   "",    0,      3,      0,       kFlagInt, kSourceNames },
    { "Trigger",      "trigger",  "",    0,      1,      0,       0 },
    { "Depth",        "depth",    "dB",  0,      48,     12,      0 },
    { "Shape",        "shape",    "",    0,      1,      0.5f,    0 },
    { "Length",       "length",   "ms",  20,     480,    250,     0 },
    { "Rate",         "rate",     "",    0,      4,      2,       kFlagInt, kRateNames },
    { "Smooth",       "smooth",   "ms",  0.5f,   5.5f,   3,       0 },
    { "Phase",        "phase",    "",    0,      1,      0,       0 },
    { "BPM",          "bpm",      "BPM", 100,    220,    160,     0 },
    { "Duck",         "duck",     "dB",  -40,    0,      0,       kFlagOutput },
};

namespace {
using V = Preset::Value;
const V kickTrigger[] = { { kSource, kSourceTrigger } };
const V hostQuarter[] = { { kSource, kSourceHost } };
const V bpmQuarter[]  = { { kSource, kSourceBpm } };
const V deepPump[]    = { { kSource, kSourceBpm }, { kDepth, 24 }, { kShape, 0.8f }, { kLength, 300 } };
const V softGlue[]    = { { kSource, kSourceBpm }, { kDepth, 6 }, { kShape, 0.2f }, { kLength, 180 } };
const V eighthPump[]  = { { kSource, kSourceBpm }, { kRate, 3 }, { kLength, 150 }, { kDepth, 10 } };
const V midiPump[]    = { { kSource, kSourceMidi } };

#define PRESET(name, arr) { name, arr, (int)(sizeof(arr) / sizeof(arr[0])) }
} // namespace

const Preset kPresets[] = {
    PRESET("Kick-Trigger (LMMS Peak Controller)", kickTrigger),
    PRESET("Host-Takt 1/4", hostQuarter),
    PRESET("BPM-Takt 1/4 (160)", bpmQuarter),
    PRESET("Tiefes Pumpen 1/4", deepPump),
    PRESET("Sanft 1/4", softGlue),
    PRESET("Achtel-Pumpen 1/8", eighthPump),
    PRESET("MIDI-Trigger", midiPump),
};
// clang-format on

const int kPresetCount = (int)(sizeof(kPresets) / sizeof(kPresets[0]));

void applyDefaults(float out[kParamCount])
{
    for (int i = 0; i < kParamCount; ++i)
        out[i] = kParams[i].def;
}

void applyPreset(int index, float out[kParamCount])
{
    applyDefaults(out);
    if (index < 0 || index >= kPresetCount)
        return;
    const Preset& p = kPresets[index];
    for (int i = 0; i < p.count; ++i)
        out[p.values[i].id] = p.values[i].value;
}

int jsonParams(const float[kParamCount], int* out)
{
    // alle einstellbaren Regler (ohne Anzeige-Werte)
    int n = 0;
    for (int p = 0; p < kParamCount; ++p)
        if (!(kParams[p].flags & kFlagOutput))
            out[n++] = p;
    return n;
}

} // namespace offbeat
