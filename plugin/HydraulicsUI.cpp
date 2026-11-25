// Oberfläche von Offbeat Hydraulics: Quelle (Pillen), PUMPE (Depth, Shape, Length, Smooth, Lowrider),
// TAKT (Rate, Phase, BPM, Trigger) und KURVE (Pump-Hüllkurve aus den Reglern + Absenkungs-Anzeige).
// Look und Bedienung: OffbeatKit.hpp (aus offbeat-engine).
#include "OffbeatKit.hpp"
#include "LowriderImage.h" // aus assets/lowrider.png erzeugt (xxd -i), siehe plugin/Makefile
#include "Pump.hpp"

START_NAMESPACE_DISTRHO

using namespace offbeat;
using kit::kCellH;
using kit::kCellW;
using kit::kLabelH;

namespace {
constexpr float kW = 860, kH = 400, kMargin = 20, kGridX = 31;
constexpr float kPumpY = 104, kLowY = 252, kTaktW = 480;

const int kPumpKnobs[4] = { kDepth, kShape, kLength, kSmooth };
const int kTaktKnobs[4] = { kRate, kPhase, kBpm, kTrigger };

kit::Theme hydraulicsTheme()
{
    kit::Theme t;
    t.title = "OFFBEAT";
    t.subtitle = "HYDRAULICS";
    t.brand = Color(160, 60, 255);
    t.brandInk = Color(70, 10, 140);
    t.night = Color(6, 3, 14);
    t.night2 = Color(34, 10, 70);
    t.light = Color(190, 130, 255);
    t.plate = Color(16, 8, 36);
    t.track = Color(52, 30, 100);
    t.text = Color(244, 236, 255);
    t.dim = Color(190, 165, 235);
    return t;
}
} // namespace

class HydraulicsUI : public OffbeatKitUI {
public:
    HydraulicsUI()
        : OffbeatKitUI(kW, kH, hydraulicsTheme())
    {
        lowrider = createImageFromMemory(lowrider_png, lowrider_png_len, IMAGE_GENERATE_MIPMAPS);
    }

protected:
    void drawContent() override
    {
        const int source = (int)std::lround(values[kSource]);
        drawLogo(145, 50);
        for (int s = 0; s < kSourceCount; ++s) {
            const float w = 120, x = kW - kMargin - (kSourceCount - s) * (w + 6) + 6;
            pill(100 + s, x, 33, w, 34, kSourceNames[s], th.brand, s == source);
        }

        drawPanel(kMargin, kPumpY, kW - 2 * kMargin, kLabelH + kCellH, "PUMPE", th.brand);
        // Lowrider rechts; ragt etwas über die Plakette hinaus, die Funken laufen unter dem letzten Regler aus.
        const float carW = 410, carH = carW * 384.0f / 1024.0f;
        drawImage(lowrider, kW - carW + 6, kPumpY + (kLabelH + kCellH) / 2 - carH / 2 + 4, carW, carH);
        for (int i = 0; i < 4; ++i)
            knob(kPumpKnobs[i], kGridX + i * kCellW, kPumpY + kLabelH, th.brand);
        copyPaste(kW - kMargin - 12, kPumpY + 3);

        drawPanel(kMargin, kLowY, kTaktW, kLabelH + kCellH, "TAKT", th.brand);
        drawTag(kMargin + kTaktW - 14, kLowY + 13, source == kSourceTrigger ? "TRIGGER AKTIV" : source == kSourceMidi ? "MIDI AKTIV" : "TAKT AKTIV");
        for (int i = 0; i < 4; ++i)
            knob(kTaktKnobs[i], kGridX + i * kCellW, kLowY + kLabelH, th.brand);

        drawCurve(kMargin + kTaktW + 12, kLowY, kW - 2 * kMargin - kTaktW - 12, kLabelH + kCellH, source);
    }

    void onButton(int id) override
    {
        if (id >= 100 && id < 100 + kSourceCount)
            setValue(kSource, (float)(id - 100), true);
    }

    const char* controlLabel(int p) const override
    {
        if (p == kSmooth)
            return "Smooth";
        return kParams[p].name;
    }

private:
    NanoImage lowrider;

