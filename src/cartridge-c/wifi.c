#include "wifi.h"
#include "platform.h"
#include "ui.h"

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

static uint8_t read_event(void) {
  uint8_t raw = platform_read_key();
  if (raw == P2000T_KEY_STOP || raw == 0x58u)
    return P2000T_KEY_STOP;
  return platform_translate_key(raw);
}

static uint8_t display_byte(uint8_t value) {
  return value == '#' ? 0x5fu : value;
}

static uint8_t request_ok(p2wp_session_t *s, uint8_t type, const uint8_t *p,
                          uint16_t n, p2wp_response_t *r) {
  return p2wp_request(s, type, p, n, r) == P2WP_OK;
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
    platform_wait_ticks(5);
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
    platform_wait_ticks(2);
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

void wifi_show_scanning(void) {
  platform_clear_screen();
  ui_title(1u, " P2000T  WIFI-INSTELLING");
  ui_action(2u, "   WIFI-NETWERKEN ZOEKEN...");
  ui_panel(3u, " LINK ACTIEF - POLL 0000 |");
  ui_panel(4u, " NETWERKEN GEVONDEN: 0");
  ui_action(5u, "   RADIO: STARTEN");
  ui_rule(6u);
}

static uint8_t scan_networks(p2wp_session_t *s) {
  p2wp_response_t r;
  uint8_t tries = 100, count, index;
  if (!request_ok(s, SCAN_START, 0, 0, &r) || r.payload_length != 0u)
    return 0u;
  while (tries-- != 0u) {
    platform_wait_ticks(5);
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
  ui_title(1u, " P2000T  WIFI-INSTELLING");
  ui_action(2u, "N  SIGNAAL B  NETWERK");
  for (index = 0u; index != MAX_NETWORKS; ++index)
    ui_panel((uint8_t)(3u + index), "");
  ui_rule(12u);
  for (index = 0; index != count; ++index) {
    uint8_t line[P2000T_SCREEN_COLUMNS];
    uint8_t bars;
    uint8_t column;
    uint8_t ssid_length;
    if (!request_ok(s, SCAN_RESULT, &index, 1u, &r) || r.payload_length < 5u ||
        r.payload[0] != index || r.payload[2] > 2u || r.payload[3] == 0u ||
        r.payload[3] > 32u || r.payload_length != (uint16_t)(4u + r.payload[3]))
      return 0u;
    security[index] = r.payload[2];
    for (column = 0u; column != sizeof(line); ++column)
      line[column] = ' ';
    line[0] = 0x04u;
    line[1] = 0x1du;
    line[2] = 0x07u;
    line[3] = (uint8_t)('1' + index);
    line[4] = 0x1du;
    line[5] = 0x04u;
    bars = 1u;
    if (r.payload[1] >= 0xb5u)
      ++bars;
    if (r.payload[1] >= 0xbdu)
      ++bars;
    if (r.payload[1] >= 0xc9u)
      ++bars;
    for (column = 0u; column != 4u; ++column)
      line[7u + column] = column < bars ? 0x5fu : '.';
    line[11] = security[index] == 0u ? ' ' : security[index] == 1u ? '*' : '!';
    ssid_length = r.payload[3] > 27u ? 27u : r.payload[3];
    for (column = 0u; column != ssid_length; ++column)
      line[13u + column] = display_byte(r.payload[4u + column]);
    platform_write_bytes((uint8_t)(3u + index), 0u, line, sizeof(line));
  }
  return count;
}
static uint8_t choose_network(uint8_t count) {
  uint8_t key;
  ui_action(13u, " KIES NETWERK (1-9): ");
  for (;;) {
    key = read_event();
    if (key == P2000T_KEY_STOP)
      return 0xffu;
    if (key >= '1' && key < (uint8_t)('1' + count))
      return (uint8_t)(key - '1');
  }
}
static uint8_t read_password(void) {
  for (;;) {
    uint8_t visible;
    uint8_t key;
    wipe_password();
    platform_clear_line(14u);
    platform_clear_line(15u);
    platform_clear_line(16u);
    platform_clear_line(17u);
    ui_action(14u, " WACHTWOORD TONEN BIJ INVOER? (J/N)");
    ui_action(15u, "");
    for (;;) {
      key = read_event();
      if (key == P2000T_KEY_STOP)
        return 0u;
      key = platform_lower_ascii(key);
      if (key == 'j') {
        visible = 1u;
        break;
      }
      if (key == 'n') {
        visible = 0u;
        break;
      }
    }

    ui_panel(14u, "WACHTWOORD>");
    ui_panel(15u, "");
    for (;;) {
      uint8_t row;
      uint8_t column;
      uint8_t shown;
      key = read_event();
      if (key == P2000T_KEY_STOP) {
        wipe_password();
        return 0u;
      }
      if (key == 13u) {
        if (password_length >= 8u)
          return 1u;
        wipe_password();
        ui_action(16u, " WACHTWOORD: MINSTENS 8 TEKENS");
        break;
      }
      if (key == 8u) {
        if (password_length == 0u)
          continue;
        --password_length;
        row = password_length < 26u ? 14u : 15u;
        column = password_length < 26u ? (uint8_t)(14u + password_length)
                                       : (uint8_t)(3u + password_length - 26u);
        platform_write_text(row, column, " ");
        continue;
      }
      if (key < 32u || key >= 127u || password_length == MAX_PASSWORD)
        continue;
      password[password_length] = key;
      row = password_length < 26u ? 14u : 15u;
      column = password_length < 26u ? (uint8_t)(14u + password_length)
                                     : (uint8_t)(3u + password_length - 26u);
      shown = visible ? display_byte(key) : '*';
      platform_write_bytes(row, column, &shown, 1u);
      ++password_length;
    }
  }
}
static uint8_t connect_selected(p2wp_session_t *s, uint8_t index) {
  uint8_t payload[2 + MAX_PASSWORD], n;
  p2wp_response_t r;
  payload[0] = index;
  payload[1] = password_length;
  for (n = 0; n != password_length; ++n)
    payload[2u + n] = password[n];
  platform_clear_line(14u);
  platform_clear_line(15u);
  ui_action(16u, "VERBINDEN - POGING 1/3...");
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
  do {
    key = platform_read_ascii();
    key = platform_lower_ascii(key);
  } while (key != 'j' && key != 'n');
  if (key == 'n')
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
static uint8_t manual_startup(p2wp_session_t *session) {
  uint8_t count, index;
  count = scan_networks(session);
  if (count == 0u) {
    platform_write_text(15, 0, "GEEN WIFI-NETWERKEN");
    return 0u;
  }
  index = choose_network(count);
  if (index == 0xffu)
    return 0u;
  if (security[index] == 2u) {
    platform_write_text(17, 0, "BEVEILIGING NIET ONDERSTEUND");
    return 0u;
  }
  if (security[index] == 1u && !read_password()) {
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

uint8_t wifi_startup(p2wp_session_t *session) {
  if (try_profile(session)) {
    platform_clear_screen();
    platform_write_text(10, 0, "WIFI VERBONDEN VIA BEWAARD PROFIEL");
    return 1u;
  }
  return manual_startup(session);
}

uint8_t wifi_reconfigure(p2wp_session_t *session) {
  wipe_password();
  wifi_show_scanning();
  return manual_startup(session);
}
