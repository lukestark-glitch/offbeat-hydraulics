// Vorschau von Offbeat Hydraulics: ein eingebautes Demo-Pad (1 Takt bei 160 BPM, mit Offbeat Engine
// erzeugt, assets/demo-pad.raw) ohne und mit Pumpen, dazu die Pump-Kurve. Genutzt von der Oberfläche
// (Wellenform), vom DSP (Vorhören über den Plugin-Ausgang) und von hydraulics-render (--preview-check).
// In der Vorschau pumpt es auf jedem Schlag des Demos, egal welche Quelle eingestellt ist.
#pragma once

#include "Pump.hpp"

#include <vector>

namespace offbeat {

constexpr int kDemoRate = 48000;
constexpr double kDemoBpm = 160.0;
constexpr uint32_t kDemoFrames = 72000; // 4 Schläge bei 160 BPM
constexpr uint32_t kDemoBeat = kDemoFrames / 4;

// Sample des Demo-Pads (−18 dBFS RMS), channel 0 = links, 1 = rechts.
float demoSample(uint32_t frame, int channel);

// Ebene 0 = ohne (Demo-Ausschnitt), 1 = mit Pumpen (Takt des Demos).
void renderPreview(const float* params, int layer, double beats, std::vector<float>& l, std::vector<float>& r);

// Pump-Kurve (Verstärkung 0..1) für denselben Ausschnitt.
void renderPumpCurve(const float* params, double beats, std::vector<float>& gain);

} // namespace offbeat
