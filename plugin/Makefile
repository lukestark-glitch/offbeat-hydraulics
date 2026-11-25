#!/usr/bin/make -f
# DPF-Build des Plugins. Aufruf über das Makefile im Projektordner:
#   make -C plugin             VST3 + LV2 (mit Oberfläche, MIDI und Host-Takt)
#   make -C plugin LADSPA=1    LADSPA für LMMS (eigener Build: ohne MIDI, Zeitinfo und Oberfläche)

NAME = offbeat-hydraulics

FILES_DSP = \
	HydraulicsPlugin.cpp \
	../src/dsp/Pump.cpp \
	../src/dsp/Params.cpp \
	../src/dsp/ParamText.cpp

DPF_PATH = ../dpf
DPF_TARGET_DIR = ../bin

ifeq ($(LADSPA),1)
DPF_BUILD_DIR = ../build/offbeat-hydraulics-ladspa
else
DPF_BUILD_DIR = ../build/offbeat-hydraulics
# Params.cpp und ParamText.cpp nutzt auch die UI; sie steckt bei VST3 und LV2 (eine Datei) im selben Binary.
FILES_UI = HydraulicsUI.cpp
endif

include ../dpf/Makefile.plugins.mk

BUILD_CXX_FLAGS += -std=gnu++17 -I../src/dsp -I$(DPF_BUILD_DIR)

ifeq ($(LADSPA),1)
BUILD_CXX_FLAGS += -DOFFBEAT_LADSPA
TARGETS += ladspa
else
# Lowrider-Bild für die Oberfläche als C-Array einbetten
$(DPF_BUILD_DIR)/LowriderImage.h: ../assets/lowrider.png
	-@mkdir -p $(DPF_BUILD_DIR)
	cd ../assets && xxd -i lowrider.png > $(abspath $@)

$(DPF_BUILD_DIR)/HydraulicsUI.cpp.o: $(DPF_BUILD_DIR)/LowriderImage.h

TARGETS += vst3
TARGETS += lv2
endif

all: $(TARGETS)
