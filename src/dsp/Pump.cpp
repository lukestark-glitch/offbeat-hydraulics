#include "Pump.hpp"

#if defined(__SSE__) || defined(__x86_64__)
#include <xmmintrin.h>
#define OFFBEAT_HAVE_SSE 1
#endif

namespace offbeat {

namespace {
// Weiche Absenkung 0 → 1 (Raised Cosine), ohne Knick an den Enden.
inline float raisedCos(float x) { return 0.5f - 0.5f * std::cos(kPi * clampf(x, 0.0f, 1.0f)); }
} // namespace

Pump::Pump()
{
    applyDefaults(params);
}

void Pump::setSampleRate(float newSr)
{
    sr = newSr;
    sinceTrigger = 1e9;
    gain = startGain = 1.0f;
}

void Pump::setParam(int id, float value)
{
    if (id < 0 || id >= kParamCount || (kParams[id].flags & kFlagOutput))
        return;
    params[id] = clampf(value, kParams[id].min, kParams[id].max);
}

void Pump::setTransport(bool valid, bool playing, double beatPosition, double hostBpm)
{
    hostValid = valid && hostBpm > 0.0;
    hostPlaying = hostValid && playing;
    if (hostValid)
        bpm = hostBpm;
    if (hostPlaying && (int)params[kSource] == kSourceHost)
        beat = beatPosition; // sonst läuft der Takt intern weiter
}

void Pump::trigger()
{
    startGain = gain; // Absenkung startet beim aktuellen Pegel: kein Sprung bei schnellen Schlägen
    sinceTrigger = 0.0;
}

// Linear (Shape 0) bis S-Kurve (Shape 1, Smootherstep: bleibt länger unten, kommt dann schnell hoch).
float Pump::recovery(float x, float shape)
{
    x = clampf(x, 0.0f, 1.0f);
    const float s = x * x * x * (x * (6.0f * x - 15.0f) + 10.0f);
    return x + shape * (s - x);
}

void Pump::process(const float* inL, const float* inR, float* outL, float* outR, uint32_t frames)
{
#ifdef OFFBEAT_HAVE_SSE
    const unsigned int oldCsr = _mm_getcsr();
    _mm_setcsr(oldCsr | 0x8040); // Flush-to-Zero + Denormals-are-Zero
#endif
    const int source = (int)params[kSource];
    const float duck = std::pow(10.0f, -params[kDepth] / 20.0f);
    const float shape = params[kShape];
    const float lengthSamples = params[kLength] * 0.001f * sr;
    float smoothSamples = params[kSmooth] * 0.001f * sr;

    // Trigger-Regler: steigende Flanke über 0,5 löst einmal aus, neu scharf erst unter 0,3.
    if (source == kSourceTrigger) {
        if (armed && params[kTrigger] > 0.5f) {
            trigger();
            armed = false;
        } else if (params[kTrigger] < 0.3f) {
            armed = true;
        }
    }

    const bool clock = source == kSourceHost || source == kSourceBpm;
    const double tempo = (source == kSourceHost && hostValid) ? bpm : (double)params[kBpm];
    const double rateBeats = kRateBeats[(int)params[kRate]];
    const double periodSamples = rateBeats * 60.0 / tempo * sr;
    if (clock)
        smoothSamples = std::fmin(smoothSamples, 0.5f * (float)periodSamples);
    const double beatStep = tempo / 60.0 / sr;
    float minGain = 1.0f;

    for (uint32_t i = 0; i < frames; ++i) {
        float g;
        if (clock) {
            // Zustandslos aus der Taktposition: Absenkung beginnt "Smooth" vor dem Schlag und ist
            // genau auf dem Schlag am tiefsten; danach Erholung über "Length".
            double u = beat / rateBeats - (double)params[kPhase];
            u -= std::floor(u);
            const float since = (float)(u * periodSamples), until = (float)((1.0 - u) * periodSamples);
            g = since >= lengthSamples ? 1.0f : duck + (1.0f - duck) * recovery(since / lengthSamples, shape);
            if (until < smoothSamples)
                g = std::fmin(g, 1.0f - (1.0f - duck) * raisedCos(1.0f - until / smoothSamples));
            beat += beatStep;
        } else {
            // Auslöser kommt ohne Vorwarnung: Absenkung über "Smooth", dann Erholung über "Length".
            const float t = (float)sinceTrigger;
            if (t < smoothSamples)
                g = startGain + (duck - startGain) * raisedCos(t / smoothSamples);
            else if (t - smoothSamples >= lengthSamples)
                g = 1.0f; // Ruhe: exakt unverändert
            else
                g = duck + (1.0f - duck) * recovery((t - smoothSamples) / lengthSamples, shape);
            sinceTrigger += 1.0;
        }
        gain = g;
        minGain = std::fmin(minGain, g);
        outL[i] = g * inL[i];
        outR[i] = g * inR[i];
    }
    params[kDuck] = clampf(20.0f * std::log10(minGain + 1e-9f), -40.0f, 0.0f);
#ifdef OFFBEAT_HAVE_SSE
    _mm_setcsr(oldCsr);
#endif
}

} // namespace offbeat
