TARGET ?= tlj.elf
TARGET_SIM ?= tlj-sim.elf

PROJECT_NAME = tlj
TEAM_NAME ="the gang"
DISC_SERIAL = HB-TLJD
RELEASE_DATE = 20250401
MKDCDISC_PAD_OPTION=-N

all: $(TARGET_SIM) $(TARGET) repacker.elf

DATA_DIR=repack-data/tlj

# include common.mk
include reactphysics3d.mk
OBJS = \
	../main.o \
	../audio_driver.o \
	../pavo/pavo.o \
	../vendor/gldc/alloc.o \
	../vendor/dca3/thread.o \
	../first_level/hierarchy.o \
	../first_level/components.o \
	../first_level/animations.o \
	../first_level/cameras.o \
	../first_level/audio_sources.o \
	../first_level/audio_clips.o \
	../first_level/scripts.o \
	../first_level/physics.o \
	../first_level/fonts.o \
	../first_level/flowmachines.o \
	\
	$(RP3D_OBJECTS)
	

DEPS_OBJS=$(OBJS:.o=.d) $(OBJS_NATIVE:.o=.d`)

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

CLAGS += -MMD -MP 

CXXFLAGS += \
	-MMD -MP -ffunction-sections -fdata-sections -ffast-math \
	-fmerge-all-constants -fomit-frame-pointer  -std=gnu++20 \
	-fno-exceptions -fno-rtti -fno-PIC -Wno-write-strings \
	-Wno-deprecated-enum-enum-conversion -Wno-deprecated-enum-float-conversion \
	-Wno-multichar -Wno-unused-value -Wno-char-subscripts -Wno-reorder \
	-Wno-unused-function -Wno-class-memaccess -fno-permissive \
	-fno-asynchronous-unwind-tables -fno-non-call-exceptions \
	-fno-strict-aliasing -fwrapv -Wno-unknown-pragmas \
	-fcoroutines

KOS_CPPFLAGS += -fbuiltin -ffast-math -ffp-contract=fast \
	-mfsrra -mfsca -ml -fipa-pta -fno-enforce-eh-specs

ifdef KOS_BASE
include $(KOS_BASE)/Makefile.rules
else
$(warning "KOS_BASE is not set. Only prebuilt targets will work.")
endif

%.o: %.cpp
	kos-c++ $(CXXFLAGS) $(CPPFLAGS) $(RP3D_INCLUDE_DIRS) -I../ -DDC_SH4=1 -c $< -o $@

$(TARGET): $(OBJS)
	kos-c++ -o $(TARGET) $(OBJS) -Wl,--gc-sections -Wl,--as-needed -Wl,-Map,output.map
	@echo && echo && echo "*** Build Completed Successfully ($(TARGET)) ***" && echo && echo

%.sim.o: %.c
	$(CC) -c -O0 -g -fno-pic -no-pie -o $@ $(CFLAGS) -I../vendor/koshle -I../vendor/emu -I../ $(RP3D_INCLUDE_DIRS) -DDC_SIM $<
%.sim.o: %.cpp
	$(CXX) -c -O0 -g -fno-pic -no-pie -o $@ $(CXXFLAGS) -I../vendor/koshle -I../vendor/emu -I../ $(RP3D_INCLUDE_DIRS) -DDC_SIM $<
%.sim.o3: %.cpp
	$(CXX) -c -O3 -g -fno-pic -no-pie -o $@ $(CXXFLAGS) -I../vendor/koshle -I../vendor/emu -I../ $(RP3D_INCLUDE_DIRS) -DDC_SIM $<


pvrtex:
	$(MAKE) -C ../vendor/pvrtex

$(TARGET_SIM): $(OBJS_SIM)
	$(CXX) -fno-pic -no-pie -o $(TARGET_SIM) $(OBJS_SIM) -lX11


%.repacker.o: %.c
	$(CC) -c -O3 -g -fno-pic -no-pie -o $@ $(CFLAGS) -I../vendor/koshle -I../vendor/emu -I../ -I../vendor/TriStripper/include -DDC_REPACKER $<
%.repacker.o: %.cpp
	$(CXX) -c -O3 -g -fno-pic -no-pie -o $@ $(CXXFLAGS) -I../vendor/koshle -I../vendor/emu -I../ -I../vendor/TriStripper/include -DDC_REPACKER $<


OBJS_REPACKER= \
	../tools/repacker.repacker.o \
	\
	../vendor/dca3/vq.repacker.o \
	\
	../vendor/TriStripper/src/connectivity_graph.repacker.o \
	../vendor/TriStripper/src/policy.repacker.o \
	../vendor/TriStripper/src/tri_stripper.repacker.o \
	\
	../vendor/hmm/base.repacker.o \
	../vendor/hmm/blur.repacker.o \
	../vendor/hmm/heightmap.repacker.o \
	../vendor/hmm/triangulator.repacker.o

DEPS_REPACKER=$(OBJS_REPACKER:.o=.d)

repacker.elf: $(OBJS_REPACKER) | pvrtex
	$(CXX) -g -fno-pic -no-pie -o $@ $(CXXFLAGS) -DDC_REPACKER $(OBJS_REPACKER) -lmeshoptimizer

aud2adpcm: ../vendor/dca3/aud2adpcm.c
	$(CC) -o $@ -O3 -g $< -I../vendor/minimp3

$(PROJECT_NAME).cdi: $(TARGET)
	mkdcdisc -e $(TARGET) -o $(PROJECT_NAME).cdi -d $(DATA_DIR)/ $(MKDCDISC_PAD_OPTION) -n $(PROJECT_NAME) -a $(TEAM_NAME) -s $(DISC_SERIAL) -r $(RELEASE_DATE)

sim: $(TARGET_SIM)
	@echo && echo && echo "*** Build Completed Successfully ($(TARGET_SIM)) ***" && echo && echo

cdi: $(PROJECT_NAME).cdi
	@echo && echo && echo "*** Build Completed Successfully ($(PROJECT_NAME).cdi) ***" && echo && echo

repack-data/fonts.repacked: $(shell ls fonts/font_*.png) | pvrtex
	@mkdir -p repack-data/tlj
	@mkdir -p repack-data/fonts
	@mkdir -p repack-data/tlj/fonts
	@for i in $(shell ls fonts/font_*.png); do \
		cp $${i} repack-data/fonts/; \
		mogrify -resize '256x256>' repack-data/fonts/$$(basename $${i}); \
		convert repack-data/fonts/$$(basename $${i}) -channel RGB -negate +channel repack-data/fonts/$$(basename $${i}); \
		../vendor/pvrtex/pvrtex -i repack-data/fonts/$$(basename $${i}) -o repack-data/tlj/fonts/$$(basename $${i%.png}.pvr) -c small -d; \
	done
	@echo && echo && echo "*** Repacked Fonts ***" && echo && echo
	@touch $@

repack-data/audio.repacked: $(shell ls audio/*.wav) aud2adpcm
	@mkdir -p repack-data/tlj
	@mkdir -p repack-data/tlj/audio
	@for i in $(shell ls audio/*.wav); do \
		./aud2adpcm -rawm audio/$$(basename $${i}) repack-data/tlj/audio/$$(basename $${i%.wav}.raw); \
	done
	@echo && echo && echo "*** Repacked Audio ***" && echo && echo
	@touch $@
.PHONY: pvrtex cdi sim


clean:
	-rm -f $(OBJS) $(DEPS_OBJS) $(OBJS_SIM) $(DEPS_SIM) $(OBJS_REPACKER) $(DEPS_REPACKER) $(TARGET)

-include $(DEPS_OBJS)
-include $(DEPS_SIM)
-include $(DEPS_REPACKER)