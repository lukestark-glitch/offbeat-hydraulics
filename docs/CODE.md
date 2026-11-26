# Offbeat Hydraulics – Code-Doku

## Überblick

```
offbeat-hydraulics/
├── src/dsp/
│   ├── Params.hpp/.cpp   Regler, Quellen (kSourceNames), Raten (kRateBeats), Presets, jsonParams()
│   ├── Pump.hpp/.cpp     Pump: Auslöser (Takt, Trigger-Flanke, MIDI) → Gain-Kurve → Signal × Gain
│   └── Blocks.hpp, ParamInfo.hpp, ParamText.*   gemeinsame Bausteine
├── plugin/
│   ├── DistrhoPluginInfo.h  Name, URI, IDs; mit OFFBEAT_LADSPA ohne MIDI, Zeitinfo und Oberfläche
│   ├── HydraulicsPlugin.cpp DPF-Hülle: Parameter, Presets, MIDI-Noten → trigger(), Host-Takt → setTransport()
│   ├── HydraulicsUI.cpp     Oberfläche: Quellen-Pillen, Regler, Kurve und Duck-Anzeige, Lowrider
│   ├── Makefile             zwei Builds: normal (VST3/VST2/AU/CLAP/LV2) und LADSPA=1
│   └── OffbeatKit.hpp, OffbeatParam.hpp
├── tools/hydraulics-render.cpp  Testsignale/eigene WAVs pumpen, --check-all
└── assets/lowrider.png
```

## Die Pump-Kurve

`Pump::process()` berechnet für jedes Sample eine Verstärkung `g` zwischen `duck = 10^(−Depth/20)` und 1:

```
Takt-Quellen (Host-Takt, BPM-Takt), zustandslos aus der Taktposition:
  u      = Position im Raster (beat / Rate − Phase, nur der Nachkommaanteil)
  since  = Samples seit dem letzten Raster-Schlag,  until = Samples bis zum nächsten
  g      = duck + (1 − duck) · recovery(since / Length, Shape)   (nach Length: 1)
  vor dem Schlag: über "Smooth" mit einem Raised-Cosine nach unten → genau auf dem Schlag am tiefsten

Auslöser-Quellen (Trigger-Flanke, MIDI), ohne Vorwarnung:
  0 … Smooth:          Raised-Cosine vom aktuellen Gain auf duck
  danach … Length:     duck + (1 − duck) · recovery(t / Length, Shape)
  danach:              g = 1 (exakt unverändert)

recovery(x, Shape) = x + Shape · (smootherstep(x) − x)
  Shape 0 → linear, Shape 1 → S-Kurve (bleibt länger unten, kommt dann schnell hoch)
```

**Trigger-Flanke:** Pro Block wird `trigger` geprüft. Über 0,5 löst es aus, aber nur, wenn vorher entschärft wurde. Unter 0,3 ist es wieder scharf (Hysterese gegen Flattern). Der LMMS-Peak-Controller ändert den Wert einmal pro Audio-Puffer, also etwa alle 5 ms; das Pumpen kommt darum höchstens einen Puffer nach der Kick.

**Host-Takt:** `setTransport()` bekommt vom Plugin die Schlagposition aus BBT. Ohne gültige Zeitinfo (oder in LADSPA) läuft der Takt mit dem BPM-Regler weiter.

Die Duck-Anzeige ist der tiefste Gain des Blocks in dB.

## Zwei Builds wegen LADSPA

DPF übersetzt die Plugin-Datei für alle Formate gemeinsam. LADSPA verbietet aber MIDI und Zeitinfo. Deshalb baut `plugin/Makefile` mit `LADSPA=1` ein zweites Mal, in einen eigenen Build-Ordner und mit `-DOFFBEAT_LADSPA`. `DistrhoPluginInfo.h` schaltet dann MIDI, Zeitinfo und Oberfläche ab, und `HydraulicsPlugin::run()` hat dafür einen eigenen Zweig. Das Makefile im Hauptordner ruft beide Builds auf.

LMMS lädt LADSPA nur mit der Endung `.so`, und es kennt nur grobe Standard-Stufen. Die Bereiche sind so gewählt, dass jeder Standard genau auf einer Stufe liegt. `ladspaDefault()` im Renderer prüft das (siehe Code-Doku von offbeat-turbo).

## Anpassen

