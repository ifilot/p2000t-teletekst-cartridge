CC ?= cc
CFLAGS ?= -std=c11 -Wall -Wextra -Werror -O2

.PHONY: test clean

p2wp-test: test_p2wp.c ../src/p2wp.c ../src/p2wp.h ../src/teletekst.c ../src/teletekst.h ../src/custom_endpoint.c ../src/custom_endpoint.h
	$(CC) $(CFLAGS) -I../src test_p2wp.c ../src/p2wp.c ../src/teletekst.c ../src/custom_endpoint.c -o $@

test: p2wp-test
	./p2wp-test

clean:
	rm -f p2wp-test
