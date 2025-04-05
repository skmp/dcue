TARGET ?= tlg-sim.elf

all: $(TARGET) repacker

# include common.mk

OBJS = ../main.o ../vendor/gldc/alloc.o

OBJS_SIM=$(OBJS:.o=.sim.o) \
	../vendor/koshle/hlekos.sim.o \
	../vendor/koshle/hlematrix3d.sim.o \
	../vendor/koshle/hlepvr_mem.sim.o \
	../vendor/koshle/hlepvr_prim.sim.o \
	../vendor/koshle/hlepvr_scene.sim.o \
	../vendor/koshle/hlepvr_misc.sim.o \
	../vendor/koshle/hlepvr_init_term.sim.o \
	../vendor/koshle/hlepvr_buffers.sim.o \
	../vendor/koshle/hlepvr_irq.sim.o \
	../vendor/koshle/hlepvr_fog.sim.o \
	\
	../vendor/emu/emu/window.sim.o \
	\
	../vendor/emu/lxdream/tacore.sim.o3 \
	\
	../vendor/emu/refsw/pvr_mem.sim.o3 \
	../vendor/emu/refsw/pvr_regs.sim.o3 \
	../vendor/emu/refsw/refsw_lists.sim.o3 \
	../vendor/emu/refsw/refsw_tile.sim.o3 \
	../vendor/emu/refsw/TexUtils.sim.o3 \
	
DEPS_SIM1=$(OBJS_SIM:.o=.d)
DEPS_SIM=$(DEPS_SIM1:.o3=.d)

CXXFLAGS+= -MMD -MP

%.sim.o: %.c
	$(CC) -c -O0 -g -fno-pic -no-pie -o $@ $(CFLAGS) -I../vendor/koshle -I../vendor/emu -DDC_SIM $<
%.sim.o: %.cpp
	$(CXX) -c -O0 -g -fno-pic -no-pie -o $@ $(CXXFLAGS) -I../vendor/koshle -I../vendor/emu -DDC_SIM $<
%.sim.o3: %.cpp
	$(CXX) -c -O3 -g -fno-pic -no-pie -o $@ $(CXXFLAGS) -I../vendor/koshle -I../vendor/emu -DDC_SIM $<

clean:
	-rm -f $(OBJS_SIM) $(TARGET) $(DEPS_SIM)

$(TARGET): $(OBJS_SIM)
	$(CXX) -fno-pic -no-pie -o $(TARGET) $(OBJS_SIM) -lX11

repacker.elf: ../repacker.cpp
	$(CXX) -O3 -g -fno-pic -no-pie -o $@ $(CXXFLAGS) -DDC_REPACK $<

-include $(DEPS_SIM)