# Plan – Offbeat Hydraulics

Pump-/Sidechain-Effekt der Offbeat-Familie. Er duckt Bass, Pads und Leads im Takt der Kick, die Regler sind Tiefe, Kurvenform, Länge, Rate und Glättung. LV2 und VST3 haben eine eigene Oberfläche, Farbe Violett, mit dem Lowrider-Bild. Für LMMS-Spuren und den Master gibt es zusätzlich **LADSPA**.

## Machbarkeit: Wie bleibt das Pumpen im Takt?

Ausgangslage in LMMS 1.2.2:
- **LADSPA** ist der einzige Weg auf beliebige LMMS-Spuren und den Master. LADSPA kennt kein Tempo, keine Taktposition und kein MIDI.
- **Carla Rack** ist in LMMS ein *Instrument*. Ein Effekt darin wirkt nur auf Klänge aus demselben Carla. Ob Carla das LMMS-Tempo an Plugins durchreicht, ist ungeklärt; LMMS selbst liefert es laut seinen Symbolen an Carla.

**Entscheidung:** Der Regler **Source** hat vier Quellen. Standard ist die robusteste:

| Source | Wie | Wo | Warum |
|---|---|---|---|
| **Trigger** (Standard) | Der Regler `trigger` löst bei einer steigenden Flanke über 0,5 einmal aus. Neu scharf wird er erst unter 0,3 (Hysterese). In LMMS kommt auf die Kick-Spur der eingebaute **Peak Controller**, verbunden mit dem Trigger-Regler. | LADSPA auf jeder LMMS-Spur und dem Master; geht auch in LV2/VST3 | Echtes Sidechain auf die tatsächliche Kick, unabhängig vom Tempo. Funktioniert auch bei Tempowechseln, Breaks und ungeraden Kick-Mustern. |
| **Host-Takt** | Schlagposition vom Host, wie in Offbeat Engine. Ohne gültiges Host-Tempo läuft der Takt frei mit dem BPM-Regler. | LV2/VST3 in Carla | Bequem, wenn Carla das Tempo durchreicht |
| **BPM-Takt** | Freilaufend mit den Reglern BPM und Phase (0–100 % einer Periode). Der Takt startet beim Laden des Plugins. | überall | Fallback ohne Host-Tempo und ohne Peak Controller. Die Phase muss man einmal von Hand ausrichten. |
| **MIDI** | Jede Note-On löst genau auf dem Sample aus. | LV2/VST3 in Carla | Kick-Noten (oder eine eigene Trigger-Spur) an dasselbe Carla schicken |

Im LADSPA-Build gibt es kein MIDI und keine Zeitinfo. „Host-Takt“ läuft dort automatisch mit dem BPM-Regler, „MIDI“ löst nie aus.

## Signalweg und Kurve

Stereo-Verstärkung, beide Seiten gleich. Latenz 0. Alles wird gleich geduckt, auch unter 150 Hz.

**Takt-Quellen** (Host, BPM):
- Die Kurve wird ohne inneren Zustand aus der Taktposition berechnet.
- Die Absenkung beginnt **Smooth** vor dem Schlag (Raised Cosine) und ist genau auf dem Schlag bei **−Depth**.
- Danach erholt sie sich über **Length**.
- Rate wählt den Abstand (1/1, 1/2, 1/4, 1/8, 1/16), Phase verschiebt um einen Anteil der Periode.

**Auslöser-Quellen** (Trigger, MIDI):
- Der Auslöser kommt ohne Vorwarnung. Darum fällt die Kurve ab dem Auslöser über Smooth vom aktuellen Pegel auf −Depth; ein Neu-Auslösen springt also nicht.
- Danach erholt sie sich über Length.

**Shape:**
- 0 = lineare Erholung (in der Amplitude).
- 1 = S-Kurve (Smootherstep): bleibt länger unten und kommt dann schnell hoch.

**Ruhe:** Ist die Kurve fertig, ist die Verstärkung exakt 1. Bei Depth 0 ist der Ausgang bit-identisch mit dem Eingang.

**Anzeige:** Ausgabe-Parameter **Duck**, die tiefste Absenkung im letzten Block in dB.

## Regler

