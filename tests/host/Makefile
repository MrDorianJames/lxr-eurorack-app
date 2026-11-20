# Host-side (PC) tests for firmware logic that doesn't need the hardware.
# Run: make -C tests/host
CORE   := ../../Core
FATFS  := ../../Middlewares/Third_Party/FatFs/src
GEN    := gen

# Ordinary warnings are left at the compiler default; the checks GCC 14+
# treats as hard errors are forced on, so older compilers catch them too.
STRICT := -Werror=implicit-function-declaration -Werror=implicit-int \
          -Werror=int-conversion -Werror=incompatible-pointer-types
CFLAGS := $(STRICT) -I$(GEN) -I$(CORE)/Inc -I$(FATFS) -Istub -include stdint.h

all: test_voice test_store test_load test_menu test_cfg test_mix test_undo

test_voice: test_voice.c $(CORE)/Src/Preset/voicePreset.c $(CORE)/Src/menu/Cc2Text.c
	$(CC) $(CFLAGS) $^ -o $@
	./$@

# The module's own FatFs configuration, minus its hardware includes
$(GEN)/ffconf.h: ../../FATFS/Target/ffconf.h
	mkdir -p $(GEN)
	sed '/#include "main.h"/d; /#include "stm32f7xx_hal.h"/d; /#include "bsp_driver_sd.h"/d' $< > $@

test_store: test_store.c $(CORE)/Src/Preset/voiceStore.c $(FATFS)/ff.c stub/ramdisk.c $(GEN)/ffconf.h
	$(CC) $(CFLAGS) $(filter %.c,$^) -o $@
	./$@

test_load: test_load.c $(CORE)/Src/Preset/voiceLoad.c $(CORE)/Src/Preset/voicePreset.c $(CORE)/Src/menu/Cc2Text.c
	$(CC) $(CFLAGS) $^ -o $@
	./$@

test_menu: test_menu.c $(CORE)/Src/menu/lsMenu.c $(CORE)/Src/Preset/undo.c $(CORE)/Src/Preset/voiceLoad.c $(CORE)/Src/Preset/voiceStore.c $(CORE)/Src/Preset/voicePreset.c $(CORE)/Src/menu/Cc2Text.c $(FATFS)/ff.c stub/ramdisk.c $(GEN)/ffconf.h
	$(CC) $(CFLAGS) $(filter %.c,$^) -o $@
	./$@

test_cfg: test_cfg.c $(CORE)/Src/menu/cfgMenu.c $(CORE)/Src/globalMix.c
	$(CC) $(CFLAGS) $^ -o $@
	./$@

test_mix: test_mix.c $(CORE)/Src/menu/mixMenu.c $(CORE)/Src/globalMix.c
	$(CC) $(CFLAGS) $^ -o $@
	./$@

test_undo: test_undo.c $(CORE)/Src/Preset/undo.c $(CORE)/Src/Preset/voicePreset.c $(CORE)/Src/menu/Cc2Text.c
	$(CC) $(CFLAGS) $^ -o $@
	./$@

clean:
	rm -rf test_voice test_store test_load test_menu test_cfg test_mix test_undo $(GEN)
.PHONY: all clean