    // Pump-Hüllkurve über eine Periode (Takt-Quellen) bzw. 400 ms (Trigger/MIDI), in dB;
    // rechts daneben die aktuelle Absenkung als Balken.
    void drawCurve(float x, float y, float w, float h, int source)
    {
        drawPanel(x, y, w, h, "KURVE", th.brand);
        const bool clock = source == kSourceHost || source == kSourceBpm;
        const float periodMs = clock ? kRateBeats[(int)values[kRate]] * 60000.0f / values[kBpm] : 400.0f;
        const float depth = values[kDepth], duck = std::pow(10.0f, -depth / 20.0f), floorDb = -std::fmax(12.0f, depth) - 3.0f;
        const float smooth = clock ? std::fmin(values[kSmooth], 0.5f * periodMs) : values[kSmooth];
        const float gx = x + 14, gy = y + kLabelH + 6, gw = w - 60, gh = h - kLabelH - 30;

        auto gainAt = [&](float t) { // t in ms ab dem Schlag bzw. Auslöser
            float g;
            if (clock) {
                g = t >= values[kLength] ? 1.0f : duck + (1.0f - duck) * Pump::recovery(t / values[kLength], values[kShape]);
                const float until = periodMs - t;
                if (until < smooth)
                    g = std::fmin(g, 1.0f - (1.0f - duck) * (0.5f - 0.5f * std::cos(kit::kPi * (1.0f - until / smooth))));
            } else {
                g = t < smooth ? 1.0f + (duck - 1.0f) * (0.5f - 0.5f * std::cos(kit::kPi * t / smooth))
                  : (t - smooth >= values[kLength] ? 1.0f
                     : duck + (1.0f - duck) * Pump::recovery((t - smooth) / values[kLength], values[kShape]));
            }
            return gy + gh * std::fmin(1.0f, (20.0f * std::log10(g + 1e-6f)) / floorDb);
        };

        beginPath(); // Raster: 0 dB, −Depth
        moveTo(gx, gy);
        lineTo(gx + gw, gy);
        moveTo(gx, gy + gh * (-depth / floorDb));
        lineTo(gx + gw, gy + gh * (-depth / floorDb));
        strokeColor(kit::withAlpha(th.dim, 0.25f));
        strokeWidth(1);
        stroke();

        beginPath();
        moveTo(gx, gy);
        for (int i = 0; i <= 120; ++i)
            lineTo(gx + gw * i / 120.0f, gainAt(periodMs * i / 120.0f));
        lineTo(gx + gw, gy);
        closePath();
        fillPaint(linearGradient(0, gy, 0, gy + gh, kit::withAlpha(th.brand, 0.05f), kit::withAlpha(th.brand, 0.45f)));
        fill();
        beginPath();
        moveTo(gx, gainAt(0));
        for (int i = 1; i <= 120; ++i)
            lineTo(gx + gw * i / 120.0f, gainAt(periodMs * i / 120.0f));
        strokeColor(th.brand);
        strokeWidth(2.5f);
        stroke();

        fontSize(9);
        textAlign(ALIGN_LEFT | ALIGN_TOP);
        fillColor(th.dim);
        char label[48];
        std::snprintf(label, sizeof(label), "%.0f ms  \xc2\xb7  \xe2\x88\x92%.0f dB", periodMs, depth);
        text(gx, gy + gh + 6, label, nullptr);

        // Aktuelle Absenkung (Ausgabe-Parameter "Duck")
        const float bx = x + w - 34, bw = 14, duckDb = values[kDuck];
        beginPath();
        roundedRect(bx, gy, bw, gh, 4);
        fillColor(th.track);
        fill();
        const float fillH = gh * std::fmin(1.0f, duckDb / floorDb);
        if (fillH > 0.5f) {
            beginPath();
            roundedRect(bx, gy, bw, fillH, 4);
            fillColor(th.brand);
            fill();
        }
        char db[16];
        std::snprintf(db, sizeof(db), "%.0f", duckDb);
        textAlign(ALIGN_CENTER | ALIGN_TOP);
        fillColor(th.text);
        text(bx + bw / 2, gy + gh + 6, db, nullptr);
    }

    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(HydraulicsUI)
};

UI* createUI()
{
    return new HydraulicsUI();
}

END_NAMESPACE_DISTRHO
