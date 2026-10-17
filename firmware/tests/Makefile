CC ?= cc
CFLAGS ?= -std=c11 -Wall -Wextra -Werror -O2

.PHONY: test http-test check-version clean

p2wp-test: test_p2wp.c ../src/firmware_core.c ../src/firmware_core.h ../src/p2wp.c ../src/p2wp.h ../src/teletekst.c ../src/teletekst.h ../src/custom_endpoint.c ../src/custom_endpoint.h ../src/version.c ../src/version.h
	$(CC) $(CFLAGS) -I../src test_p2wp.c ../src/firmware_core.c ../src/p2wp.c ../src/teletekst.c ../src/custom_endpoint.c ../src/version.c -o $@

check-version:
	python3 check_release_version.py

test: check-version p2wp-test
	./p2wp-test
	python3 -m unittest -q test_release_version

# Uses the same lwIP HTTP client as the firmware; hardware/DNS/TCP are adapters.
# Keep this separate from portable tests because it requires the Pico SDK.
http-test:
	@test -n "$(PICO_SDK_PATH)" || { echo "Set PICO_SDK_PATH to run HTTP recovery tests" >&2; exit 1; }
	$(CC) $(CFLAGS) -Wno-unused-function -Wno-return-type \
		-ffunction-sections -fdata-sections -fsanitize=address,undefined -g \
		-Ihttp/stubs -I$(PICO_SDK_PATH)/lib/lwip/src/include -I../src \
		http/test_http_recovery.c ../src/custom_endpoint.c ../src/teletekst.c ../src/version.c \
		$(PICO_SDK_PATH)/lib/lwip/src/apps/http/http_client.c \
		$(PICO_SDK_PATH)/lib/lwip/src/core/altcp.c \
		$(PICO_SDK_PATH)/lib/lwip/src/core/mem.c \
		$(PICO_SDK_PATH)/lib/lwip/src/core/memp.c \
		$(PICO_SDK_PATH)/lib/lwip/src/core/pbuf.c \
		$(PICO_SDK_PATH)/lib/lwip/src/core/ipv4/ip4_addr.c \
		-Wl,--gc-sections -o http/http-test
	./http/http-test

clean:
	rm -f p2wp-test http/http-test
