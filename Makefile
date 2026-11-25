# Offbeat Hydraulics – Build für Plugin (VST3 + LV2 + LADSPA, x86_64) und Offline-Renderer.
#   make            Plugin + hydraulics-render
#   make install    nach ~/Library/Audio/Plug-Ins kopieren (LADSPA auch in den LMMS-LADSPA-Ordner) und ad-hoc signieren
#   make check      Abnahme mit hydraulics-render --check-all

CXX ?= c++
DSP_SRC = src/dsp/Pump.cpp src/dsp/Params.cpp src/dsp/ParamText.cpp
DSP_HDR = $(wildcard src/dsp/*.hpp) tools/Measure.hpp
VST3_DIR   = $(HOME)/Library/Audio/Plug-Ins/VST3
LV2_DIR    = $(HOME)/Library/Audio/Plug-Ins/LV2
LADSPA_DIR = $(HOME)/Library/Audio/Plug-Ins/LADSPA
# LMMS-LADSPA-Ordner aus ~/.lmmsrc.xml (Attribut laddir), sonst der LMMS-Standard
LMMS_LADSPA_DIR := $(shell sed -n 's/.*laddir="\([^"]*\)".*/\1/p' $(HOME)/.lmmsrc.xml 2>/dev/null | head -1)
ifeq ($(LMMS_LADSPA_DIR),)
LMMS_LADSPA_DIR = $(HOME)/Documents/lmms/plugins/ladspa/
endif
LADSPA_FILE = offbeat-hydraulics-ladspa.dylib

all: plugin render

plugin:
	$(MAKE) -C plugin
	$(MAKE) -C plugin LADSPA=1
	$(MAKE) -C dpf/utils/lv2-ttl-generator
	dpf/utils/generate-ttl.sh $(CURDIR)/bin

render: bin/hydraulics-render

bin/hydraulics-render: tools/hydraulics-render.cpp $(DSP_SRC) $(DSP_HDR)
	@mkdir -p bin
	$(CXX) -std=c++17 -O2 -Wall -Wextra -Isrc/dsp -Itools tools/hydraulics-render.cpp $(DSP_SRC) -o $@

check: render
	bin/hydraulics-render --check-all

install: plugin
	rm -rf "$(VST3_DIR)/offbeat-hydraulics.vst3" "$(LV2_DIR)/offbeat-hydraulics.lv2"
	mkdir -p "$(VST3_DIR)" "$(LV2_DIR)" "$(LADSPA_DIR)" "$(LMMS_LADSPA_DIR)"
	cp -R bin/offbeat-hydraulics.vst3 "$(VST3_DIR)/"
	cp -R bin/offbeat-hydraulics.lv2 "$(LV2_DIR)/"
	cp bin/$(LADSPA_FILE) "$(LADSPA_DIR)/$(LADSPA_FILE)"
	rm -f "$(LMMS_LADSPA_DIR)/$(LADSPA_FILE)"  # alte Kopie mit falscher Endung
	cp bin/$(LADSPA_FILE) "$(LMMS_LADSPA_DIR)/offbeat-hydraulics-ladspa.so"  # LMMS lädt nur *.so
	codesign --force --deep -s - "$(VST3_DIR)/offbeat-hydraulics.vst3"
	codesign --force -s - "$(LV2_DIR)/offbeat-hydraulics.lv2/offbeat-hydraulics.dylib"  # LV2-Ordner ist kein Bundle
	codesign --force -s - "$(LADSPA_DIR)/$(LADSPA_FILE)"
	codesign --force -s - "$(LMMS_LADSPA_DIR)/offbeat-hydraulics-ladspa.so"

clean:
	rm -rf bin build

.PHONY: all plugin render check install clean
