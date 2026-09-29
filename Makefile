# Convenience entry points that build and run the complete project.
#
# The emulator needs CMake, Ninja, SDL2 and libcurl; the cartridge needs
# Docker for the pinned Z88DK toolchain.

EMULATOR := emulator/build/p2000t-emulator
MONITOR ?= $(if $(P2000_MONITOR_ROM),$(P2000_MONITOR_ROM),emulator/assets/P2000ROM.bin)
FONT := emulator/assets/Default.fnt
CARTRIDGE := src/p2wp-cartridge.bin

# Emulator options: EMUFLAGS replaces the network mode, ARGS appends extras
# such as --auto-key or --p2wp-version.
EMUFLAGS ?= --live
ARGS ?=

# $(call run_emulator,<cartridge image>)
run_emulator = $(EMULATOR) --monitor $(MONITOR) --cartridge $(1) --font $(FONT) \
	$(EMUFLAGS) $(ARGS)

.PHONY: all run emulator check-emulator-deps rom test clean

all: emulator rom

# Build the emulator and cartridge, then boot the cartridge.
run: emulator rom
	$(call run_emulator,$(CARTRIDGE))

emulator: check-emulator-deps
	$(MAKE) -C emulator

# CMake reports a missing SDL2 or libcurl as a pkg-config failure; name the
# packages instead so a fresh checkout knows what to install.
check-emulator-deps:
	@command -v pkg-config >/dev/null || exit 0; \
	pkg-config --exists sdl2 libcurl || { \
		echo "emulator: SDL2 and libcurl development files are required; install them with:" >&2; \
		echo "  sudo apt install cmake ninja-build libsdl2-dev libcurl4-openssl-dev" >&2; \
		exit 1; }

rom:
	$(MAKE) -C src

test: emulator rom
	$(MAKE) -C emulator test

clean:
	$(MAKE) -C emulator clean
	$(MAKE) -C src clean
