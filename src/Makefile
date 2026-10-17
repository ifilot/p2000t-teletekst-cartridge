# Build, document, sign, and smoke-test the sole C-based P2000T cartridge ROM.
# Application logic is C; the listed assembly files are narrow runtime,
# hardware-ABI, and decompression support modules rather than an alternate application.
# SPDX-License-Identifier: GPL-3.0-only

ROM := ../build/p2wp-cartridge.bin
PYTHON ?= python3
SIGNER := sign_cartridge.py
Z88DK_IMAGE := z88dk/z88dk:2.4@sha256:7da21d721a76bf25db518f2415b859b2dfe215ccd059f117cb6ec052251ac2ae
Z88DK_DOCKER := docker run --rm \
	--user $(shell id -u):$(shell id -g) \
	-v $(abspath ..):/src -w /src/src $(Z88DK_IMAGE)
BUILD := ../build/cartridge
ZCC_FLAGS := -compiler=sdcc -clib=default --opt-code-size \
	-I$(BUILD) --max-allocs-per-node10000 -SO3
PACK_ARGS := --data $(BUILD)/p2wp-cartridge_DATA.bin \
	--runtime $(BUILD)/p2wp-cartridge_runtime_code.bin \
	--compressed $(BUILD)/runtime.zx0 \
	--decoded $(BUILD)/runtime.roundtrip.bin \
	--map $(BUILD)/p2wp-cartridge.map \
	--functions $(BUILD)/runtime-functions.json \
	--report $(BUILD)/measurements.json
C_SOURCES := main.c p2wp.c platform.c ui.c teletekst.c wifi.c \
	platform.asm lz4_z80.asm crt0.asm runtime_loader.asm
C_HEADERS := lz4_z80.h p2wp.h platform.h teletekst.h ui.h ui_screens.h version.h wifi.h
PYTHON_SOURCES := check_doxygen_style.py make_cartridge_rom.py sign_cartridge.py \
	build_info.py prepare_runtime.py experiment_split_runtime.py benchmark_split_runtime.py
ASM_SOURCES := crt0.asm lz4_z80.asm platform.asm runtime_loader.asm

.PHONY: all lint smoke sign verify check-version clean

all: $(ROM)

.DELETE_ON_ERROR:

lint:
	clang-format --dry-run --Werror *.c *.h
	$(PYTHON) check_doxygen_style.py *.c *.h \
		$(ASM_SOURCES) $(PYTHON_SOURCES)
	cd .. && doxygen docs/Doxyfile.cartridge

$(ROM): $(C_SOURCES) $(C_HEADERS) make_cartridge_rom.py prepare_runtime.py $(SIGNER) build_info.py Makefile
	mkdir -p $(BUILD)
	$(PYTHON) build_info.py > $(BUILD)/build_info.h
	$(Z88DK_DOCKER) /opt/z88dk/bin/zcc +embedded $(ZCC_FLAGS) \
		-a -o $(BUILD)/teletekst.asm teletekst.c
	$(PYTHON) prepare_runtime.py $(BUILD)/teletekst.asm \
		$(BUILD)/teletekst-runtime.asm --manifest $(BUILD)/runtime-functions.json
	$(Z88DK_DOCKER) /opt/z88dk/bin/zcc +embedded $(ZCC_FLAGS) \
		-crt0=crt0.asm \
		-m \
		-o $(BUILD)/p2wp-cartridge \
		main.c p2wp.c wifi.c ui.c $(BUILD)/teletekst-runtime.asm \
		platform.c platform.asm lz4_z80.asm
	$(Z88DK_DOCKER) /opt/z88dk/bin/z88dk-zx0 -f \
		$(BUILD)/p2wp-cartridge_runtime_code.bin $(BUILD)/runtime.zx0
	$(Z88DK_DOCKER) /opt/z88dk/bin/z88dk-dzx0 -f \
		$(BUILD)/runtime.zx0 $(BUILD)/runtime.roundtrip.bin
	$(PYTHON) make_cartridge_rom.py $(PACK_ARGS) \
		$(BUILD)/p2wp-cartridge $(ROM)
	$(PYTHON) $(SIGNER) $(ROM)
	$(PYTHON) $(SIGNER) --verify $(ROM)

smoke: $(ROM)
	$(PYTHON) -m pytest -q tests ../emulator/tests/test_cartridge.py

sign: $(ROM)
	$(PYTHON) $(SIGNER) $(ROM)

check-version:
	$(PYTHON) ../firmware/tests/check_release_version.py

verify: check-version $(ROM)
	$(Z88DK_DOCKER) /opt/z88dk/bin/z88dk-dzx0 -f \
		$(BUILD)/runtime.zx0 $(BUILD)/runtime.roundtrip.bin
	$(PYTHON) make_cartridge_rom.py $(PACK_ARGS) --verify \
		$(BUILD)/p2wp-cartridge $(ROM)
	$(PYTHON) $(SIGNER) --verify $(ROM)

clean:
	rm -f $(ROM)
	rm -rf $(BUILD)
