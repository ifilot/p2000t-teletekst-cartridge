/**
 * @file teletekst.c
 * @brief Source selection, page retrieval, rendering, and viewer state logic.
 *
 * SPDX-License-Identifier: GPL-3.0-only
 * This file is part of the P2000T Teletekst cartridge and is licensed under
 * version 3 of the GNU General Public License. See the repository LICENSE.
 */

#include "teletekst.h"

#include "lz4_z80.h"
#include "platform.h"
#include "ui.h"
#include "wifi.h"

enum {
  FETCH_CONNECTING = 1,
  FETCH_RECEIVING = 2,
  FETCH_COMPLETE = 3,
  FETCH_FAILED = 4,
  SOURCE_NOS = 0,
  SOURCE_P2000T = 1,
  SOURCE_CUSTOM = 2,
  SOURCE_ARCHIVE = 3,
  MAX_CUSTOM_URL = 96,
  SCREEN_SIZE = 960,
  CHUNK_SIZE = 240,
  CHUNK_COUNT = 4,
  RAW_START_KEY = 128,
  EMULATOR_STOP_KEY = 0x58,
  AUTOSTART_DISABLED = 0xff,
};

/**
 * @brief Mutable navigation, rendering, clock, and error state for the viewer.
 */
typedef struct {
  uint8_t source;             /**< Active wire source identifier. */
  uint16_t page;              /**< Current three-digit page number. */
  uint8_t subpage;            /**< Requested subpage, or zero for default. */
  uint8_t next_subpage;       /**< Provider-advertised following subpage. */
  uint16_t previous_page;     /**< Provider-advertised previous page. */
  uint16_t next_page;         /**< Provider-advertised following page. */
  uint16_t rotation_deadline; /**< Next automatic-navigation monitor tick. */
  uint8_t rotation_paused;    /**< Whether subpage rotation is paused. */
  uint8_t cycle_started;      /**< Whether the current subpage cycle started. */
  uint8_t reveal;             /**< Whether concealed text is shown. */
  uint8_t zoom;               /**< Normal, top-half, or bottom-half view. */
  uint8_t auto_page;          /**< Whether page navigation is automatic. */
  uint8_t error;              /**< Last cartridge or remote fetch error. */
  uint8_t hour;               /**< Provider clock hour. */
  uint8_t minute;             /**< Provider clock minute. */
  uint8_t second;             /**< Provider clock second. */
  uint8_t day;                /**< Provider clock day. */
  uint8_t month;              /**< Provider clock month. */
  uint8_t year;               /**< Provider clock two-digit year. */
  uint8_t weekday;            /**< Provider weekday index. */
  uint8_t clock_valid;        /**< Whether clock time is available. */
  uint8_t clock_has_date; /**< Whether complete date fields are available. */
  uint8_t clock_blink;    /**< Current blinking-colon phase. */
  uint16_t clock_tick;    /**< Deadline for the next clock update. */
  uint8_t http_result;    /**< Pico network-layer HTTP result. */
  uint8_t lwip_error;     /**< Pico lwIP error byte. */
  uint16_t http_status;   /**< HTTP response status. */
  uint8_t error_details;  /**< Whether extended error fields are valid. */
  uint8_t auto_retry;     /**< Whether automatic mode retries this page. */
  uint8_t page_visible;   /**< Whether video RAM currently holds a page. */
} viewer_state_t;

