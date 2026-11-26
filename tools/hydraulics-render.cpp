// hydraulics-render: rechnet eine WAV-Datei durch Offbeat Hydraulics bzw. prüft die Pump-Kurve.
//
//   hydraulics-render --in pad.wav --out pumped.wav [--preset NAME|NR] [--set depth=18] [--json datei]
//                     [--bpm 160]   (Host-Tempo für Source Host-Takt; Trigger/MIDI brauchen den Kick: --kicks MS,MS,...)
//   hydraulics-render --check-all    (Abnahme gegen die Ziele in docs/CODE.md, wie make check)
//   hydraulics-render --list
#include "Measure.hpp"
#include "ParamText.hpp"
#include "Preview.hpp"
#include "Pump.hpp"

#include <cfloat>
#include <string>
#include <vector>

using namespace offbeat;
using namespace measure;

namespace {

int findPreset(const char* s)
{
    for (int i = 0; i < kPresetCount; ++i)
        if (std::strcmp(kPresets[i].name, s) == 0)
            return i;
    char* end;
    const long i = std::strtol(s, &end, 10);
    return *end == 0 && i >= 0 && i < kPresetCount ? (int)i : -1;
}

int findParam(const std::string& symbol)
{
    for (int i = 0; i < kParamCount; ++i)
        if (symbol == kParams[i].symbol)
            return i;
    return -1;
}

constexpr int kSr = 48000, kBlock = 256;

// Wie der Host das Plugin füttert: Blöcke à 256 Samples, Transport pro Block, Kick-Ereignisse als
// MIDI-Note (genau auf dem Sample) bzw. als Trigger-Regler (wie LMMS' Peak Controller: pro Block,
// 1 für 40 ms ab dem Kick, sonst 0).
struct Host {
    bool valid = false, playing = false;
    double bpm = 160.0, startBeat = 0.0;
    std::vector<size_t> kicks; // Sample-Positionen
};

void run(const float* values, const Host& host, const std::vector<float>& inL, const std::vector<float>& inR,
         std::vector<float>& outL, std::vector<float>& outR)
{
    Pump pump;
    pump.setSampleRate((float)kSr);
    for (int i = 0; i < kParamCount; ++i)
        pump.setParam(i, values[i]);
    const size_t n = inL.size();
    outL.assign(n, 0.0f);
    outR.assign(n, 0.0f);
    const bool midi = (int)values[kSource] == kSourceMidi;
    size_t nextKick = 0;
    for (size_t pos = 0; pos < n;) {
        pump.setTransport(host.valid, host.playing, host.startBeat + (double)pos / kSr * host.bpm / 60.0, host.bpm);
        bool high = false;
        for (size_t k : host.kicks)
            high |= pos + kBlock > k && pos < k + (size_t)(0.04 * kSr);
        pump.setParam(kTrigger, high ? 1.0f : 0.0f);
        size_t len = std::min<size_t>(kBlock, n - pos);
        if (midi) {
            while (nextKick < host.kicks.size() && host.kicks[nextKick] < pos)
                ++nextKick;
            if (nextKick < host.kicks.size() && host.kicks[nextKick] == pos) {
                pump.trigger();
                ++nextKick;
            }
            if (nextKick < host.kicks.size() && host.kicks[nextKick] < pos + len)
                len = host.kicks[nextKick] - pos; // Block am Ereignis teilen, wie im Plugin
        }
        pump.process(&inL[pos], &inR[pos], &outL[pos], &outR[pos], (uint32_t)len);
        pos += len;
    }
}

// Verstärkungskurve: Eingang konstant 0,5 → Ausgang / 0,5.
std::vector<float> gainCurve(const float* values, const Host& host, size_t samples)
{
    std::vector<float> in(samples, 0.5f), l, r;
    run(values, host, in, in, l, r);
    for (float& v : l)
        v *= 2.0f;
    return l;
}

int failed = 0;
void expect(const std::string& what, bool ok)
{
    std::printf("  [%s] %s\n", ok ? "OK " : "FEHLER", what.c_str());
    failed += ok ? 0 : 1;
}

// Prüft eine Kurve gegen erwartete Schlag-Positionen: Tiefe am Schlag, Minimum ±2 ms, Erholung vor
// dem nächsten Schlag, keine Sprünge. offsetMs: wann das Minimum relativ zum Schlag erwartet wird.
void checkPumps(const char* title, const std::vector<float>& g, const std::vector<double>& beatsMs, const float* values,
                float offsetMs)
{
    char buf[200];
    float worstDepth = 0.0f, worstTime = 0.0f, worstRecover = 0.0f, maxStep = 0.0f;
    for (size_t b = 0; b + 1 < beatsMs.size(); ++b) {
        const size_t at = (size_t)((beatsMs[b] + offsetMs) * 0.001 * kSr);
        const size_t from = at > (size_t)(0.02 * kSr) ? at - (size_t)(0.02 * kSr) : 0, to = std::min(g.size(), at + (size_t)(0.02 * kSr));
        size_t minAt = from;
        for (size_t i = from; i < to; ++i)
            if (g[i] < g[minAt])
                minAt = i;
        worstDepth = std::fmax(worstDepth, std::fabs(toDb(g[minAt]) + values[kDepth]));
        worstTime = std::fmax(worstTime, std::fabs(((float)minAt - (float)at) * 1000.0f / kSr));
        // kurz bevor die nächste Absenkung beginnt
        const size_t before = (size_t)((beatsMs[b + 1] - values[kSmooth] - 1.0) * 0.001 * kSr);
        worstRecover = std::fmin(worstRecover, toDb(g[before]));
    }
    for (size_t i = 1; i < g.size(); ++i)
        maxStep = std::fmax(maxStep, std::fabs(g[i] - g[i - 1]));
    std::printf("== %s\n", title);
    std::snprintf(buf, sizeof(buf), "Absenkung am Schlag = Depth %.0f dB ±1 (größte Abweichung %.2f dB)", values[kDepth], worstDepth);
    expect(buf, worstDepth <= 1.0f);
    std::snprintf(buf, sizeof(buf), "Tiefster Punkt %s ±2 ms (größte Abweichung %.2f ms)",
                  offsetMs > 0.0f ? "nach Auslöser + Smooth" : "genau auf dem Schlag", worstTime);
    expect(buf, worstTime <= 2.0f);
    std::snprintf(buf, sizeof(buf), "Erholung vor dem nächsten Schlag > −1 dB (schlechtester Wert %.2f dB)", worstRecover);
    expect(buf, worstRecover > -1.0f);
    std::snprintf(buf, sizeof(buf), "Keine Klicks: größter Sprung der Kurve < 0,01 pro Sample (gemessen %.4f)", maxStep);
    expect(buf, maxStep < 0.01f);
}

// Anzahl der Pump-Vorgänge: Abwärts-Durchgänge durch die halbe Tiefe.
int countPumps(const std::vector<float>& g, float depthDb)
{
    const float half = std::pow(10.0f, -0.5f * depthDb / 20.0f);
    int count = 0;
    for (size_t i = 1; i < g.size(); ++i)
        count += g[i - 1] >= half && g[i] < half ? 1 : 0;
    return count;
}

// Standardwert, den LMMS beim Einfügen der LADSPA-Version einstellt: DPF wählt eine LADSPA-Stufe
// (DistrhoPluginLADSPA+DSSI.cpp), LMMS rechnet sie zurück (bei logarithmischen Reglern geometrisch).
float ladspaDefault(const ParamInfo& p)
{
    const float a = p.min, b = p.max, d = p.def;
    const bool log = p.flags & kFlagLog;
    if (d == 0.0f || d == 1.0f || d == 100.0f || d == 440.0f || d == a || d == b)
        return d;
    if (d < 0.625f * a + 0.375f * b)
        return log ? std::exp(0.75f * std::log(a) + 0.25f * std::log(b)) : 0.75f * a + 0.25f * b;
    if (d > 0.375f * a + 0.625f * b)
        return log ? std::exp(0.25f * std::log(a) + 0.75f * std::log(b)) : 0.25f * a + 0.75f * b;
    return log ? std::sqrt(a * b) : 0.5f * (a + b);
}

int checkAll()
{
    char buf[200];
    const size_t len = (size_t)(3.0 * kSr);
    float values[kParamCount];

    // BPM-Takt 1/4 bei 160 BPM (Preset 2) und 1/8 (Preset 5): Schläge alle 375 bzw. 187,5 ms ab 0.
    for (int preset : { 2, 5 }) {
        applyPreset(preset, values);
        const double step = kRateBeats[(int)values[kRate]] * 60000.0 / values[kBpm];
        std::vector<double> beats;
        for (double t = step; t < 2900.0; t += step)
            beats.push_back(t);
        checkPumps(kPresets[preset].name, gainCurve(values, Host(), len), beats, values, 0.0f);
    }

    // Phase 25 %: Schläge um eine Viertel-Periode verschoben.
    applyPreset(2, values);
    values[kPhase] = 0.25f;
    {
        std::vector<double> beats;
        for (double t = 0.25 * 375.0 + 375.0; t < 2900.0; t += 375.0)
            beats.push_back(t);
        checkPumps("BPM-Takt 1/4, Phase 25 %", gainCurve(values, Host(), len), beats, values, 0.0f);
    }

    // Host-Takt: Host spielt mit 150 BPM, Song steht bei Schlag 3,37 → Schläge bei (4 − 3,37) · 400 ms usw.
    applyPreset(1, values);
    {
        Host host;
        host.valid = host.playing = true;
        host.bpm = 150.0;
        host.startBeat = 3.37;
        std::vector<double> beats;
        for (double t = (4.0 - 3.37) * 400.0; t < 2900.0; t += 400.0)
            beats.push_back(t);
        checkPumps("Host-Takt 1/4 (Host 150 BPM, Start bei Schlag 3,37)", gainCurve(values, host, len), beats, values, 0.0f);
    }
    // Host ohne Tempo → Fallback auf den BPM-Regler (160).
    {
        std::vector<double> beats;
        for (double t = 375.0; t < 2900.0; t += 375.0)
            beats.push_back(t);
        checkPumps("Host-Takt ohne Host-Tempo → BPM-Regler 160", gainCurve(values, Host(), len), beats, values, 0.0f);
    }

    // MIDI: Kicks bei unregelmäßigen Zeiten; tiefster Punkt nach Smooth.
    applyPreset(6, values);
    Host kicks;
    for (double t : { 100.0, 480.0, 820.0, 1203.7, 1580.0, 1960.0, 2340.0, 2700.0 })
        kicks.kicks.push_back((size_t)(t * 0.001 * kSr));
    {
        std::vector<double> beats;
        for (size_t k : kicks.kicks)
            beats.push_back(k * 1000.0 / kSr);
        checkPumps("MIDI-Trigger (Kicks unregelmäßig)", gainCurve(values, kicks, len), beats, values, values[kSmooth]);
    }

    // Trigger-Regler (LMMS Peak Controller): Auslösung am Block-Anfang, in dem der Regler über 0,5 geht.
    applyPreset(0, values);
    {
        std::vector<double> beats;
        for (size_t k : kicks.kicks)
            beats.push_back((double)(k / kBlock * kBlock) * 1000.0 / kSr); // Blockanfang
        const std::vector<float> g = gainCurve(values, kicks, len);
        checkPumps("Trigger-Regler (Peak Controller, Blöcke à 256)", g, beats, values, values[kSmooth]);
        std::snprintf(buf, sizeof(buf), "Genau ein Pumpen pro Flanke (%zu Kicks, %d gezählt)", kicks.kicks.size(), countPumps(g, values[kDepth]));
        expect(buf, countPumps(g, values[kDepth]) == (int)kicks.kicks.size());
    }
    // Flattern um 0,5 ohne Rückkehr unter 0,3 → nur ein Auslösen (Hysterese).
    {
        Pump pump;
        pump.setSampleRate((float)kSr);
        for (int i = 0; i < kParamCount; ++i)
            pump.setParam(i, values[i]);
        std::vector<float> in(kBlock, 0.5f), o1(kBlock), o2(kBlock), g;
        const float seq[] = { 0.0f, 0.6f, 0.45f, 0.6f, 0.4f, 0.7f, 0.35f, 0.55f };
        for (int rep = 0; rep < 60; ++rep) {
            pump.setParam(kTrigger, seq[rep % 8 == rep ? rep : 7 - (rep % 2)]);
            pump.process(in.data(), in.data(), o1.data(), o2.data(), kBlock);
            for (float v : o1)
                g.push_back(2.0f * v);
        }
        std::snprintf(buf, sizeof(buf), "Hysterese: Flattern zwischen 0,35 und 0,7 löst nur einmal aus (%d gezählt)", countPumps(g, values[kDepth]));
        expect(buf, countPumps(g, values[kDepth]) == 1);
    }

    // Depth 0 → bit-identisch; Stille → Stille; kein NaN/Denormal; in allen Quellen.
    std::puts("== Grenzfälle");
    Noise noise;
    std::vector<float> nl(len), nr(len), ol, orr;
    for (size_t i = 0; i < len; ++i)
        nl[i] = 0.5f * noise.next(), nr[i] = 0.5f * noise.next();
    bool identical = true, silent = true, finite = true;
    for (int src = 0; src < kSourceCount; ++src) {
        applyDefaults(values);
        values[kSource] = (float)src;
        values[kDepth] = 0.0f;
        run(values, kicks, nl, nr, ol, orr);
        for (size_t i = 0; i < len; ++i)
            identical &= ol[i] == nl[i] && orr[i] == nr[i];
        values[kDepth] = 36.0f;
        values[kSmooth] = 0.5f;
        const std::vector<float> zero(len, 0.0f);
        run(values, kicks, zero, zero, ol, orr);
        for (size_t i = 0; i < len; ++i)
            silent &= ol[i] == 0.0f && orr[i] == 0.0f;
        run(values, kicks, nl, nr, ol, orr);
        for (size_t i = 0; i < len; ++i)
            finite &= std::isfinite(ol[i]) && std::isfinite(orr[i]) && !(ol[i] != 0.0f && std::fabs(ol[i]) < FLT_MIN);
    }
    expect("Depth 0: Ausgang bit-identisch mit dem Eingang (alle Quellen)", identical);
    expect("Stille bleibt Stille (alle Quellen, Depth 36 dB)", silent);
    expect("Kein NaN/Inf, keine Denormals (Depth 36 dB, Smooth 0,5 ms)", finite);

    std::puts("== LMMS-LADSPA: Standardwerte beim Einfügen");
    for (const ParamInfo& p : kParams) {
        if (p.flags & kFlagOutput)
            continue;
        const float got = ladspaDefault(p);
        std::snprintf(buf, sizeof(buf), "%s: LMMS stellt %g ein (Standard %g)", p.name, got, p.def);
        expect(buf, std::fabs(got - p.def) <= 0.01f * (p.max - p.min));
    }

    std::printf("%s\n", failed ? "NICHT BESTANDEN" : "BESTANDEN");
    return failed ? 1 : 0;
}

// Abnahme der Vorschau: renderPreview() (Oberfläche) ist bitgleich zu einer Pump mit Quelle BPM-Takt 160
// auf dem Demo-Pad; die Pump-Kurve pumpt auf jedem Schlag; Vorhören über den Plugin-Ausgang knackst nicht
// (Start, Wechsel ohne→mit, Stopp); Host-Audio hat Vorrang.
int previewCheck()
{
    char buf[200];
    std::puts("== Vorschau bitgleich zu Quelle BPM-Takt 160 (alle Presets, 1 Schlag und 1 Takt), Pump-Kurve");
    int compared = 0, differing = 0, curveWrong = 0;
    for (int p = 0; p < kPresetCount; ++p) {
        float values[kParamCount];
        applyPreset(p, values);
        for (double beats : { 1.0, 4.0 }) {
            std::vector<float> al, ar, curve;
            renderPreview(values, 1, beats, al, ar);
            std::vector<float> bl, br;
            renderPreview(values, 0, beats, bl, br); // Demo-Ausschnitt
            Pump ref;
            ref.setSampleRate((float)kDemoRate);
            for (int i = 0; i < kParamCount; ++i)
                if (!(kParams[i].flags & (kFlagHidden | kFlagOutput)))
                    ref.setParam(i, values[i]);
            ref.setParam(kSource, kSourceBpm);
            ref.setParam(kBpm, (float)kDemoBpm);
            for (uint32_t pos = 0; pos < bl.size(); pos += 256) {
                const uint32_t n = std::min<uint32_t>(256, (uint32_t)bl.size() - pos);
                ref.process(&bl[pos], &br[pos], &bl[pos], &br[pos], n);
            }
            ++compared;
            differing += (al == bl && ar == br) ? 0 : 1;
            renderPumpCurve(values, beats, curve);
            const float rate = kRateBeats[(int)values[kRate]];
            const int want = (int)std::ceil(beats / rate - 1e-9);
            curveWrong += countPumps(curve, values[kDepth]) == want ? 0 : 1;
        }
    }
    std::snprintf(buf, sizeof(buf), "%d Vorschau-Renderings, davon %d nicht bitgleich", compared, differing);
    expect(buf, differing == 0);
    std::snprintf(buf, sizeof(buf), "Pump-Kurve pumpt im Takt des Demos (%d von %d falsch)", curveWrong, compared);
    expect(buf, curveWrong == 0);

    std::puts("== Vorhören über den Plugin-Ausgang (Standard-Preset, Loop): Start, Wechsel ohne→mit, Stopp, Host-Audio");
    float values[kParamCount];
    applyPreset(2, values);
    // Bezug: größter Sprung im Klang beider Ebenen selbst (frisch gestartet, zwei Loop-Runden)
    float maxStepSteady = 0.0f;
    for (int layer : { 1, 2 }) {
        Pump ref;
        ref.setSampleRate((float)kDemoRate);
        for (int i = 0; i < kParamCount; ++i)
            ref.setParam(i, values[i]);
        ref.setParam(kPreviewLoop, 1);
        ref.setParam(kPreviewBar, 1);
        std::vector<float> z(256, 0.0f), ol(2 * kDemoFrames + 512), orr(ol.size());
        for (uint32_t pos = 0; pos + 256 <= ol.size(); pos += 256) {
            if (pos == 256)
                ref.setParam(kPreview, layer | 8);
            ref.process(z.data(), z.data(), &ol[pos], &orr[pos], 256);
        }
        for (size_t i = 256 * 2; i < ol.size(); ++i)
            maxStepSteady = std::fmax(maxStepSteady, std::fabs(ol[i] - ol[i - 1]));
    }
    Pump t;
    t.setSampleRate((float)kDemoRate);
    for (int i = 0; i < kParamCount; ++i)
        t.setParam(i, values[i]);
    t.setParam(kPreviewLoop, 1);
    t.setParam(kPreviewBar, 1);
    const uint32_t total = 4 * kDemoRate;
    std::vector<float> host(256), l(total), r(total);
    float maxStep = 0.0f;
    bool played = false, hostWon = false;
    for (uint32_t pos = 0; pos < total; pos += 256) {
        if (pos == 256) t.setParam(kPreview, 1 | 8);       // ohne
        if (pos == 256 * 200) t.setParam(kPreview, 2);     // Wechsel auf mit Pumpen
        if (pos == 256 * 350) t.setParam(kPreview, 0 | 8); // Stopp
        if (pos == 256 * 450) t.setParam(kPreview, 2 | 8); // wieder mit
        const bool hostPlays = pos >= 256 * 650;
        for (uint32_t i = 0; i < 256; ++i)
            host[i] = hostPlays ? 0.1f * std::sin(2 * kPi * 220.0f * (pos + i) / kDemoRate) : 0.0f;
        t.process(host.data(), host.data(), &l[pos], &r[pos], 256);
        played |= t.getParam(kPreviewActive) == 2.0f;
        if (hostPlays && pos > 256 * 652)
            hostWon |= t.getParam(kPreviewActive) == 0.0f;
        for (uint32_t i = pos + 1; i < pos + 256; ++i)
            for (uint32_t at : { 256u, 256u * 200, 256u * 350, 256u * 450 })
                if (i + kDemoRate / 100 > at && i < at + kDemoRate / 100)
                    maxStep = std::fmax(maxStep, std::fabs(l[i] - l[i - 1]));
    }
    expect("Vorhören startet ohne Host-Audio", played);
    {
        // Play im Host (auch ohne Audio am Eingang): beendet das Vorhören, und solange er spielt, startet es nicht
        Pump e;
        e.setSampleRate((float)kDemoRate);
        e.setParam(kPreviewLoop, 1);
        std::vector<float> z(256, 0.0f), bl(256), br(256);
        e.setTransport(true, false, 0.0, 160.0);
        e.process(z.data(), z.data(), bl.data(), br.data(), 256);
        e.setParam(kPreview, 2 | 8);
        e.process(z.data(), z.data(), bl.data(), br.data(), 256);
        const bool running = e.getParam(kPreviewActive) == 2.0f;
        e.setTransport(true, true, 0.0, 160.0); // Play
        for (int b = 0; b < 4; ++b)
            e.process(z.data(), z.data(), bl.data(), br.data(), 256);
        const bool stoppedByPlay = e.getParam(kPreviewActive) == 0.0f;
        e.setParam(kPreview, 2);
        e.process(z.data(), z.data(), bl.data(), br.data(), 256);
        expect("Play im Host beendet das Vorhören", running && stoppedByPlay);
        expect("Solange der Host spielt, startet kein Vorhören", e.getParam(kPreviewActive) == 0.0f);
    }
    expect("Host-Audio beendet das Vorhören", hostWon);
    std::snprintf(buf, sizeof(buf), "Kein Knackser an Start/Wechsel/Stopp: größter Sprung %.4f ≤ im Klang der Ebenen selbst %.4f", maxStep, maxStepSteady);
    expect(buf, maxStep <= maxStepSteady * 1.01f);
    std::printf("%s\n", failed ? "NICHT BESTANDEN" : "BESTANDEN");
    return failed ? 1 : 0;
}

// JSON für die LADSPA-Version in LMMS: LADSPA hat keine Oberfläche und kein EINFÜGEN. Darum zeigt dieses
// Werkzeug zu einem JSON (z. B. aus KOPIEREN) die Werte, die man im LMMS-Effektfenster einstellt.
int lmmsValues(const char* file)
{
    std::string json, errors;
    if (FILE* f = std::fopen(file, "rb")) {
        char b[4096];
        for (size_t n; (n = std::fread(b, 1, sizeof(b), f)) > 0;)
            json.append(b, n);
        std::fclose(f);
    }
    float values[kParamCount];
    applyDefaults(values);
    bool changed[kParamCount] = {};
    if (applyJson(json.c_str(), values, changed, errors) == 0) {
        std::fprintf(stderr, "Kein gültiges JSON in %s\n", file);
        return 2;
    }
    std::printf("In LMMS im Effektfenster von Offbeat Hydraulics (LADSPA) einstellen%s%s:\n", errors.empty() ? "" : " – ignoriert: ", errors.c_str());
    for (int p = 0; p < kParamCount; ++p) {
        const ParamInfo& i = kParams[p];
        if (i.flags & (kFlagOutput | kFlagHidden))
            continue;
        std::printf("  %-12s %8g %-3s%s\n", i.name, values[p], i.unit, changed[p] ? "" : "  (Standard)");
    }
    return 0;
}

void usage()
{
    std::puts("hydraulics-render --in datei.wav [--out datei.wav] [--preset NAME|NR] [--json datei] [--set symbol=wert]...\n"
              "                  [--bpm 160] [--kicks MS,MS,...]\n"
              "hydraulics-render --check-all | --preview-check | --lmms-values datei.json | --list");
}

} // namespace

