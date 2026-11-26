#ifndef DISTRHO_PLUGIN_INFO_H_INCLUDED
#define DISTRHO_PLUGIN_INFO_H_INCLUDED

// URI, IDs und Bundle-Name (plugin/Makefile: NAME) ab dem ersten Release nie mehr ändern:
// gespeicherte LMMS-/Carla-Projekte finden das Plugin darüber.
#define DISTRHO_PLUGIN_BRAND   "Offbeat"
#define DISTRHO_PLUGIN_NAME    "Offbeat Hydraulics"
#define DISTRHO_PLUGIN_URI     "https://github.com/lukestark-glitch/offbeat-hydraulics"
#define DISTRHO_PLUGIN_CLAP_ID "com.github.lukestark-glitch.offbeat-hydraulics"

// VST2 kennt keine Anzeige-Werte; DPF zeigt dort sonst nur Parameter vor dem ersten Anzeige-Wert, und
// die Vorhör-Parameter (hinter „Duck“ angehängt) würden fehlen. So zeigt VST2 alle Parameter.
#define DPF_VST_SHOW_PARAMETER_OUTPUTS 1

#define DISTRHO_PLUGIN_BRAND_ID  Ofbt
#define DISTRHO_PLUGIN_UNIQUE_ID OfbH

#define DISTRHO_PLUGIN_IS_RT_SAFE    1
#define DISTRHO_PLUGIN_IS_SYNTH      0
#define DISTRHO_PLUGIN_NUM_INPUTS    2
#define DISTRHO_PLUGIN_NUM_OUTPUTS   2
#define DISTRHO_PLUGIN_WANT_PROGRAMS 1

// LADSPA (eigener Build mit OFFBEAT_LADSPA, siehe plugin/Makefile) kennt weder MIDI noch Zeitinfo
// noch eine eigene Oberfläche: dort gibt es nur die Quellen Trigger und BPM-Takt.
#ifdef OFFBEAT_LADSPA
#define DISTRHO_PLUGIN_HAS_UI           0
#define DISTRHO_PLUGIN_WANT_MIDI_INPUT  0
#define DISTRHO_PLUGIN_WANT_TIMEPOS     0
#else
#define DISTRHO_PLUGIN_HAS_UI           1
#define DISTRHO_PLUGIN_WANT_MIDI_INPUT  1
#define DISTRHO_PLUGIN_WANT_TIMEPOS     1
#define DISTRHO_UI_USE_NANOVG           1
#define DISTRHO_UI_USER_RESIZABLE       0
#define DISTRHO_UI_DEFAULT_WIDTH        860
#define DISTRHO_UI_DEFAULT_HEIGHT       548
#endif

#define DISTRHO_PLUGIN_LV2_CATEGORY    "lv2:DynamicsPlugin"
#define DISTRHO_PLUGIN_VST3_CATEGORIES "Fx|Dynamics|Stereo"

#endif // DISTRHO_PLUGIN_INFO_H_INCLUDED