/** Raw 40-by-24 page bytes downloaded from the Pico. */
static uint8_t page_screen[SCREEN_SIZE];
/** Packed 40-by-24 rendered display and compressed-screen workspace. */
static uint8_t display_screen[SCREEN_SIZE];
/** Shared page-fetch and persistent-setting request payload. */
static uint8_t fetch_request[5u + MAX_CUSTOM_URL];
/** Current custom server base URL without a terminator. */
static uint8_t custom_url[MAX_CUSTOM_URL];
/** Number of populated bytes in custom_url. */
static uint8_t custom_url_length;
/** Saved source-menu digit, or AUTOSTART_DISABLED. */
static uint8_t auto_start_source = AUTOSTART_DISABLED;
/** Video bytes temporarily covered by the fetch indicator. */
static uint8_t indicator_saved[4];
/** Current frame index in indicator_frames. */
static uint8_t indicator_phase;
/** RAM workspace receiving decompressed error descriptions. */
static uint8_t error_text[341];
/** Raw LZ4 block containing the null-delimited error descriptions. */
static const uint8_t error_text_lz4[] = {
    0xf1u, 0x5au, 0x47u, 0x45u, 0x45u, 0x4eu, 0x20u, 0x57u, 0x49u, 0x46u, 0x49u,
    0x2du, 0x56u, 0x45u, 0x52u, 0x42u, 0x49u, 0x4eu, 0x44u, 0x49u, 0x4eu, 0x47u,
    0x00u, 0x54u, 0x4cu, 0x53u, 0x2du, 0x43u, 0x4fu, 0x4eu, 0x46u, 0x49u, 0x47u,
    0x55u, 0x52u, 0x41u, 0x54u, 0x49u, 0x45u, 0x20u, 0x4du, 0x49u, 0x53u, 0x4cu,
    0x55u, 0x4bu, 0x54u, 0x00u, 0x41u, 0x41u, 0x4eu, 0x56u, 0x52u, 0x41u, 0x41u,
    0x47u, 0x20u, 0x4bu, 0x4fu, 0x4eu, 0x20u, 0x4eu, 0x49u, 0x45u, 0x54u, 0x20u,
    0x53u, 0x54u, 0x41u, 0x52u, 0x54u, 0x45u, 0x4eu, 0x00u, 0x4fu, 0x4eu, 0x42u,
    0x45u, 0x4bu, 0x45u, 0x4eu, 0x44u, 0x45u, 0x20u, 0x4eu, 0x45u, 0x54u, 0x57u,
    0x45u, 0x52u, 0x4bu, 0x46u, 0x4fu, 0x55u, 0x54u, 0x00u, 0x48u, 0x54u, 0x54u,
    0x50u, 0x2du, 0x53u, 0x45u, 0x52u, 0x56u, 0x45u, 0x52u, 0x10u, 0x00u, 0xf2u,
    0x18u, 0x41u, 0x4eu, 0x54u, 0x57u, 0x4fu, 0x4fu, 0x52u, 0x44u, 0x20u, 0x54u,
    0x45u, 0x20u, 0x47u, 0x52u, 0x4fu, 0x4fu, 0x54u, 0x00u, 0x4fu, 0x4eu, 0x47u,
    0x45u, 0x4cu, 0x44u, 0x49u, 0x47u, 0x45u, 0x20u, 0x50u, 0x41u, 0x47u, 0x49u,
    0x4eu, 0x41u, 0x44u, 0x41u, 0x54u, 0x41u, 0x00u, 0x0bu, 0x00u, 0x02u, 0x61u,
    0x00u, 0xfbu, 0x02u, 0x47u, 0x45u, 0x56u, 0x4fu, 0x4eu, 0x44u, 0x45u, 0x4eu,
    0x00u, 0x44u, 0x4eu, 0x53u, 0x2du, 0x4eu, 0x41u, 0x41u, 0x4du, 0x17u, 0x00u,
    0x06u, 0xb7u, 0x00u, 0x75u, 0x20u, 0x4fu, 0x46u, 0x20u, 0x54u, 0x4cu, 0x53u,
    0xadu, 0x00u, 0x07u, 0x1au, 0x00u, 0xb2u, 0x41u, 0x46u, 0x47u, 0x45u, 0x42u,
    0x52u, 0x4fu, 0x4bu, 0x45u, 0x4eu, 0x00u, 0x8eu, 0x00u, 0x91u, 0x20u, 0x52u,
    0x45u, 0x41u, 0x47u, 0x45u, 0x45u, 0x52u, 0x54u, 0x4eu, 0x00u, 0xf1u, 0x07u,
    0x00u, 0x54u, 0x45u, 0x20u, 0x57u, 0x45u, 0x49u, 0x4eu, 0x49u, 0x47u, 0x20u,
    0x50u, 0x49u, 0x43u, 0x4fu, 0x2du, 0x47u, 0x45u, 0x48u, 0x45u, 0x55u, 0x47u,
    0xd6u, 0x00u, 0x94u, 0x56u, 0x4fu, 0x4cu, 0x4cu, 0x45u, 0x44u, 0x49u, 0x47u,
    0x20u, 0xbbu, 0x00u, 0x06u, 0x04u, 0x01u, 0x07u, 0x55u, 0x00u, 0x06u, 0xfeu,
    0x00u, 0x01u, 0xe7u, 0x00u, 0x00u};

/** Raw LZ4 block for the complete 24x40 SAA5050 help screen. */
static const uint8_t help_screen_lz4[] =
    "\xff\x02\x04\x1d\x07\x20\x50\x32\x30\x30\x30\x54\x20\x20\x48\x55"
    "\x4c\x50\x20\x01\x00\x04\x2f\x14\x73\x01\x00\x13\x33\x07\x1d\x04"
    "\x32\x00\xff\x0c\x42\x45\x44\x49\x45\x4e\x49\x4e\x47\x20\x56\x41"
    "\x4e\x20\x44\x45\x20\x43\x41\x52\x54\x52\x49\x44\x47\x45\x20\x01"
    "\x00\x18\x01\xa0\x00\xf0\x01\x41\x47\x49\x4e\x41\x20\x45\x4e\x20"
    "\x56\x45\x52\x42\x49\x4e\x44\x55\x00\x0b\x01\x00\x00\x78\x00\xfb"
    "\x0a\x31\x30\x30\x2d\x38\x39\x39\x20\x20\x54\x59\x50\x20\x44\x52"
    "\x49\x45\x20\x43\x49\x4a\x46\x45\x52\x53\x28\x00\xe2\x53\x54\x41"
    "\x52\x54\x2f\x49\x20\x20\x49\x4e\x44\x45\x58\x5e\x00\x00\x3d\x00"
    "\x0c\x50\x00\xf4\x0c\x3c\x2d\x2f\x50\x20\x2d\x3e\x2f\x4e\x20\x56"
    "\x4f\x52\x49\x47\x45\x20\x2f\x20\x56\x4f\x4c\x47\x45\x4e\x44\x45"
    "\x94\x00\x01\x28\x00\x14\x56\x35\x00\x4e\x41\x55\x54\x4f\x23\x00"
    "\x05\xc8\x00\x7e\x57\x45\x45\x52\x47\x41\x56\x00\x01\x0b\x78\x00"
    "\x32\x3f\x2f\x52\x0d\x00\x00\xef\x00\xf2\x06\x4f\x52\x47\x45\x4e"
    "\x20\x54\x45\x4b\x53\x54\x20\x4f\x4e\x54\x48\x55\x4c\x4c\x45\x4e"
    "\x28\x00\x24\x5a\x20\x6b\x01\xf4\x07\x4f\x56\x45\x4e\x20\x2f\x20"
    "\x4f\x4e\x44\x45\x52\x20\x2f\x20\x4e\x4f\x52\x4d\x41\x41\x4c\x78"
    "\x00\x32\x53\x55\x42\x8c\x00\x2f\x27\x53\x78\x00\x0a\x05\x1e\x00"
    "\x86\x4b\x49\x45\x53\x20\x45\x45\x4e\x3a\x00\x09\x28\x00\x05\x0e"
    "\x00\x06\x1f\x00\x40\x50\x41\x55\x5a\x20\x01\x45\x44\x4f\x4f\x52"
    "\x28\x00\x1d\x57\x50\x00\x11\x41\xa1\x00\xc2\x57\x49\x46\x49\x2d"
    "\x4e\x45\x54\x57\x45\x52\x4b\x90\x01\x12\x4f\x78\x02\x01\x1f\x00"
    "\x50\x45\x20\x42\x52\x4f\xce\x00\xc1\x49\x4e\x56\x4f\x45\x52\x20"
    "\x54\x45\x52\x55\x47\x50\x00\x14\x48\x50\x00\x41\x44\x45\x5a\x45"
    "\xae\x02\x0b\x9d\x00\x0f\x01\x00\x40\x0f\xf8\x02\x15\x0f\xd0\x02"
    "\x19\x01\xc3\x00\x31\x52\x55\x4b\x13\x01\x83\x54\x4f\x45\x54\x53"
    "\x20\x4f\x4d\xe8\x00\x7f\x54\x45\x20\x47\x41\x41\x4e\x4b\x00\x11"
    "\x50\x20\x20\x20\x20\x20";

