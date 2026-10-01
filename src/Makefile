# Build, document, sign, and smoke-test the sole C-based P2000T cartridge ROM.
# Application logic is C; the listed assembly files are narrow runtime,
# hardware-ABI, and LZ4 support modules rather than an alternate application.
# SPDX-License-Identifier: GPL-3.0-only

ROM := ../build/p2wp-cartridge.bin
SIGNER := sign_cartridge.py
Z88DK_IMAGE := z88dk/z88dk:2.4@sha256:7da21d721a76bf25db518f2415b859b2dfe215ccd059f117cb6ec052251ac2ae
Z88DK_DOCKER := docker run --rm \
	--user $(shell id -u):$(shell id -g) \
	-v $(abspath ..):/src -w /src/src $(Z88DK_IMAGE)
BUILD := ../build/cartridge
C_SOURCES := main.c p2wp.c platform.c ui.c teletekst.c wifi.c \
	platform.asm lz4_z80.asm crt0.asm
C_HEADERS := lz4_z80.h p2wp.h platform.h teletekst.h ui.h ui_screens.h version.h wifi.h
PYTHON_SOURCES := check_doxygen_style.py make_cartridge_rom.py sign_cartridge.py build_info.py
ASM_SOURCES := crt0.asm lz4_z80.asm platform.asm

.PHONY: all lint smoke sign verify check-version clean

all: $(ROM)

lint:
	clang-format --dry-run --Werror *.c *.h
	python3 check_doxygen_style.py *.c *.h \
		$(ASM_SOURCES) $(PYTHON_SOURCES)
	cd .. && doxygen docs/Doxyfile.cartridge

$(ROM): $(C_SOURCES) $(C_HEADERS) make_cartridge_rom.py $(SIGNER) build_info.py Makefile
	mkdir -p $(BUILD)
	python3 build_info.py > $(BUILD)/build_info.h
	$(Z88DK_DOCKER) /opt/z88dk/bin/zcc +embedded \
		-compiler=sdcc -clib=default --opt-code-size \
		-I$(BUILD) \
		--max-allocs-per-node10000 -SO3 \
		-crt0=crt0.asm \
		-m \
		-o $(BUILD)/p2wp-cartridge \
		main.c p2wp.c wifi.c ui.c teletekst.c \
		platform.c platform.asm lz4_z80.asm
	python3 make_cartridge_rom.py \
		--data $(BUILD)/p2wp-cartridge_DATA.bin \
		$(BUILD)/p2wp-cartridge $(ROM)
	python3 $(SIGNER) $(ROM)
	python3 $(SIGNER) --verify $(ROM)

smoke: $(ROM)
	python3 -m pytest -q ../emulator/tests/test_cartridge.py

sign: $(ROM)
	python3 $(SIGNER) $(ROM)

check-version:
	python3 ../firmware/tests/check_release_version.py

verify: check-version $(ROM)
	python3 $(SIGNER) --verify $(ROM)

clean:
	rm -f $(ROM)
	rm -rf $(BUILD)
