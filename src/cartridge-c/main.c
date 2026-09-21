#include "p2wp.h"
#include "platform.h"
#include "teletekst.h"
#include "ui.h"
#include "wifi.h"

int main(void) {
  p2wp_session_t session;
  enum p2wp_result result;

  ui_opening_screen();

  (void)platform_read_key();
  wifi_show_scanning();
  result = p2wp_hello(&session);
  if (result == P2WP_OK) {
    if (wifi_startup(&session))
      (void)teletekst_start(&session);
  } else if (result == P2WP_INCOMPATIBLE) {
    platform_write_text(10, 0, "GEEN GEDEELDE P2WP-VERSIE");
  } else if (result == P2WP_TIMEOUT) {
    platform_write_text(10, 0, "PICO W NIET GEVONDEN");
  } else {
    platform_write_text(10, 0, "ONGELDIG P2WP-ANTWOORD");
  }
  platform_halt();
  return 0;
}
