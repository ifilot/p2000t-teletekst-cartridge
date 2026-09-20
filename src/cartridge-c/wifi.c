#include "wifi.h"
#include "platform.h"

enum {
  SCAN_START = 0x10,
  SCAN_STATUS = 0x11,
  SCAN_RESULT = 0x12,
  WIFI_CONNECT = 0x13,
  WIFI_STATUS = 0x14,
  PROFILE_STATUS = 0x20,
  PROFILE_CONNECT = 0x21,
  PROFILE_SAVE = 0x22,
  MAX_NETWORKS = 9,
  MAX_PASSWORD = 63
};
static uint8_t security[MAX_NETWORKS], password[MAX_PASSWORD], password_length;

static uint8_t request_ok(p2wp_session_t *s, uint8_t type, const uint8_t *p,
                          uint16_t n, p2wp_response_t *r) {
  return p2wp_request(s, type, p, n, r) == P2WP_OK;
}
static void wait_ticks(uint8_t ticks) {
  uint16_t end = platform_clock() + ticks;
  while ((int16_t)(platform_clock() - end) < 0) {
  }
}
static void wipe_password(void) {
  volatile uint8_t *p = password;
  uint8_t n = sizeof(password);
  while (n-- != 0u)
    *p++ = 0u;
  password_length = 0u;
}
static uint8_t poll_connection(p2wp_session_t *s) {
  p2wp_response_t r;
  uint16_t tries = 350u;
  while (tries-- != 0u) {
    wait_ticks(5);
    if (!request_ok(s, WIFI_STATUS, 0, 0, &r) || r.payload_length != 1u)
      return 0u;
    if (r.payload[0] == 2u)
      return 1u;
    if (r.payload[0] != 1u)
      return 0u;
  }
  return 0u;
}
static uint8_t poll_profile(p2wp_session_t *s) {
  p2wp_response_t r;
  uint8_t tries = 100u;
  while (tries-- != 0u) {
    wait_ticks(2);
    if (!request_ok(s, PROFILE_STATUS, 0, 0, &r) || r.payload_length != 2u)
      return 0u;
    if (r.payload[0] != 2u)
      return r.payload[0] == 1u && r.payload[1] == 0u;
  }
  return 0u;
}
static uint8_t try_profile(p2wp_session_t *s) {
  p2wp_response_t r;
  if (!request_ok(s, PROFILE_STATUS, 0, 0, &r) || r.payload_length != 2u ||
      r.payload[0] != 1u)
    return 0u;
  platform_clear_screen();
  platform_write_text(2, 0, "BEWAARD WIFI-PROFIEL VERBINDEN...");
  if (!request_ok(s, PROFILE_CONNECT, 0, 0, &r) || r.payload_length != 0u)
    return 0u;
  return poll_profile(s) && poll_connection(s);
}
static uint8_t scan_networks(p2wp_session_t *s) {
  p2wp_response_t r;
  uint8_t tries = 100, count, index;
  platform_clear_screen();
  platform_write_text(1, 0, "WIFI-NETWERKEN ZOEKEN...");
  if (!request_ok(s, SCAN_START, 0, 0, &r) || r.payload_length != 0u)
    return 0u;
  while (tries-- != 0u) {
    wait_ticks(5);
    if (!request_ok(s, SCAN_STATUS, 0, 0, &r) || r.payload_length != 3u)
      return 0u;
    if (r.payload[0] == 2u)
      break;
    if (r.payload[0] != 1u)
      return 0u;
  }
  if (r.payload[0] != 2u)
    return 0u;
  count = r.payload[1] > MAX_NETWORKS ? MAX_NETWORKS : r.payload[1];
  for (index = 0; index != count; ++index) {
    if (!request_ok(s, SCAN_RESULT, &index, 1u, &r) || r.payload_length < 5u ||
        r.payload[0] != index || r.payload[2] > 2u || r.payload[3] == 0u ||
        r.payload[3] > 32u || r.payload_length != (uint16_t)(4u + r.payload[3]))
      return 0u;
    security[index] = r.payload[2];
    platform_write_u8((uint8_t)(4u + index), 0, (uint8_t)(index + 1u));
    platform_write_text((uint8_t)(4u + index), 2, "-");
    platform_write_bytes((uint8_t)(4u + index), 4, r.payload + 4, r.payload[3]);
  }
  return count;
}
static uint8_t choose_network(uint8_t count) {
  uint8_t key;
  platform_write_text(15, 0, "KIES NETWERK (1-9)");
  for (;;) {
    key = platform_read_ascii();
    if (key >= '1' && key < (uint8_t)('1' + count))
      return (uint8_t)(key - '1');
  }
}
static uint8_t read_password(void) {
  uint8_t key;
  wipe_password();
  platform_clear_line(17);
  platform_clear_line(18);
  platform_write_text(17, 0, "WACHTWOORD (8-63), ENTER:");
  for (;;) {
    key = platform_read_ascii();
    if (key == 13u)
      return password_length >= 8u;
    if (key == 8u) {
      if (password_length != 0u)
        --password_length;
    } else if (key >= 32u && key < 127u && password_length < MAX_PASSWORD)
      password[password_length++] = key;
    platform_clear_line(18);
    if (password_length != 0u)
      platform_write_u8(18, 0, password_length);
    platform_write_text(18, 4, "TEKENS");
  }
}
static uint8_t connect_selected(p2wp_session_t *s, uint8_t index) {
  uint8_t payload[2 + MAX_PASSWORD], n;
  p2wp_response_t r;
  payload[0] = index;
  payload[1] = password_length;
  for (n = 0; n != password_length; ++n)
    payload[2u + n] = password[n];
  platform_clear_screen();
  platform_write_text(8, 0, "VERBINDEN MET WIFI...");
  if (!request_ok(s, WIFI_CONNECT, payload, (uint16_t)(2u + password_length),
                  &r) ||
      r.payload_length != 0u)
    return 0u;
  return poll_connection(s);
}
static void offer_save(p2wp_session_t *s) {
  uint8_t payload[1 + MAX_PASSWORD], n, key;
  p2wp_response_t r;
  platform_clear_screen();
  platform_write_text(6, 0, "WIFI-PROFIEL BEWAREN? J/N");
  do
    key = platform_read_ascii();
  while (key != 'j' && key != 'J' && key != 'n' && key != 'N');
  if (key == 'n' || key == 'N')
    return;
  payload[0] = password_length;
  for (n = 0; n != password_length; ++n)
    payload[1u + n] = password[n];
  platform_write_text(8, 0, "PROFIEL VERSLEUTELEN...");
  if (request_ok(s, PROFILE_SAVE, payload, (uint16_t)(1u + password_length),
                 &r) &&
      r.payload_length == 0u && poll_profile(s))
    platform_write_text(10, 0, "WIFI-PROFIEL BEWAARD");
  else
    platform_write_text(10, 0, "OPSLAAN MISLUKT");
}
uint8_t wifi_startup(p2wp_session_t *session) {
  uint8_t count, index;
  if (try_profile(session)) {
    platform_clear_screen();
    platform_write_text(10, 0, "WIFI VERBONDEN VIA BEWAARD PROFIEL");
    return 1u;
  }
  count = scan_networks(session);
  if (count == 0u) {
    platform_write_text(15, 0, "GEEN WIFI-NETWERKEN");
    return 0u;
  }
  index = choose_network(count);
  if (security[index] == 2u) {
    platform_write_text(17, 0, "BEVEILIGING NIET ONDERSTEUND");
    return 0u;
  }
  if (security[index] == 1u && !read_password()) {
    platform_write_text(20, 0, "WACHTWOORD TE KORT");
    return 0u;
  }
  if (!connect_selected(session, index)) {
    wipe_password();
    platform_write_text(10, 0, "WIFI VERBINDING MISLUKT");
    return 0u;
  }
  offer_save(session);
  wipe_password();
  platform_clear_screen();
  platform_write_text(10, 0, "WIFI VERBONDEN");
  return 1u;
}
