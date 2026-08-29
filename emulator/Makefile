.PHONY: all test clean
BUILD := build

all:
	cmake -S . -B $(BUILD) -G Ninja
	cmake --build $(BUILD)

test:
	python3 tests/test_integration.py

clean:
	cmake -E remove_directory $(BUILD)
