#include "p2wp.h"
#include "platform.h"
#include "teletekst.h"
#include "ui.h"
#include "wifi.h"

static uint8_t payload_equal(const p2wp_response_t *reply,
                             const uint8_t *expected, uint16_t length) {
  uint16_t index;

  if (reply->payload_length != length) {
    return 0u;
  }
  for (index = 0u; index != length; ++index) {
    if (reply->payload[index] != expected[index]) {
      return 0u;
    }
  }
  return 1u;
}

static void show_device_info(p2wp_session_t *session) {
  p2wp_response_t reply;
  enum p2wp_result result;

  if ((session->capabilities & P2WP_CAPABILITY_DEVICE_INFO) == 0u) {
    platform_write_text(16, 0, "APPARAATINFO NIET BESCHIKBAAR");
    return;
  }
  result = p2wp_request(session, P2WP_TYPE_DEVICE_INFO, 0, 0u, &reply);
  if (result != P2WP_OK || reply.payload_length != 4u ||
      reply.payload[0] < 1u || reply.payload[0] > 2u) {
    platform_write_text(16, 0, "APPARAATINFO MISLUKT");
    return;
  }
  platform_write_text(
      16, 0, reply.payload[0] == 1u ? "PICO W      FW" : "PICO 2 W    FW");
  platform_write_u8(16, 15, reply.payload[1]);
  platform_write_text(16, 16, ".");
  platform_write_u8(16, 17, reply.payload[2]);
  platform_write_text(16, 18, ".");
  platform_write_u8(16, 19, reply.payload[3]);
}

static void run_echo_test(p2wp_session_t *session) {
  static const uint8_t test_payload[] = {
      'P', '2', 'W', 'P', 0x7d, 0x7e, 0x00, 0xff,
  };
  p2wp_response_t reply;
  enum p2wp_result result;
  uint8_t display_length = 0u;

  reply.payload_length = 0u;
  result = p2wp_request(session, P2WP_TYPE_ECHO, test_payload,
                        sizeof(test_payload), &reply);

  if (result == P2WP_OK &&
      payload_equal(&reply, test_payload, sizeof(test_payload))) {
    platform_write_text(18, 0, "ECHO/ESCAPE TEST OK");
  } else {
    platform_write_text(18, 0, "ECHO/ESCAPE TEST MISLUKT");
    platform_write_text(20, 0, "RESULTAAT/LENGTE:");
    platform_write_u8(20, 18, (uint8_t)result);
    platform_write_text(20, 21, "/");
    if (result == P2WP_OK) {
      display_length = (uint8_t)reply.payload_length;
    }
    platform_write_u8(20, 23, display_length);
  }
}

int main(void) {
  p2wp_session_t session;
  enum p2wp_result result;

  ui_opening_screen();

  (void)platform_read_key();
  platform_clear_screen();
  ui_title(0u, "P2000T TELETEKST          PICO TEST");
  ui_panel(4u, " PICO VERBINDING TESTEN...");
  result = p2wp_hello(&session);
  if (result == P2WP_OK) {
    platform_write_text(10, 0, "P2WP/   VERBONDEN");
    platform_write_u8(10, 5, session.version);
    platform_write_text(12, 0, "ONTVANGSTLIMIET:");
    platform_write_u8(12, 18, (uint8_t)session.receive_limit);
    platform_write_text(14, 0, "CAPACITEITEN:");
    platform_write_u8(14, 14, session.capabilities);
    show_device_info(&session);
    run_echo_test(&session);
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