/** Six animation frames for the top-left page-fetch indicator. */
static const uint8_t indicator_frames[6][4] = {
    {0x17u, 0x21u, 0x19u, 0x07u}, {0x17u, 0x22u, 0x19u, 0x07u},
    {0x17u, 0x28u, 0x19u, 0x07u}, {0x17u, 0x60u, 0x19u, 0x07u},
    {0x17u, 0x30u, 0x19u, 0x07u}, {0x17u, 0x24u, 0x19u, 0x07u},
};

static void present_page(const viewer_state_t *state);

/**
 * @brief Shows the compressed resident help page and restores viewer content.
 * @param state Viewer state to restore, or null when called from a menu.
 */
static void show_help(const viewer_state_t *state) {
  lz4_decompress(help_screen_lz4, display_screen,
                 (uint16_t)(sizeof(help_screen_lz4) - 1u));
  platform_present_screen(display_screen);
  (void)platform_read_key();
  if (state != 0) present_page(state);
}

/**
 * @brief Displays cartridge and queried Pico firmware version information.
 * @param session Active protocol session.
 */
static void show_source_runtime_info(p2wp_session_t *session) {
  p2wp_response_t reply;
  ui_panel(18u, "CARTRIDGE: v0.5.0 / PICO v0.0.0");
  if ((session->capabilities & P2WP_CAPABILITY_DEVICE_INFO) != 0u &&
      p2wp_request(session, P2WP_TYPE_DEVICE_INFO, 0, 0u, &reply) == P2WP_OK &&
      reply.payload_length == 4u) {
    platform_write_u8(18u, 29u, reply.payload[1]);
    platform_write_u8(18u, 31u, reply.payload[2]);
    platform_write_u8(18u, 33u, reply.payload[3]);
  }
  ui_action(19u, "LAATSTE VERSIE ONLINE: v0.5.0");
}

/**
 * @brief Loads the persisted custom URL when supported by the protocol.
 * @param session Active protocol session.
 */
static void load_custom_url(p2wp_session_t *session) {
  p2wp_response_t reply;
  uint8_t index;
  if (session->version < 5u ||
      p2wp_request(session, P2WP_TYPE_TELETEKST_CUSTOM_URL_LOAD, 0, 0u,
                   &reply) != P2WP_OK ||
      reply.payload_length == 0u || reply.payload[0] == 0u ||
      reply.payload[0] > MAX_CUSTOM_URL ||
      reply.payload_length != (uint16_t)(reply.payload[0] + 1u))
    return;
  custom_url_length = reply.payload[0];
  for (index = 0u; index != custom_url_length; ++index)
    custom_url[index] = reply.payload[1u + index];
}

/**
 * @brief Persists the current custom URL when supported by the protocol.
 * @param session Active protocol session.
 */
static void save_custom_url(p2wp_session_t *session) {
  p2wp_response_t reply;
  uint8_t index;
  if (session->version < 5u) return;
  fetch_request[0] = custom_url_length;
  for (index = 0u; index != custom_url_length; ++index)
    fetch_request[1u + index] = custom_url[index];
  (void)p2wp_request(session, P2WP_TYPE_TELETEKST_CUSTOM_URL_SAVE,
                     fetch_request, (uint16_t)(custom_url_length + 1u), &reply);
}

/**
 * @brief Loads the source-menu autostart setting from Pico flash.
 * @param session Active protocol session.
 */
static void load_settings(p2wp_session_t *session) {
  p2wp_response_t reply;
  auto_start_source = AUTOSTART_DISABLED;
  if (session->version >= 6u &&
      p2wp_request(session, P2WP_TYPE_TELETEKST_SETTINGS_LOAD, 0, 0u, &reply) ==
          P2WP_OK &&
      reply.payload_length == 1u &&
      (reply.payload[0] < 4u || reply.payload[0] == AUTOSTART_DISABLED))
    auto_start_source = reply.payload[0];
}

/**
 * @brief Saves the current source-menu autostart setting.
 * @param session Active protocol session.
 */
