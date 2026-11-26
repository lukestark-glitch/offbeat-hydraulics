#include "Pump.hpp"
#include "Preview.hpp"

#include <algorithm>


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
    // Play im Host beendet ein laufendes Vorhören: beim Abspielen klingt nur, was vom Host kommt.
    if (playing && !transportRunning && previewActive) {
        previewPending = 0;
        previewFadeStep = -1.0f / (0.003f * sr);
    }
    transportRunning = playing;
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

void Pump::processChain(const float* inL, const float* inR, float* outL, float* outR, uint32_t frames)
{
    const DenormalGuard denormals; // Flush-to-Zero für diesen Block (Intel und Apple Silicon)
    const int source = previewClock ? (int)kSourceBpm : (int)params[kSource]; // Vorschau: eigener Takt
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
    const double tempo = previewClock ? kDemoBpm : (source == kSourceHost && hostValid) ? bpm : (double)params[kBpm];
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
}

// ---- Vorhören ---------------------------------------------------------------------------------

void Pump::setPreviewClock(bool on)
{
    previewClock = on;
    beat = 0.0; // Pumpen genau auf den Schlägen des Demos
}

void Pump::startPreview(int code)
{
    previewActive = code;
    previewPending = 0;
    previewPos = 0;
    previewLen = params[kPreviewBar] >= 0.5f ? kDemoFrames : kDemoBeat;
    previewFade = 0.0f;
    previewFadeStep = 1.0f / (0.003f * sr);
    setPreviewClock(code == 2);
    gain = startGain = 1.0f;
}

void Pump::updatePreview(const float* inL, const float* inR, uint32_t frames)
{
    const int request = (int)params[kPreview];
    if (!previewSeen) { // Wert beim Laden eines Projekts: nur merken, nicht abspielen
        previewSeen = true;
        lastPreview = request;
    } else if (request != lastPreview) {
        lastPreview = request;
        const int code = request & 7;
        if (transportRunning && code != 0) {
            // Host spielt: Vorhören startet nicht (auch nicht durch wiederhergestellte Parameter)
        } else if (code == 1 || code == 2) {
            if (previewActive) { // läuft schon: erst ausblenden, dann umschalten (kein Pegelsprung)
                previewPending = code;
                previewFadeStep = -1.0f / (0.003f * sr);
            } else {
                startPreview(code);
            }
        } else if (previewActive) {
            previewPending = 0;
            previewFadeStep = -1.0f / (0.003f * sr);
        }
    }
    if (!previewActive)
        return;
    for (uint32_t i = 0; i < frames; ++i) // Host-Audio hat Vorrang
        if (std::fabs(inL[i]) > 1e-4f || std::fabs(inR[i]) > 1e-4f) {
            previewActive = 0;
            setPreviewClock(false);
            return;
        }
}

void Pump::process(const float* inL, const float* inR, float* outL, float* outR, uint32_t frames)
{
    updatePreview(inL, inR, frames);
    if (!previewActive) {
        processChain(inL, inR, outL, outR, frames);
        params[kPreviewActive] = 0.0f;
        return;
    }
    for (uint32_t done = 0; done < frames;) { // Demo-Pad statt Eingang, in Stücken von 256
        const uint32_t n = std::min<uint32_t>(256, frames - done);
        for (uint32_t i = 0; i < n; ++i) {
            const bool playing = previewPos < previewLen;
            demoL[i] = playing ? demoSample(previewPos, 0) : 0.0f;
            demoR[i] = playing ? demoSample(previewPos, 1) : 0.0f;
            if (++previewPos >= previewLen && params[kPreviewLoop] >= 0.5f) {
                previewPos = 0;
                if (previewClock)
                    beat = 0.0; // Takt mit dem Loop neu ausrichten
            }
        }
        if (previewActive == 2)
            processChain(demoL, demoR, outL + done, outR + done, n);
        else
            std::copy(demoL, demoL + n, outL + done), std::copy(demoR, demoR + n, outR + done);
        for (uint32_t i = 0; i < n; ++i) {
            previewFade = clampf(previewFade + previewFadeStep, 0.0f, 1.0f);
            outL[done + i] *= previewFade;
            outR[done + i] *= previewFade;
        }
        done += n;
        if (previewPending && previewFade <= 0.0f)
            startPreview(previewPending);
    }
    if ((previewFadeStep < 0.0f && previewFade <= 0.0f && !previewPending) || (params[kPreviewLoop] < 0.5f && previewPos >= previewLen)) {
        previewActive = 0;
        setPreviewClock(false);
    }
    params[kPreviewActive] = (float)previewActive;
}

} // namespace offbeat
