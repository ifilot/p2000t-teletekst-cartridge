.PHONY: all test clean
BUILD := build

all:
	cmake -S . -B $(BUILD) -G Ninja
	cmake --build $(BUILD)

test:
	python3 -m pytest --verbose --tb=short tests

clean:
	cmake -E remove_directory $(BUILD)