static void save_settings(p2wp_session_t *session) {
  p2wp_response_t reply;
  (void)p2wp_request(session, P2WP_TYPE_TELETEKST_SETTINGS_SAVE,
                     &auto_start_source, 1u, &reply);
}

/**
 * @brief Renders the current persistent autostart choice on the source menu.
 * @param session Active protocol session.
 */
static void show_auto_start(p2wp_session_t *session) {
  const char *text = " AUTOSTART VEREIST P2WP/6";
  if (session->version >= 6u) {
    if (auto_start_source == AUTOSTART_DISABLED)
      text = " A AUTOSTART NA 60S: UIT";
    else if (auto_start_source == 0u)
      text = " A AUTOSTART NA 60S: EIGEN";
    else if (auto_start_source == 1u)
      text = " A AUTOSTART NA 60S: NOS";
    else if (auto_start_source == 2u)
      text = " A AUTOSTART NA 60S: P2000T";
    else
      text = " A AUTOSTART NA 60S: ARCHIEF";
  }
  ui_panel(11u, text);
}

/**
 * @brief Converts a source-menu digit into a wire source identifier.
 *
 * Stored settings use menu digits rather than wire identifiers. Protocols 4-6
 * route the Archive service through the compatible custom-source request.
 *
 * @param session Active protocol session.
 * @param menu Source-menu digit from zero through three.
 * @param[out] source Selected wire source identifier.
 * @return One when the menu source is supported, otherwise zero.
 */
static uint8_t select_menu_source(p2wp_session_t *session, uint8_t menu,
                                  uint8_t *source) {
  static const uint8_t archive_url[] = "https://teletekstarchief.nl";
  uint8_t index;
  if (menu == 1u) {
    *source = SOURCE_NOS;
    return 1u;
  }
  if (menu == 2u) {
    *source = SOURCE_P2000T;
    return 1u;
  }
  if (menu == 3u) {
    if (session->version >= 7u) {
      *source = SOURCE_ARCHIVE;
      return 1u;
    }
    if (session->version < 4u) return 0u;
    custom_url_length = (uint8_t)(sizeof(archive_url) - 1u);
    for (index = 0u; index != custom_url_length; ++index)
      custom_url[index] = archive_url[index];
    *source = SOURCE_CUSTOM;
    return 1u;
  }
  if (menu == 0u && session->version >= 4u) {
    load_custom_url(session);
    if (custom_url_length != 0u) {
      *source = SOURCE_CUSTOM;
      return 1u;
    }
  }
  return 0u;
}

/**
 * @brief Edits and optionally persists a custom server base URL.
 * @param session Active protocol session.
 * @return One when a nonempty URL is accepted, or zero when cancelled.
 */
static uint8_t choose_custom_url(p2wp_session_t *session) {
  uint8_t index;
  uint8_t key;
  uint8_t row = 9u;
  uint8_t column = 4u;

  load_custom_url(session);
  platform_clear_screen();
  ui_title(1u, " P2000T  EIGEN TELETEKSTSERVER");
  ui_rule(2u);
  ui_panel(3u, " BASISADRES VAN UW EIGEN SERVER");
  ui_panel(4u, " PICO ONTHOUDT ALLEEN EEN NIEUW ADRES");
  ui_panel(5u, " VOORBEELD  http://terra:8080");
  ui_panel(6u, " HTTPS: CERTIFICAATCONTROLE STAAT UIT");
  ui_action(8u, " SERVERADRES                 MAX. 96");
  ui_panel(9u, " ");
  ui_panel(10u, " ");
  ui_panel(11u, " ");
  ui_panel(13u, " ENTER OPSLAAN BS WIS STOP TERUG");
  ui_rule(22u);
  ui_footer();
  for (index = 0u; index != custom_url_length; ++index) {
    platform_write_bytes(row, column, custom_url + index, 1u);
    if (++column == 36u) {
      column = 4u;
      ++row;
    }
  }
  for (;;) {
    key = platform_read_key();
    if (key == P2000T_KEY_STOP || key == EMULATOR_STOP_KEY) return 0u;
    key = platform_translate_key(key);
    if (key == 13u) {
      if (custom_url_length != 0u) {
        save_custom_url(session);
        return 1u;
      }
      continue;
    }
    if (key == 8u) {
      if (custom_url_length == 0u) continue;
      --custom_url_length;
      if (column == 4u) {
        column = 36u;
        --row;
      }
      --column;
      platform_write_text(row, column, " ");
      continue;
    }
    if (key < 0x20u || key >= 0x7fu || custom_url_length == MAX_CUSTOM_URL)
      continue;
    custom_url[custom_url_length++] = key;
    platform_write_bytes(row, column, &key, 1u);
    if (++column == 36u) {
      column = 4u;
      ++row;
    }
  }
}

/**
 * @brief Shows the source menu or resolves a configured automatic source.
 * @param session Active protocol session.
 * @param use_autostart Whether the saved autostart source may be used.
 * @param[out] started_automatically Set when no menu interaction was needed.
 * @return Selected wire source identifier.
 */
