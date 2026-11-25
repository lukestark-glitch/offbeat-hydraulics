# Handoff – Offbeat Hydraulics

**Fertig**
- Pump-Effekt mit vier Quellen: Trigger (Standard, für den LMMS-Peak-Controller), Host-Takt, BPM-Takt und MIDI.
- Regler Depth, Shape, Length, Rate, Smooth, Phase und BPM, dazu 7 Presets.
- Formate: LV2 und VST3 mit Oberfläche (violett, Lowrider, Live-Kurve, Absenkungs-Balken), außerdem LADSPA für LMMS-Spuren und den Master.
- `make check` prüft Tiefe, Takt-Genauigkeit, Erholung, Klickfreiheit, Trigger-Flanken und Grenzfälle.
- `make install` legt die Dateien nach ~/Library/Audio/Plug-Ins und LADSPA zusätzlich in den LMMS-LADSPA-Ordner.

**Als Nächstes**
- Hörtest in LMMS mit Peak Controller auf der Kick (Anleitung in `docs/REGLER.md`).
- Prüfen, ob „Host-Takt“ in Carla dem LMMS-Tempo folgt.
