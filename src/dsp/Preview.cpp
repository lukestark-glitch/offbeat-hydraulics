#include "Preview.hpp"
#include "DemoPad.h" // aus assets/demo-pad.raw erzeugt (xxd -i), siehe Makefile

#include <algorithm>
#include <cmath>
#include <cstring>

namespace offbeat {

float demoSample(uint32_t frame, int channel)
{
    const size_t at = ((size_t)(frame % kDemoFrames) * 2 + channel) * 2;
    int16_t v;
    std::memcpy(&v, demo_pad_raw + at, 2); // 16 Bit little-endian
    return (float)v / 32767.0f;
}

namespace {
// Eingang durch eine frische Pump mit dem Takt des Demos (Blöcke von 256 wie im Plugin)
void pump(const float* params, std::vector<float>& l, std::vector<float>& r)
{
    Pump p;
    p.setSampleRate((float)kDemoRate);
    for (int i = 0; i < kParamCount; ++i)
        if (!(kParams[i].flags & (kFlagHidden | kFlagOutput)))
            p.setParam(i, params[i]);
    p.setPreviewClock(true);
    for (uint32_t pos = 0; pos < l.size(); pos += 256) {
        const uint32_t n = std::min<uint32_t>(256, (uint32_t)l.size() - pos);
        p.process(&l[pos], &r[pos], &l[pos], &r[pos], n);
    }
}
} // namespace

void renderPreview(const float* params, int layer, double beats, std::vector<float>& l, std::vector<float>& r)
{
    const uint32_t frames = (uint32_t)std::lround(beats * kDemoBeat);
    l.resize(frames), r.resize(frames);
    for (uint32_t i = 0; i < frames; ++i)
        l[i] = demoSample(i, 0), r[i] = demoSample(i, 1);
    if (layer == 1)
        pump(params, l, r);
}

void renderPumpCurve(const float* params, double beats, std::vector<float>& gain)
{
    std::vector<float> r((size_t)std::lround(beats * kDemoBeat), 1.0f);
    gain.assign(r.size(), 1.0f);
    pump(params, gain, r); // Eingang 1,0 → Ausgang = Verstärkung
}

} // namespace offbeat