static uint8_t choose_source(p2wp_session_t *session, uint8_t use_autostart,
                             uint8_t *started_automatically) {
  uint8_t key;
  uint8_t source;
  *started_automatically = 0u;
  if (use_autostart && auto_start_source != AUTOSTART_DISABLED &&
      select_menu_source(session, auto_start_source, &source)) {
    *started_automatically = 1u;
    return source;
  }
draw_menu:
  platform_clear_screen();
  ui_title(1u, " P2000T TELETEKST           BRONKEUZE");
  ui_rule(2u);
  ui_action(3u, "      KIES UW TELETEKSTBRON");
  ui_panel(4u, "");
  ui_panel(5u, "  1 - NOS TELETEKST");
  ui_panel(6u, "  2 - P2000T TELETEKST");
  ui_panel(7u, "  3 - TELETEKSTARCHIEF.NL");
  ui_panel(8u, "  0 - EIGEN SERVER");
  ui_rule(9u);
  ui_action(10u, "        KIES BRON (0-3)");
  show_auto_start(session);
  ui_action(12u, "      BEDIENING OP DE PAGINA");
  ui_panel(13u, " START/I INDEX ?/R ONTHUL Z ZOOM");
  ui_panel(14u, "  <-/P VORIGE     ->/N VOLGENDE");
  ui_panel(15u, "  A PAUZE/DOORGAAN  S SUBPAGINA");
  ui_panel(16u, "  V AUTO-PAGINA  W WIFI  H HULP");
  ui_panel(17u, "  STOP - ANDERE TELETEKSTBRON");
  show_source_runtime_info(session);
  ui_footer();
  for (;;) {
    key = platform_read_ascii();
    key = platform_lower_ascii(key);
    if (key == '1') return SOURCE_NOS;
    if (key == '2') return SOURCE_P2000T;
    if (key == '3' && select_menu_source(session, 3u, &source)) return source;
    if (key == '0' && session->version >= 4u && choose_custom_url(session))
      return SOURCE_CUSTOM;
    if (key == 'a' && session->version >= 6u) {
      if (auto_start_source == AUTOSTART_DISABLED)
        auto_start_source = 1u;
      else if (auto_start_source == 0u)
        auto_start_source = AUTOSTART_DISABLED;
      else if (++auto_start_source == 4u)
        auto_start_source = 0u;
      save_settings(session);
      show_auto_start(session);
    }
    if (key == 'h') {
      show_help(0);
      goto draw_menu;
    }
  }
}

/**
 * @brief Returns the fetch-status payload size for a protocol revision.
 * @param version Negotiated P2WP version.
 * @return Exact expected status payload length.
 */
static uint8_t expected_status_length(uint8_t version) {
  if (version >= 7u) return 21u;
  if (version >= 4u) return 17u;
  if (version == 3u) return 13u;
  return 5u;
}

/**
 * @brief Draws the current four-byte page-fetch animation frame.
 */
static void indicator_draw(void) {
  platform_write_bytes(0u, 0u, indicator_frames[indicator_phase], 4u);
}

/**
 * @brief Saves the page corner and starts the fetch animation.
 */
static void indicator_begin(void) {
  platform_read_bytes(0u, 0u, indicator_saved, sizeof(indicator_saved));
  indicator_phase = 0u;
  indicator_draw();
}

/**
 * @brief Advances and draws the cyclic page-fetch animation.
 */
static void indicator_next(void) {
  ++indicator_phase;
  if (indicator_phase == 6u) indicator_phase = 0u;
  indicator_draw();
}

/**
 * @brief Restores the four screen bytes covered by the fetch animation.
 */
static void indicator_restore(void) {
  platform_write_bytes(0u, 0u, indicator_saved, sizeof(indicator_saved));
}

/**
 * @brief Records a page-fetch error and removes the fetch animation.
 * @param state Viewer state to update.
 * @param error Cartridge or remote error code.
 * @return Always zero for direct use in failure returns.
 */
static uint8_t fail_fetch(viewer_state_t *state, uint8_t error) {
  state->error = error;
  indicator_restore();
  return 0u;
}

/**
 * @brief Converts a local or remote P2WP failure into viewer error state.
 * @param state Viewer state to update.
 * @param result P2WP transaction result.
 * @param reply Response containing an optional remote error code.
 * @return Always zero for direct use in failure returns.
 */
static uint8_t request_failure(viewer_state_t *state, enum p2wp_result result,
                               const p2wp_response_t *reply) {
  return fail_fetch(state, result == P2WP_REMOTE_ERROR
                               ? reply->remote_error
                               : (uint8_t)(0x80u + result));
}

/**
 * @brief Formats the viewer's compact date/time metadata.
 * @param state Viewer state containing clock fields.
 * @param[out] out Destination text buffer.
 * @return Number of formatted display bytes.
 */
static uint8_t clock_text(const viewer_state_t *state, uint8_t *out) {
  return platform_format_clock((const uint8_t *)state, out);
}

/**
 * @brief Inserts valid provider clock metadata into the raw page header.
 * @param state Viewer state containing clock fields.
 */
static void clock_overlay_raw(const viewer_state_t *state) {
  if (!state->clock_valid) return;
  page_screen[0] = 0x03u;
  (void)clock_text(state, page_screen + 1u);
}

/**
 * @brief Advances and redraws the live clock when its deadline expires.
 * @param state Viewer state and clock metadata to update.
 */
static void clock_update(viewer_state_t *state) {
  uint8_t text[20];
  uint8_t length;
  if (!platform_advance_clock((uint8_t *)state)) return;
  length = clock_text(state, text);
  if (!state->page_visible) {
    platform_write_bytes(0u, (uint8_t)(40u - length), text, length);
    return;
  }
  platform_write_bytes(0u, state->zoom == 0u ? 1u : 2u, text, length);
  {
    uint8_t index;
    for (index = 0u; index != length; ++index) {
      page_screen[1u + index] = text[index];
      display_screen[(state->zoom == 0u ? 1u : 2u) + index] = text[index];
    }
  }
}

/**
 * @brief Renders the raw page into the packed display buffer.
 * @param state Viewer state selecting zoom and reveal modes.
 */