- **Andere Kurvenform:** `Pump::recovery()` ändern. Die Oberfläche zeichnet dieselbe Funktion, die Anzeige stimmt also automatisch.
- **Neue Rate:** `kRateBeats` und `kRateNames` in `Params.cpp` erweitern und `rate` max erhöhen. Für LMMS muss der Standard dann wieder auf einer LADSPA-Stufe liegen.
- **Neue Quelle:** `kSourceCount`/`kSourceNames` erweitern, dann in `process()` den Fall behandeln und in `HydraulicsUI` einen Pill-Knopf ergänzen.
- **Regler:** `ParamId`/`kParams` **hinten** anhängen, `make check` laufen lassen.

## hydraulics-render

```sh
bin/hydraulics-render --check-all                      # = make check
bin/hydraulics-render --in pad.wav --out pumped.wav --preset 2
bin/hydraulics-render --in pad.wav --out pumped.wav --json pump.json
bin/hydraulics-render --preset-json 3
```

`--check-all` erzeugt die Testsignale selbst und prüft:
- **Absenkung:** genau Depth am Schlag,
- **Takt:** der tiefste Punkt liegt auf dem Schlag, bei BPM-Takt 1/4 und 1/8, Phase und Host-Takt,
- **Erholung** vor dem nächsten Schlag,
- **Trigger:** genau ein Pump pro Flanke, auch bei flatterndem Trigger,
- **Klickfreiheit:** größter Sprung der Gain-Kurve pro Sample,
- **Depth 0:** bit-identisch mit dem Eingang,
- **Stille und Zahlen:** Stille bleibt Stille, kein NaN, keine Denormals,
- **LMMS:** die LMMS-Standardwerte.

## Gemeinsame Bausteine der Offbeat-Familie

