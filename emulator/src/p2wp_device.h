#ifndef EMULATOR_P2WP_DEVICE_H
#define EMULATOR_P2WP_DEVICE_H

#include <stddef.h>
#include <stdint.h>

typedef int (*p2wp_fetch_fn)(void *, uint8_t, uint16_t, uint8_t,
                            uint8_t screen[960], uint8_t *, uint8_t[7]);

void p2wp_device_init(p2wp_fetch_fn fetch, void *context);
void p2wp_device_set_protocol_range(uint8_t minimum, uint8_t maximum);
void p2wp_device_set_status_length(uint8_t length);
void p2wp_device_reset(void);
void p2wp_device_out(uint8_t port, uint8_t value);
uint8_t p2wp_device_in(uint8_t port);

#endif