int main(int argc, char** argv)
{
    int preset = 2;
    const char *in = nullptr, *out = nullptr, *jsonFile = nullptr;
    Host host;
    std::vector<std::pair<int, float>> overrides;
    for (int i = 1; i < argc; ++i) {
        const std::string a = argv[i];
        const char* v = i + 1 < argc ? argv[i + 1] : nullptr;
        if (a == "--check-all")
            return checkAll();
        if (a == "--preview-check")
            return previewCheck();
        if (a == "--lmms-values" && v)
            return lmmsValues(v);
        if (a == "--preset-json" && v) { // Preset als JSON, genau wie KOPIEREN in der Oberfläche
            float vals[kParamCount];
            applyPreset(std::atoi(v), vals);
            int list[kParamCount];
            std::fputs(toJson(vals, list, jsonParams(vals, list)).c_str(), stdout);
            return 0;
        }
        if (a == "--list") {
            std::puts("Presets:");
            for (int p = 0; p < kPresetCount; ++p)
                std::printf("  %d  %s\n", p, kPresets[p].name);
            std::puts("Parameter (symbol: min..max, default):");
            for (const ParamInfo& p : kParams)
                if (!(p.flags & (kFlagOutput | kFlagHidden)))
                    std::printf("  %-10s %g..%g %s, %g\n", p.symbol, p.min, p.max, p.unit, p.def);
            return 0;
        }
        if (!v) {
            usage();
            return 2;
        }
        ++i;
        if (a == "--in")
            in = v;
        else if (a == "--out")
            out = v;
        else if (a == "--json")
            jsonFile = v;
        else if (a == "--bpm") {
            host.valid = host.playing = true;
            host.bpm = std::atof(v);
        } else if (a == "--kicks") {
            for (const char* s = v; *s;) {
                host.kicks.push_back((size_t)(std::atof(s) * 0.001 * kSr));
                const char* c = std::strchr(s, ',');
                s = c ? c + 1 : s + std::strlen(s);
            }
        } else if (a == "--preset") {
            if ((preset = findPreset(v)) < 0) {
                std::fprintf(stderr, "Unbekanntes Preset: %s\n", v);
                return 2;
            }
        } else if (a == "--set") {
            const std::string kv = v;
            const size_t eq = kv.find('=');
            const int id = eq == std::string::npos ? -1 : findParam(kv.substr(0, eq));
            if (id < 0) {
                std::fprintf(stderr, "Unbekannter Parameter: %s\n", v);
                return 2;
            }
            overrides.emplace_back(id, (float)std::atof(kv.c_str() + eq + 1));
        } else {
            usage();
            return 2;
        }
    }
    if (!in) {
        usage();
        return 2;
    }
    std::vector<float> l, r;
    int sr = kSr;
    if (!readWav(in, l, r, sr)) {
        std::fprintf(stderr, "Konnte %s nicht lesen (WAV mit 16/24/32 Bit oder Float erwartet)\n", in);
        return 1;
    }
    if (sr != kSr)
        std::printf("Hinweis: Datei hat %d Hz, gerechnet wird mit %d Hz (Zeiten verschieben sich entsprechend)\n", sr, kSr);
    float values[kParamCount];
    applyPreset(preset, values);
    if (jsonFile) {
        std::string json, errors;
        if (FILE* f = std::fopen(jsonFile, "rb")) {
            char buf[4096];
            for (size_t rd; (rd = std::fread(buf, 1, sizeof(buf), f)) > 0;)
                json.append(buf, rd);
            std::fclose(f);
        }
        bool changed[kParamCount] = {};
        std::printf("JSON: %d Werte gesetzt%s%s\n", applyJson(json.c_str(), values, changed, errors),
                    errors.empty() ? "" : "; ", errors.c_str());
    }
    for (const auto& o : overrides)
        values[o.first] = o.second;
    std::vector<float> ol, orr;
    run(values, host, l, r, ol, orr);
    double before = 0.0, after = 0.0;
    for (size_t i = 0; i < l.size(); ++i) {
        before += 0.5 * ((double)l[i] * l[i] + (double)r[i] * r[i]);
        after += 0.5 * ((double)ol[i] * ol[i] + (double)orr[i] * orr[i]);
    }
    std::printf("%s: Source %s, Depth %.1f dB, RMS vorher %.2f dBFS, nachher %.2f dBFS\n", in,
                kSourceNames[(int)values[kSource]], values[kDepth], 10.0 * std::log10(before / l.size() + 1e-24),
                10.0 * std::log10(after / l.size() + 1e-24));
    if (out) {
        if (!writeWav(out, ol, orr, kSr)) {
            std::fprintf(stderr, "Konnte %s nicht schreiben\n", out);
            return 1;
        }
        std::printf("WAV: %s\n", out);
    }
    return 0;
}