static void render_page(const viewer_state_t *state) {
  platform_render_page(page_screen, display_screen, state->zoom, state->reveal);
}

/**
 * @brief Renders and atomically presents the current page and mode markers.
 * @param state Current viewer state.
 */
static void present_page(const viewer_state_t *state) {
  render_page(state);
  platform_present_screen(display_screen);
  if (state->auto_page) platform_write_text(0u, 35u, "V");
  if (state->rotation_paused) platform_write_text(0u, 39u, "A");
}

/**
 * @brief Fetches page metadata and four display chunks from the Pico.
 * @param session Active protocol session.
 * @param state Requested page state, updated with metadata and errors.
 * @return One after presenting a complete page, otherwise zero.
 */
static uint8_t fetch_page(p2wp_session_t *session, viewer_state_t *state) {
  uint8_t chunk;
  uint8_t request_length = 4u;
  uint8_t *destination = page_screen;
  uint16_t tries = 750u;
  p2wp_response_t reply;
  enum p2wp_result result;

  state->error = 0u;
  state->http_result = 0u;
  state->lwip_error = 0u;
  state->http_status = 0u;
  state->error_details = session->version >= 7u;
  indicator_begin();
  fetch_request[0] = (uint8_t)state->page;
  fetch_request[1] = (uint8_t)(state->page >> 8u);
  fetch_request[2] = state->subpage;
  fetch_request[3] = state->source;
  if (state->source == SOURCE_CUSTOM) {
    fetch_request[4] = custom_url_length;
    for (chunk = 0u; chunk != custom_url_length; ++chunk)
      fetch_request[5u + chunk] = custom_url[chunk];
    request_length = (uint8_t)(5u + custom_url_length);
  }
  result = p2wp_request(session, P2WP_TYPE_TELETEKST_FETCH_START, fetch_request,
                        request_length, &reply);
  if (result != P2WP_OK) return request_failure(state, result, &reply);
  if (reply.payload_length != 0u) return fail_fetch(state, 0x82u);

  while (tries-- != 0u) {
    platform_wait_ticks(2u);
    indicator_next();
    result =
        p2wp_request(session, P2WP_TYPE_TELETEKST_FETCH_STATUS, 0, 0u, &reply);
    if (result != P2WP_OK) return request_failure(state, result, &reply);
    if (reply.payload_length != expected_status_length(session->version))
      return fail_fetch(state, 0x82u);
    if (reply.payload[0] == FETCH_COMPLETE) break;
    if (reply.payload[0] == FETCH_FAILED) {
      if (session->version >= 7u) {
        state->http_result = reply.payload[17];
        state->lwip_error = reply.payload[18];
        state->http_status =
            (uint16_t)reply.payload[19] | ((uint16_t)reply.payload[20] << 8u);
      }
      return fail_fetch(state, reply.payload[1]);
    }
    if (reply.payload[0] != FETCH_CONNECTING &&
        reply.payload[0] != FETCH_RECEIVING)
      return fail_fetch(state, 0x82u);
  }
  if (reply.payload[0] != FETCH_COMPLETE) return fail_fetch(state, 0x81u);

  state->next_subpage = reply.payload[4];
  if (session->version >= 3u) {
    state->clock_valid = reply.payload[8];
    state->hour = reply.payload[5];
    state->minute = reply.payload[6];
    state->second = reply.payload[7];
    state->day = reply.payload[9];
    state->month = reply.payload[10];
    state->year = reply.payload[11];
    state->weekday = reply.payload[12];
    state->clock_has_date = state->day != 0u && state->month != 0u;
    state->clock_blink = 0u;
    state->clock_tick = (uint16_t)(platform_clock() + 25u);
  } else {
    state->clock_valid = 0u;
  }
  state->previous_page = 0u;
  state->next_page = 0u;
  if (session->version >= 4u) {
    state->previous_page =
        (uint16_t)reply.payload[13] | ((uint16_t)reply.payload[14] << 8u);
    state->next_page =
        (uint16_t)reply.payload[15] | ((uint16_t)reply.payload[16] << 8u);
  }

  for (chunk = 0u; chunk != CHUNK_COUNT; ++chunk) {
    result = p2wp_request(session, P2WP_TYPE_TELETEKST_FETCH_ROWS, &chunk, 1u,
                          &reply);
    if (result != P2WP_OK) return request_failure(state, result, &reply);
    if (reply.payload_length != CHUNK_SIZE) return fail_fetch(state, 0x82u);
    {
      uint16_t index;
      for (index = 0u; index != CHUNK_SIZE; ++index)
        *destination++ = reply.payload[index];
    }
  }
  state->reveal = 0u;
  state->zoom = 0u;
  state->page_visible = 1u;
  clock_overlay_raw(state);
  present_page(state);
  if (state->next_subpage != 0u) state->cycle_started = 1u;
  if (state->auto_page || (!state->rotation_paused &&
                           (state->next_subpage != 0u || state->cycle_started)))
    state->rotation_deadline = (uint16_t)(platform_clock() + 500u);
  return 1u;
}

/**
 * @brief Presents a missing-page or diagnostic fetch-error screen.
 * @param state Viewer state containing page and error details.
 */