| Regler | Bereich | Standard |
|---|---|---|
| Source | Trigger / Host-Takt / BPM-Takt / MIDI | Trigger |
| Trigger | 0 … 1 | 0 |
| Depth | 0 … 36 dB | 12 dB |
| Shape | 0 … 1 | 0,5 |
| Length | 20 … 1000 ms | 250 ms (bei 160 BPM vor dem nächsten Viertel erholt) |
| Rate | 1/1 … 1/16 | 1/4 |
| Smooth | 0,5 … 20 ms | 3 ms |
| Phase | 0 … 100 % | 0 |
| BPM | 60 … 220 | 160 |
| Duck (Anzeige) | −40 … 0 dB | |

Kein Mix-Regler: Depth regelt die Stärke stufenlos bis 0.

## Abnahme (`make check` = `hydraulics-render --check-all`)

Die Testkurve entsteht so: Ein Eingang mit konstant 0,5 wird in Blöcken à 256 Samples gerechnet, wie im Host.

| Prüfung | Ziel |
|---|---|
| Absenkung am Schlag | = Depth ±1 dB (BPM 1/4 und 1/8, Phase 25 %, Host-Takt 150 BPM ab Schlag 3,37, Host ohne Tempo → BPM-Regler, MIDI, Trigger) |
| Tiefster Punkt | Takt-Quellen genau auf dem Schlag ±2 ms; Trigger/MIDI bei Auslöser + Smooth ±2 ms |
| Erholung | Kurz vor der nächsten Absenkung > −1 dB (Standard-Length bei 1/4) |
| Keine Klicks | Größter Sprung der Kurve < 0,01 pro Sample (Standard-Smooth) |
| Trigger | Genau ein Pumpen pro Flanke; Flattern zwischen 0,35 und 0,7 löst nur einmal aus |
| Grenzfälle | Depth 0 bit-identisch; Stille bleibt Stille; kein NaN, keine Denormals (Depth 36 dB, Smooth 0,5 ms) |

`hydraulics-render --in datei.wav --out datei.wav [--bpm 160] [--kicks 0,375,...]` rechnet eigene Dateien.

## IDs (nie mehr ändern)

- URI `https://github.com/lukestark-glitch/offbeat-hydraulics`
- VST3 `Ofbt`/`OfbH`
- Bundle `offbeat-hydraulics`, LADSPA-Datei `offbeat-hydraulics-ladspa.dylib`

**Gemeinsamer Code** aus offbeat-engine (bewusste Kopie): `ParamInfo.hpp`, `ParamText.*`, `Blocks.hpp`, `OffbeatKit.hpp`, `OffbeatParam.hpp`, `tools/Measure.hpp`.

**LADSPA-Build:** Er ist getrennt (`make -C plugin LADSPA=1`, eigener Build-Ordner, `-DOFFBEAT_LADSPA`). DPF übersetzt die Plugin-Datei nur einmal für alle Formate, LADSPA verbietet aber MIDI. Darum wird sie mit abgeschaltetem MIDI, Zeitinfo und Oberfläche ein zweites Mal übersetzt.

## Offene Fragen

- **Pump-Sektion direkt in Engine/Topline** (Alternative): Das wäre ohne Verkabelung immer im Takt der eigenen Noten. Es wirkt aber nur auf den eigenen Klang, dupliziert Code in zwei Plugins und hilft nicht beim Master. Darum vorerst nicht; nachrüstbar, weil `Pump` ohne DPF auskommt.
- **Nur Host-Takt** (Alternative): Das wäre am einfachsten, hängt aber davon ab, dass Carla in LMMS das Tempo durchreicht (ungeprüft), und geht in LADSPA gar nicht. Darum nur als eine Quelle unter vieren.
- **Peak Controller in LMMS:**
  - Er arbeitet pro Audio-Block. Die Auslösung kann darum bis zu einen Block (bei 256 Samples etwa 5 ms) nach der Kick kommen. Mit Smooth 3 ms ist das im Mix kaum hörbar.
  - Die Peak-Controller-Einstellungen (Attack/Decay/Amount) muss der Nutzer so wählen, dass der Wert pro Kick sauber über 0,5 und danach unter 0,3 geht (Anleitung in `docs/REGLER.md`).
- **Standard-Source Trigger:** Ohne verbundenen Controller pumpt das Plugin nicht. Das ist die vorsichtigere Wahl; „BPM-Takt 1/4 (160)“ ist ein Preset.
- **BPM-Takt ohne Ausrichtung:** Der freie Takt startet beim Laden des Plugins, nicht beim Song-Start. Die Phase muss man von Hand stellen. Darum ist Trigger der Standard.
- **Repo öffentlich:** Der Nutzer hat das Repo schon öffentlich angelegt. Die Sichtbarkeit bleibt, wie sie ist.
