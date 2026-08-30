Z80ASM ?= z80asm
ROM := p2wp-cartridge.bin
SOURCE := p2wp-cartridge.asm
SIGNER := sign_cartridge.py

.PHONY: all sign verify check-version clean

all: $(ROM)

$(ROM): $(SOURCE) $(SIGNER)
	$(Z80ASM) -l -L -o $@ $<
	test "$$(stat -c '%s' $@)" -eq 16384
	python3 $(SIGNER) $@

sign: $(ROM)
	python3 $(SIGNER) $(ROM)

check-version:
	python3 ../firmware/tests/check_release_version.py

verify: check-version $(ROM)
	python3 $(SIGNER) --verify $(ROM)

clean:
	rm -f $(ROM) p2wp-cartridge.lst p2wp-cartridge.lab
