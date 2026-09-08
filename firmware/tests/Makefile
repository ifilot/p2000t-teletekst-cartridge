CC ?= cc
CFLAGS ?= -std=c11 -Wall -Wextra -Werror -O2

.PHONY: test check-version clean

p2wp-test: test_p2wp.c ../src/firmware_core.c ../src/firmware_core.h ../src/p2wp.c ../src/p2wp.h ../src/teletekst.c ../src/teletekst.h ../src/custom_endpoint.c ../src/custom_endpoint.h ../src/version.c ../src/version.h
	$(CC) $(CFLAGS) -I../src test_p2wp.c ../src/firmware_core.c ../src/p2wp.c ../src/teletekst.c ../src/custom_endpoint.c ../src/version.c -o $@

check-version:
	python3 check_release_version.py

test: check-version p2wp-test
	./p2wp-test

clean:
	rm -f p2wp-test
