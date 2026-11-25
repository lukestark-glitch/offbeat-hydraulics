# Offbeat Hydraulics – Regler und Anleitung

Hydraulics duckt alles, was durch das Plugin läuft (Bass, Pads, Leads), im Takt der Kick: das typische Pumpen.

**Bedienung** (LV2/VST3, wie bei Offbeat Engine):
- Regler hoch/runter ziehen, Shift = fein, Mausrad, Doppelklick = Standardwert.
- Klick auf den Wert oder Rechtsklick: Zahl eintippen.
- Die Quelle wählst du oben mit den Pillen.
- Rechts unten zeigt **KURVE** die Pump-Hüllkurve aus deinen Reglern und als Balken die aktuelle Absenkung.
- **KOPIEREN / EINFÜGEN:** Einstellungen als JSON über die Zwischenablage.

In LMMS (LADSPA) gibt es nur die generischen Regler von LMMS.

## Regler

| Regler | Bereich | Standard | Wirkung |
|---|---|---|---|
| Source | Trigger / Host-Takt / BPM-Takt / MIDI | Trigger | Was das Pumpen auslöst (siehe unten) |
| Trigger | 0 … 1 | 0 | Steigt der Wert über 0,5, pumpt es einmal; erst unter 0,3 wieder scharf. Hier wird der LMMS-Peak-Controller angeschlossen |
| Depth | 0 … 36 dB | 12 dB | Wie tief geduckt wird; 0 = aus |
| Shape | 0 … 100 % | 50 % | 0 = gleichmäßige Erholung, 100 = S-Kurve (bleibt länger unten, kommt dann schnell hoch) |
| Length | 20 … 1000 ms | 250 ms | Wie lange die Erholung dauert. Bei 160 BPM und 1/4 bis ~350 ms, sonst ist sie vor dem nächsten Schlag nicht fertig |
| Rate | 1/1 … 1/16 | 1/4 | Abstand der Pumps (nur Host-Takt und BPM-Takt) |
| Smooth | 0,5 … 20 ms | 3 ms | Wie weich die Absenkung einsetzt (gegen Klicks) |
| Phase | 0 … 100 % | 0 % | Verschiebt das Pumpen um einen Anteil der Rate (nur Takt-Quellen) |
| BPM | 60 … 220 | 160 | Tempo für BPM-Takt bzw. wenn der Host kein Tempo liefert |
| Duck | Anzeige | | Aktuelle Absenkung in dB |

## Quellen

- **Trigger** (empfohlen für LMMS): Pumpt genau dann, wenn die Kick schlägt, auch bei Breaks und Tempowechseln.
- **Host-Takt:** Folgt der Song-Position des Hosts (LV2/VST3 in Carla). Liefert der Host kein Tempo, läuft der Takt mit dem BPM-Regler.
- **BPM-Takt:** Läuft frei mit dem BPM-Regler. Er startet beim Laden des Plugins; mit Phase richtest du ihn auf die Kick aus.
- **MIDI:** Jede Note löst aus (LV2/VST3 in Carla). Im LADSPA gibt es kein MIDI.

## LMMS: Hydraulics auf einer Spur oder dem Master (LADSPA + Peak Controller)

`make install` hat die Datei `offbeat-hydraulics-ladspa.dylib` in deinen LMMS-LADSPA-Ordner gelegt (`~/Documents/lmms/plugins/ladspa/`). Danach LMMS einmal neu starten.

1. **Hydraulics einfügen:** Auf der Spur, die pumpen soll (z. B. Bass), das Effekt-Fenster öffnen (Zahnrad bzw. FX-Kette) und „Add effect“ klicken. Dort **Offbeat Hydraulics** wählen. Für alles gleichzeitig nimmst du den Mixer-Kanal, auf den die Spuren gehen (nicht die Kick).
2. **Quelle prüfen:** In den Hydraulics-Reglern bleibt **Source = 0** (Trigger).
3. **Peak Controller einfügen:** Auf der **Kick-Spur** wie in Schritt 1 einen Effekt hinzufügen, diesmal **Peak Controller**. Er ändert den Klang der Kick nicht.
4. **Peak Controller einstellen:**
   - Base 0, Amount 1 (voll), Attack klein (0–10 %), Decay so, dass der Wert zwischen zwei Kicks unter 0,3 fällt (z. B. 10–20 %).
   - Bei leiser Kick „Amount Multiplicator“ bzw. Amount höher drehen, damit der Wert pro Kick sicher über 0,5 geht.
5. **Verbinden:** Am Hydraulics-Regler **Trigger** rechtsklicken, „Connect to controller…“ wählen und den eben angelegten **Peak Controller** auswählen.
6. **Abspielen:** Bei jeder Kick pumpt die Spur. Depth, Shape und Length stellst du nach Gehör ein.

**Ohne Peak Controller:** Source = 2 (BPM-Takt), BPM auf dein Song-Tempo und Phase so drehen, dass das Pumpen auf der Kick sitzt.

## Carla (LV2/VST3)

1. **Laden:** Im Carla Rack nach dem Instrument (z. B. Offbeat Engine) **Offbeat Hydraulics (LV2)** hinzufügen. Es wirkt nur auf die Klänge aus diesem Carla.
2. **Host-Takt:** Source „Host-Takt“ wählen. Läuft der LMMS-Song, pumpt es auf jedem Viertel. Kommt kein Tempo an, gilt der BPM-Regler.
3. **MIDI:** Source „MIDI“ wählen und Noten an dasselbe Carla schicken. Jede Note löst aus.

## JSON (Kopieren / Einfügen)

Gleiche Regeln wie bei Offbeat Engine:
- Die Schlüssel sind die Kurznamen unten.
- Zahlen stehen in der Einheit des Plugins, %-Regler als 0 … 1.
- Auswahl-Regler gehen als Name (`"source": "BPM-Takt"`, `"rate": "1/8"`).
- Nicht genannte Regler bleiben, wie sie sind.

**Kurznamen:** `source trigger depth shape length rate smooth phase bpm`

Der Renderer liest dasselbe Format: `bin/hydraulics-render --in pad.wav --out pumped.wav --json sound.json`.

## Presets

| Nr. | Name | Idee |
|---|---|---|
| 1 | Kick-Trigger (LMMS Peak Controller) | Standard: Trigger-Regler, −12 dB |
| 2 | Host-Takt 1/4 | Pumpt auf jedem Viertel des Hosts |
| 3 | BPM-Takt 1/4 (160) | Frei mit 160 BPM |
| 4 | Tiefes Pumpen 1/4 | −24 dB, S-Kurve, 300 ms |
| 5 | Sanft 1/4 | −6 dB, kurz und fast linear |
| 6 | Achtel-Pumpen 1/8 | −10 dB auf jeder Achtel |
| 7 | MIDI-Trigger | Noten lösen aus |
