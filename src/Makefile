Z80ASM ?= z80asm
ROM := p2wp-cartridge.bin
SOURCE := p2wp-cartridge.asm
SIGNER := sign_cartridge.py
Z88DK_IMAGE := z88dk/z88dk:2.4@sha256:7da21d721a76bf25db518f2415b859b2dfe215ccd059f117cb6ec052251ac2ae
Z88DK_DOCKER := docker run --rm \
	--user $(shell id -u):$(shell id -g) \
	-v $(abspath ..):/src -w /src/src $(Z88DK_IMAGE)
C_ROM := p2wp-cartridge-c.bin
C_BUILD := build-c
C_SOURCES := cartridge-c/main.c cartridge-c/p2wp.c cartridge-c/platform.c cartridge-c/ui.c \
	cartridge-c/teletekst.c cartridge-c/wifi.c \
	cartridge-c/platform.asm cartridge-c/crt0.asm

.PHONY: all c-rom c-smoke sign verify check-version clean

all: $(ROM)

$(ROM): $(SOURCE) $(SIGNER)
	$(Z80ASM) -l -L -o $@ $<
	test "$$(stat -c '%s' $@)" -eq 16384
	python3 $(SIGNER) $@

c-rom: $(C_ROM)

$(C_ROM): $(C_SOURCES) make_cartridge_rom.py $(SIGNER)
	mkdir -p $(C_BUILD)
	$(Z88DK_DOCKER) /opt/z88dk/bin/zcc +embedded \
		-compiler=sccz80 -clib=default -O2 \
		-crt0=cartridge-c/crt0.asm \
		-m -s --list \
		-o $(C_BUILD)/p2wp-cartridge-c \
		cartridge-c/main.c cartridge-c/p2wp.c cartridge-c/wifi.c cartridge-c/ui.c \
		cartridge-c/teletekst.c \
		cartridge-c/platform.c cartridge-c/platform.asm
	python3 make_cartridge_rom.py \
		--data $(C_BUILD)/p2wp-cartridge-c_DATA.bin \
		$(C_BUILD)/p2wp-cartridge-c $(C_ROM)
	python3 $(SIGNER) $(C_ROM)
	python3 $(SIGNER) --verify $(C_ROM)

c-smoke: $(C_ROM)
	python3 ../emulator/tests/test_c_cartridge_smoke.py

sign: $(ROM)
	python3 $(SIGNER) $(ROM)

check-version:
	python3 ../firmware/tests/check_release_version.py

verify: check-version $(ROM)
	python3 $(SIGNER) --verify $(ROM)

clean:
	rm -f $(ROM) $(C_ROM) p2wp-cartridge.lst p2wp-cartridge.lab
	rm -rf $(C_BUILD)