static void show_fetch_error(viewer_state_t *state) {
  const char *description;
  uint8_t index;
  state->page_visible = 0u;
  platform_clear_screen();
  ui_title(0u, " P2000T  TELETEKST VIA PICO W");
  if (state->error == 8u) {
    ui_rule(2u);
    ui_action(9u, "       PAGINA NIET GEVONDEN");
    ui_panel(13u, "    TYP EEN NIEUW PAGINANUMMER");
    return;
  }
  ui_panel(2u, "TELETEKSTPAGINA NIET BESCHIKBAAR");
  ui_panel(3u, "PAGINA:");
  platform_write_page(3u, 11u, state->page);
  ui_panel(4u, "FOUTCODE: 00");
  platform_write_hex(4u, 13u, state->error);
  ui_panel(5u, "FOUT:");
  lz4_decompress(error_text_lz4, error_text, sizeof(error_text_lz4));
  description = (const char *)error_text;
  index = state->error >= 1u && state->error <= 15u
              ? (uint8_t)(state->error - 1u)
              : 15u;
  while (index-- != 0u) {
    while (*description++ != 0);
  }
  platform_write_text(5u, 9u, description);
  if (state->error_details) {
    ui_panel(6u, "DETAIL: HTTP 000 LWIP 00 NET 00");
    platform_write_page(6u, 16u, state->http_status);
    platform_write_hex(6u, 25u, state->lwip_error);
    platform_write_hex(6u, 32u, state->http_result);
  }
  ui_action(8u, "PROBEER OPNIEUW OF KIES ANDERE BRON");
}

/**
 * @brief Clears the four page-entry cells in the visible page header.
 */
static void clear_page_input(void) {
  static const uint8_t spaces[4] = {' ', ' ', ' ', ' '};
  platform_write_bytes(0u, 36u, spaces, sizeof(spaces));
}

/**
 * @brief Requests a manually chosen page and displays any error.
 * @param session Active protocol session.
 * @param state Viewer state to reset and update.
 * @param page Page number from 100 through 899.
 */
static void request_page(p2wp_session_t *session, viewer_state_t *state,
                         uint16_t page) {
  state->page = page;
  state->subpage = 0u;
  state->rotation_paused = 0u;
  state->cycle_started = 0u;
  if (!fetch_page(session, state)) show_fetch_error(state);
}

/**
 * @brief Schedules automatic retry or numeric skip after a fetch failure.
 * @param state Viewer state containing the failed page and error.
 */
static void handle_auto_failure(viewer_state_t *state) {
  if (state->page == 100u) {
    state->auto_page = 0u;
    show_fetch_error(state);
    return;
  }
  if (state->error >= 5u && state->error <= 8u) {
    if (++state->page == 900u) state->page = 100u;
  }
  state->auto_retry = 1u;
  state->rotation_deadline = (uint16_t)(platform_clock() + 500u);
}

/**
 * @brief Requests a page as part of automatic navigation.
 * @param session Active protocol session.
 * @param state Viewer state to reset and update.
 * @param page Page number to request.
 */
static void request_auto_page(p2wp_session_t *session, viewer_state_t *state,
                              uint16_t page) {
  state->page = page;
  state->subpage = 0u;
  state->cycle_started = 0u;
  if (!fetch_page(session, state)) handle_auto_failure(state);
}

/**
 * @brief Restores navigation cells after cancelled subpage entry.
 * @param state Viewer state controlling the pause marker.
 */
static void restore_header(const viewer_state_t *state) {
  platform_write_bytes(0u, 36u, page_screen + 36u, 4u);
  if (state->rotation_paused) platform_write_text(0u, 39u, "A");
}

/**
 * @brief Reads a one- or two-digit subpage and requests it.
 * @param session Active protocol session.
 * @param state Viewer state to update.
 */
static void select_subpage(p2wp_session_t *session, viewer_state_t *state) {
  uint8_t first;
  uint8_t second;
  uint8_t key;

  clear_page_input();
  platform_write_text(0u, 36u, "S:");
  for (;;) {
    key = platform_read_key();
    if (key == P2000T_KEY_STOP || key == EMULATOR_STOP_KEY) {
      restore_header(state);
      return;
    }
    key = platform_translate_key(key);
    if (key >= '0' && key <= '9') {
      first = key;
      platform_write_bytes(0u, 38u, &key, 1u);
      break;
    }
  }
  for (;;) {
    key = platform_read_key();
    if (key == P2000T_KEY_STOP || key == EMULATOR_STOP_KEY) {
      restore_header(state);
      return;
    }
    key = platform_translate_key(key);
    if (key == 13u) {
      state->subpage = (uint8_t)(first - '0');
      break;
    }
    if (key >= '0' && key <= '9') {
      second = key;
      platform_write_bytes(0u, 39u, &key, 1u);
      state->subpage = (uint8_t)((first - '0') * 10u + second - '0');
      break;
    }
  }
  state->rotation_paused = 1u;
  state->cycle_started = 0u;
  if (!fetch_page(session, state)) show_fetch_error(state);
}

/**
 * @brief Toggles automatic subpage rotation and its visible marker.
 * @param state Viewer state to update.
 */
static void toggle_rotation(viewer_state_t *state) {
  state->rotation_paused ^= 1u;
  if (state->rotation_paused) {
    platform_write_text(0u, 39u, "A");
    return;
  }
  platform_write_bytes(0u, 39u, page_screen + 39u, 1u);
  if (state->next_subpage != 0u || state->cycle_started)
    state->rotation_deadline = (uint16_t)(platform_clock() + 500u);
}

/**
 * @brief Toggles automatic page navigation and its visible marker.
 * @param state Viewer state to update.
 */
static void toggle_auto_page(viewer_state_t *state) {
  state->auto_page ^= 1u;
  if (state->auto_page) {
    platform_write_text(0u, 35u, "V");
    state->rotation_deadline = (uint16_t)(platform_clock() + 500u);
  } else {
    platform_write_bytes(0u, 35u, display_screen + 35u, 1u);
  }
}

