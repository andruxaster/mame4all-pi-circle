#
# Makefile - MAME4ALL kernel for Raspberry Pi Zero / Pi 1 (Circle + circle-stdlib)
#
include ../circle-stdlib/Config.mk

MAMEDIR = ../mame4all-pi

OBJS	= main.o kernel.o alignfix.o alignfix_s.o bigheap.o

include $(CIRCLEHOME)/Rules.mk

INCLUDE := $(CIRCLE_STDLIB_INCLUDES) $(INCLUDE) -I $(MAMEDIR)/src/circle

LIBS := $(MAMEDIR)/libmame.a $(CIRCLEHOME)/lib/sound/libsound.a $(CIRCLE_STDLIB_LIBS)

# large blocks come from bigheap.cpp, Circle's heap does not reuse them
LDFLAGS += --wrap=malloc --wrap=free --wrap=calloc --wrap=realloc --wrap=memalign

-include $(DEPS)
