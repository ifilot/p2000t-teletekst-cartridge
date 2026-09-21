#ifndef P2000T_WIFI_H
#define P2000T_WIFI_H
#include "p2wp.h"
void wifi_show_scanning(void);
uint8_t wifi_startup(p2wp_session_t *session);
uint8_t wifi_reconfigure(p2wp_session_t *session);
#endif