/**
 * @brief Requests the next subpage or wraps to the provider default.
 * @param session Active protocol session.
 * @param state Viewer state to update.
 */
static void rotate_subpage(p2wp_session_t *session, viewer_state_t *state) {
  if (state->next_subpage != 0u) {
    state->subpage = state->next_subpage;
  } else {
    state->subpage = 0u;
    state->cycle_started = 0u;
  }
  if (!fetch_page(session, state)) {
    if (state->auto_page)
      handle_auto_failure(state);
    else {
      state->next_subpage = 0u;
      state->cycle_started = 0u;
    }
  }
}

/**
 * @brief Processes page-viewer keys, timers, navigation, and Wi-Fi escape.
 * @param session Active protocol session.
 * @param state Viewer state to update.
 * @return One to reconfigure Wi-Fi, or zero to return to source selection.
 */
static uint8_t viewer_loop(p2wp_session_t *session, viewer_state_t *state) {
  uint8_t input[3];
  uint8_t input_count = 0u;
  uint8_t raw;
  uint8_t key;
  uint8_t status;

  for (;;) {
    status = platform_key_status();
    if (status == 2u) return 0u;
    if (status == 0u) {
      clock_update(state);
      if (input_count == 0u &&
          (state->auto_page ||
           (!state->rotation_paused &&
            (state->next_subpage != 0u || state->cycle_started))) &&
          (int16_t)(platform_clock() - state->rotation_deadline) >= 0) {
        if (!state->rotation_paused && state->next_subpage != 0u)
          rotate_subpage(session, state);
        else if (state->auto_page) {
          uint16_t page =
              state->auto_retry
                  ? state->page
                  : (state->next_page != 0u ? state->next_page : 100u);
          state->auto_retry = 0u;
          request_auto_page(session, state, page);
        } else
          rotate_subpage(session, state);
      }
      continue;
    }
    raw = platform_read_key();
    if (raw == P2000T_KEY_STOP || raw == EMULATOR_STOP_KEY) return 0u;
    key = raw == RAW_START_KEY ? P2000T_KEY_START : platform_translate_key(raw);
    key = platform_lower_ascii(key);
    if (key == P2000T_KEY_START || key == 'i') {
      input_count = 0u;
      request_page(session, state, 100u);
      continue;
    }
    if (key == 'p' || key == P2000T_KEY_LEFT) {
      input_count = 0u;
      if (state->previous_page != 0u)
        request_page(session, state, state->previous_page);
      continue;
    }
    if (key == 'n' || key == P2000T_KEY_RIGHT) {
      input_count = 0u;
      if (state->next_page != 0u)
        request_page(session, state, state->next_page);
      continue;
    }
    if (key == 'a') {
      input_count = 0u;
      toggle_rotation(state);
      continue;
    }
    if (key == 's') {
      input_count = 0u;
      select_subpage(session, state);
      continue;
    }
    if (key == 'r' || key == '?') {
      input_count = 0u;
      state->reveal ^= 1u;
      if (state->zoom == 0u) {
        render_page(state);
        platform_commit_reveal(page_screen, state->reveal);
      } else {
        present_page(state);
      }
      continue;
    }
    if (key == 'z') {
      input_count = 0u;
      ++state->zoom;
      if (state->zoom == 3u) state->zoom = 0u;
      present_page(state);
      continue;
    }
    if (key == 'h') {
      input_count = 0u;
      show_help(state);
      if (state->auto_page ||
          (!state->rotation_paused &&
           (state->next_subpage != 0u || state->cycle_started)))
        state->rotation_deadline = (uint16_t)(platform_clock() + 500u);
      continue;
    }
    if (key == 'v') {
      input_count = 0u;
      toggle_auto_page(state);
      continue;
    }
    if (key == 'w') return 1u;
    if (key == 8u) {
      if (input_count != 0u) {
        --input_count;
        platform_write_text(0u, (uint8_t)(36u + input_count), " ");
      }
      continue;
    }
    if (key < '0' || key > '9') continue;
    if (input_count == 0u) {
      if (key < '1' || key > '8') continue;
      clear_page_input();
    }
    input[input_count] = key;
    platform_write_bytes(0u, (uint8_t)(36u + input_count), &key, 1u);
    ++input_count;
    if (input_count == 3u) {
      uint16_t page = (uint16_t)(input[0] - '0');
      page = (uint16_t)(page * 10u + input[1] - '0');
      page = (uint16_t)(page * 10u + input[2] - '0');
      input_count = 0u;
      request_page(session, state, page);
    }
  }
}

/**
 * @brief Runs source selection and the interactive page viewer indefinitely.
 */
void teletekst_start(p2wp_session_t *session, uint8_t opening_timed_out) {
  viewer_state_t state;
  uint8_t started_automatically;
  load_settings(session);
  for (;;) {
    state.source =
        choose_source(session, opening_timed_out, &started_automatically);
    opening_timed_out = 0u;
    state.page = 100u;
    state.subpage = 0u;
    state.next_subpage = 0u;
    state.previous_page = 0u;
    state.next_page = 0u;
    state.rotation_paused = 0u;
    state.cycle_started = 0u;
    state.reveal = 0u;
    state.zoom = 0u;
    state.auto_page = started_automatically;
    state.auto_retry = 0u;
    if (!fetch_page(session, &state)) show_fetch_error(&state);
    if (viewer_loop(session, &state) != 0u) {
      (void)wifi_reconfigure(session);
    }
  }
}