Diese Dateien sind in allen vier Repos **gleich**. Sie sind eine bewusste Kopie; die Quelle ist [offbeat-engine](https://github.com/lukestark-glitch/offbeat-engine). Änderst du sie, dann zuerst dort und danach in die anderen Repos kopieren.

| Datei | Inhalt |
|---|---|
| `src/dsp/ParamInfo.hpp` | `ParamInfo`: Name, Kurzname (`symbol`), Einheit, Bereich, Standard, Flags (`kFlagLog`, `kFlagBool`, `kFlagInt`, `kFlagOutput`), Auswahl-Namen (`labels`) |
| `src/dsp/Blocks.hpp` | DSP-Bausteine: `Osc` (PolyBLEP-Saw/Square), `Sine`, `Tri`, `Noise`, `Svf` (TPT-State-Variable-Filter), `Smoother`, `Adsr`, `Chorus`, `DenormalGuard` (Flush-to-Zero auf Intel und ARM), Helfer wie `midiToHz`, `decayCoef`, `softLimit` |
| `src/dsp/ParamText.*` | Text ↔ Wert: `parseParamText` (Zahleneingabe am Regler), `applyJson`/`toJson` (KOPIEREN/EINFÜGEN, `--json`, `--preset-json`) |
| `plugin/OffbeatParam.hpp` | `fillParameter`: macht aus einem `ParamInfo` einen DPF-`Parameter` (Hints, Bereich, Auswahl-Werte) |
| `plugin/OffbeatKit.hpp` | `OffbeatKitUI`: Look und Bedienung aller Oberflächen (siehe unten) |
| `tools/Measure.hpp` | Messwerkzeuge der Renderer: WAV lesen/schreiben, Tonhöhe, Pegel, Spektrum, L/R-Korrelation |

### Ein Parameter, viele Stellen: alles kommt aus `kParams`

```
src/dsp/Params.cpp  kParams[]  ──┬─► DSP (Engine/Turbo/Pump liest params[id])
                                 ├─► plugin/*Plugin.cpp: initParameter() → fillParameter() → Host (VST3/AU/CLAP/LV2/…)
                                 ├─► OffbeatKitUI: Name, Bereich, Einheit, Anzeige, Zahleneingabe
                                 ├─► ParamText: JSON-Schlüssel = symbol
                                 └─► Renderer: --set symbol=wert, --list
```

Einen Regler hinzufügen heißt deshalb nur: Eintrag in `ParamId` (Params.hpp) **hinten anhängen**, Zeile in `kParams` (Params.cpp), im DSP benutzen und in der Oberfläche platzieren. Host-Anbindung, JSON und Zahleneingabe funktionieren dann von allein.

> **Nie** bestehende Einträge umsortieren oder löschen. Die Position ist die Parameter-ID in allen Formaten; gespeicherte Projekte würden sonst falsche Werte laden. Ebenso nie die Plugin-URI, VST3/AU-IDs (`DistrhoPluginInfo.h`) oder den Bundle-Namen (`plugin/Makefile: NAME`) ändern.

### OffbeatKitUI (`plugin/OffbeatKit.hpp`)

Die Oberflächen sind ein „Immediate-Mode“-GUI. `onNanoDisplay()` zeichnet bei jedem Bild alles neu. Jeder Baustein, der dabei gezeichnet wird, meldet seine Klickfläche an. Maus und Tastatur arbeiten dann mit den Flächen des zuletzt gezeichneten Bildes. Ein Plugin leitet von `OffbeatKitUI` ab und überschreibt:

| Methode | Zweck |
|---|---|
| `drawContent()` | alles zeichnen (Pflicht) |
| `onButton(id)` | eigene Knöpfe, z. B. Modus-Reiter |
| `controlLabel(p)` | kürzerer Name für einen Regler |

Bausteine:

| Baustein | Was er zeichnet |
|---|---|
| `drawBackground()` | Verlauf und Lochblech (automatisch) |
| `drawLogo(cx, cy)` | Chrom-Plakette mit Swoosh |
| `drawPanel(x, y, w, h, label, farbe)` | Abschnitt im Ticket-Stil |
| `knob(p, x, y, farbe)` | Regler in einer Zelle von 114 × 112 Pixeln; Schalter werden automatisch zur Pille |
| `bigKnob(p, cx, cy, r, farbe)` | großer Regler im Manometer-Look |
| `compact(p, x, y, w, farbe)` | kleiner Regler für Kopfzeilen |
| `pill(id, x, y, w, h, text, farbe, aktiv)` | Knopf |
| `copyPaste(xRechts, y)` | KOPIEREN/EINFÜGEN mit Statuszeile |
| `drawImage(bild, …)` | eingebettetes Bild |
| `drawTag(…)` | kleines Etikett |

Farben und Schriftzug kommen aus einem `kit::Theme`. Die Werte liegen in `values[]` und werden vom Host über `parameterChanged()` aktuell gehalten; `setValue()` schickt eine Änderung an den Host.

**Bedienung:**
- ziehen (mit Shift fein), Mausrad,
- Doppelklick setzt den Standard,
- Klick auf den Wert oder Rechtsklick öffnet die Zahleneingabe,
- Schalter per Klick,
- KOPIEREN/EINFÜGEN tauscht JSON über die Zwischenablage aus; welche Regler darin stehen, legt `jsonParams()` in Params.cpp fest.

**Bilder der Oberfläche** (für die Doku): `make standalone`, dann

```sh
OFFBEAT_SCREENSHOT=ui.ppm OFFBEAT_SCREENSHOT_PRESET=2 bin/<name>.app/Contents/MacOS/<name>
```

Die Oberfläche lädt das Preset, speichert sich als PPM und beendet sich. Die Bilder in `docs/images/` sind so entstanden (PPM → PNG umgewandelt).

### Formate und Build

`plugin/Makefile` nutzt DPF (`dpf/Makefile.plugins.mk`) und baut VST3, VST2, CLAP, LV2 und auf dem Mac AU, bei Turbo und Hydraulics dazu LADSPA. Das Makefile im Hauptordner ruft es auf und baut auf dem Mac ein Universal-Binary (`-arch x86_64 -arch arm64`, `NOOPT=true`). Außerdem erzeugt es die LV2-Beschreibungen (`generate-ttl.sh`) und baut den Offline-Renderer.

Das Bild in `assets/` wird beim Build mit `xxd -i` zu einem C-Array und fest in die Oberfläche eingebaut.

`.github/workflows/build.yml` baut bei jedem Push auf macOS, Windows (MSYS2/MinGW) und Linux und führt `make check` aus. Bei einem Tag `v*` legt es ein Release mit den ZIPs an.

### Echtzeit-Regeln im DSP

- In `process()` kein `new`/`malloc`, keine Locks, keine Datei-Zugriffe. Puffer werden in `setSampleRate()` angelegt.
- Am Anfang jedes `process()` steht `const DenormalGuard denormals;` (Flush-to-Zero).
- Parameter werden in `setParam()` auf ihren Bereich begrenzt; Sprünge glättet `Smoother`.
- Der Ausgang läuft am Ende durch `softLimit()` (Instrumente) bzw. das Ceiling (Turbo); es gibt kein hartes Übersteuern.

### Prüfen

`make check` startet den Offline-Renderer mit `--check-all`. Er rechnet Presets bzw. Testsignale ohne DAW, misst sie (Tonhöhe, Länge, Spektrum, L/R-Korrelation, True-Peak, …) und vergleicht sie mit festen Zielen. Jede Änderung am Klang sollte danach noch grün sein, oder die Ziele werden bewusst angepasst.
