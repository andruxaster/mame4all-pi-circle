CPUDEFS += -DHAS_CYCLONE=1 -DHAS_DRZ80=1

OBJDIRS += $(OBJ)/cpu/m68000_cyclone $(OBJ)/cpu/z80_drz80
CPUOBJS += $(OBJ)/cpu/m68000_cyclone/cyclone.o $(OBJ)/cpu/m68000_cyclone/c68000.o
CPUOBJS += $(OBJ)/cpu/z80_drz80/drz80.o $(OBJ)/cpu/z80_drz80/drz80_z80.o

#OBJDIRS += $(OBJ)/cpu/nec_armnec
#CPUDEFS += -DHAS_ARMNEC

#CPUOBJS += $(OBJ)/cpu/nec_armnec/armV30.o $(OBJ)/cpu/nec_armnec/armV33.o $(OBJ)/cpu/nec_armnec/armnecintrf.o

OSOBJS = $(OBJ)/circle/minimal.o \
	$(OBJ)/circle/rpi.o $(OBJ)/circle/video.o $(OBJ)/circle/blit.o \
	$(OBJ)/circle/sound.o $(OBJ)/circle/input.o $(OBJ)/circle/fileio.o \
	$(OBJ)/circle/config.o $(OBJ)/circle/fronthlp.o \
	$(OBJ)/circle/fifo_buffer.o  \
	 \
	$(OBJ)/circle/gp2x_frontend.o
