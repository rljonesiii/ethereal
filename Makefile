# Project Name
TARGET = ethereal

# Sources
CPP_SOURCES = src/harmonizer.cpp \
	$(DAISYSP_DIR)/Source/Filters/svf.cpp \
	$(DAISYSP_DIR)/DaisySP-LGPL/Source/Effects/reverbsc.cpp \
	$(DAISYSP_DIR)/DaisySP-LGPL/Source/Utility/port.cpp \
	$(DAISYSP_DIR)/Source/Synthesis/oscillator.cpp

# LGPL Includes (and Utility for dsp.h)
C_INCLUDES += -I$(DAISYSP_DIR)/DaisySP-LGPL/Source \
              -I$(DAISYSP_DIR)/Source/Filters \
              -I$(DAISYSP_DIR)/Source/Synthesis \
              -I$(DAISYSP_DIR)/Source/Utility

# Library Locations
LIBDAISY_DIR = ../DaisyExamples/libDaisy
DAISYSP_DIR = ../DaisyExamples/DaisySP

# Core location, and generic Makefile.
SYSTEM_FILES_DIR = $(LIBDAISY_DIR)/core
include $(SYSTEM_FILES_DIR)/Makefile
