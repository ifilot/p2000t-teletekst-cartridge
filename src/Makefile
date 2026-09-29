# Build, document, sign, and smoke-test the sole C-based P2000T cartridge ROM.
# Application logic is C; the listed assembly files are narrow runtime,
# hardware-ABI, and LZ4 support modules rather than an alternate application.
# SPDX-License-Identifier: GPL-3.0-only

ROM := p2wp-cartridge.bin
SIGNER := sign_cartridge.py
Z88DK_IMAGE := z88dk/z88dk:2.4@sha256:7da21d721a76bf25db518f2415b859b2dfe215ccd059f117cb6ec052251ac2ae
Z88DK_DOCKER := docker run --rm \
	--user $(shell id -u):$(shell id -g) \
	-v $(abspath ..):/src -w /src/src $(Z88DK_IMAGE)
C_BUILD := build-c
C_SOURCES := cartridge-c/main.c cartridge-c/p2wp.c cartridge-c/platform.c cartridge-c/ui.c \
	cartridge-c/teletekst.c cartridge-c/wifi.c \
	cartridge-c/platform.asm cartridge-c/lz4_z80.asm cartridge-c/crt0.asm
C_HEADERS := cartridge-c/lz4_z80.h cartridge-c/p2wp.h cartridge-c/platform.h \
	cartridge-c/teletekst.h cartridge-c/ui.h cartridge-c/version.h \
	cartridge-c/wifi.h
PYTHON_SOURCES := check_doxygen_style.py make_cartridge_rom.py sign_cartridge.py
ASM_SOURCES := cartridge-c/crt0.asm cartridge-c/lz4_z80.asm cartridge-c/platform.asm

.PHONY: all lint smoke sign verify check-version clean

all: $(ROM)

lint:
	clang-format --dry-run --Werror cartridge-c/*.c cartridge-c/*.h
	python3 check_doxygen_style.py cartridge-c/*.c cartridge-c/*.h \
		$(ASM_SOURCES) $(PYTHON_SOURCES)
	cd .. && doxygen docs/Doxyfile.cartridge

$(ROM): $(C_SOURCES) $(C_HEADERS) make_cartridge_rom.py $(SIGNER)
	mkdir -p $(C_BUILD)
	$(Z88DK_DOCKER) /opt/z88dk/bin/zcc +embedded \
		-compiler=sdcc -clib=default --opt-code-size \
		--max-allocs-per-node10000 -SO3 \
		-crt0=cartridge-c/crt0.asm \
		-m -s --list \
		-o $(C_BUILD)/p2wp-cartridge \
		cartridge-c/main.c cartridge-c/p2wp.c cartridge-c/wifi.c cartridge-c/ui.c \
		cartridge-c/teletekst.c \
		cartridge-c/platform.c cartridge-c/platform.asm cartridge-c/lz4_z80.asm
	python3 make_cartridge_rom.py \
		--data $(C_BUILD)/p2wp-cartridge_DATA.bin \
		$(C_BUILD)/p2wp-cartridge $(ROM)
	python3 $(SIGNER) $(ROM)
	python3 $(SIGNER) --verify $(ROM)

smoke: $(ROM)
	python3 ../emulator/tests/test_cartridge.py

sign: $(ROM)
	python3 $(SIGNER) $(ROM)

check-version:
	python3 ../firmware/tests/check_release_version.py

verify: check-version $(ROM)
	python3 $(SIGNER) --verify $(ROM)

clean:
	rm -f $(ROM)
	rm -rf $(C_BUILD)
