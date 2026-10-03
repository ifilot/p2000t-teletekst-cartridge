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
  SOURCE_INTERNATIONAL = 4,
  MAX_CUSTOM_URL = 96,
  SCREEN_SIZE = 960,
  CHUNK_SIZE = 240,
  CHUNK_COUNT = 4,
  RAW_START_KEY = 128,
  EMULATOR_STOP_KEY = 0x58,
  AUTOSTART_DISABLED = 0xff,
};

/** ROM policy for the date/time overlay; providers may draw their own clock. */
#define CLOCK_OVERLAY_ENABLED 1
/** Enable the overlay on international channels; disabled to preserve headers.
 */
#define CLOCK_OVERLAY_INTERNATIONAL 0

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
/** Selected international channel, or 0xff for the catalogue. */
static uint8_t international_channel = 0xffu;
/** Saved source-menu digit, or AUTOSTART_DISABLED. */
static uint8_t auto_start_source = AUTOSTART_DISABLED;
/** Video row temporarily replaced by the fetch indicator. */
static uint8_t indicator_saved[P2000T_SCREEN_COLUMNS];
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

/** Raw LZ4 help screen; the current version footer is drawn separately. */
static const uint8_t help_screen_lz4[] = {
    0xffu, 0x0cu, 0x04u, 0x1du, 0x07u, 0x20u, 0x50u, 0x32u, 0x30u, 0x30u, 0x30u,
    0x54u, 0x20u, 0x54u, 0x45u, 0x4cu, 0x45u, 0x54u, 0x45u, 0x4bu, 0x53u, 0x54u,
    0x20u, 0x20u, 0x48u, 0x55u, 0x4cu, 0x50u, 0x20u, 0x01u, 0x00u, 0x22u, 0x8fu,
    0x03u, 0x20u, 0x50u, 0x41u, 0x47u, 0x49u, 0x4eu, 0x41u, 0x28u, 0x00u, 0x0du,
    0xfeu, 0x01u, 0x06u, 0x20u, 0x31u, 0x30u, 0x30u, 0x2du, 0x38u, 0x39u, 0x39u,
    0x07u, 0x20u, 0x20u, 0x4bu, 0x49u, 0x45u, 0x53u, 0x37u, 0x00u, 0x04u, 0x28u,
    0x00u, 0xf3u, 0x01u, 0x53u, 0x54u, 0x41u, 0x52u, 0x54u, 0x20u, 0x2fu, 0x20u,
    0x49u, 0x07u, 0x20u, 0x49u, 0x4eu, 0x44u, 0x45u, 0x58u, 0x2au, 0x00u, 0x00u,
    0x40u, 0x00u, 0x08u, 0x28u, 0x00u, 0xf0u, 0x06u, 0x5bu, 0x20u, 0x2fu, 0x20u,
    0x50u, 0x07u, 0x20u, 0x56u, 0x4fu, 0x52u, 0x49u, 0x47u, 0x45u, 0x20u, 0x20u,
    0x06u, 0x5du, 0x20u, 0x2fu, 0x20u, 0x4eu, 0x10u, 0x00u, 0x66u, 0x4cu, 0x47u,
    0x45u, 0x4eu, 0x44u, 0x45u, 0x50u, 0x00u, 0x76u, 0x56u, 0x07u, 0x20u, 0x41u,
    0x55u, 0x54u, 0x4fu, 0x1au, 0x00u, 0x0eu, 0xb1u, 0x00u, 0x0fu, 0xf0u, 0x00u,
    0x1au, 0x32u, 0x53u, 0x55u, 0x42u, 0x42u, 0x00u, 0x2fu, 0x27u, 0x53u, 0xf0u,
    0x00u, 0x0au, 0x55u, 0x3cu, 0x20u, 0x2fu, 0x20u, 0x3eu, 0xa1u, 0x00u, 0x1du,
    0x2fu, 0x9au, 0x00u, 0x05u, 0xf0u, 0x00u, 0x12u, 0x07u, 0x11u, 0x01u, 0x36u,
    0x45u, 0x45u, 0x4eu, 0x5cu, 0x00u, 0x1eu, 0x20u, 0x50u, 0x00u, 0xfeu, 0x07u,
    0x4cu, 0x20u, 0x2fu, 0x20u, 0x41u, 0x07u, 0x20u, 0x4cu, 0x55u, 0x53u, 0x53u,
    0x45u, 0x4eu, 0x20u, 0x41u, 0x41u, 0x4eu, 0x2fu, 0x55u, 0x49u, 0x54u, 0x20u,
    0xa0u, 0x00u, 0x8fu, 0x57u, 0x45u, 0x45u, 0x52u, 0x47u, 0x41u, 0x56u, 0x45u,
    0x90u, 0x01u, 0x0du, 0xf2u, 0x02u, 0x3fu, 0x20u, 0x2fu, 0x20u, 0x52u, 0x07u,
    0x20u, 0x56u, 0x45u, 0x52u, 0x42u, 0x4fu, 0x52u, 0x47u, 0x45u, 0x4eu, 0x20u,
    0x0cu, 0x02u, 0x5fu, 0x54u, 0x4fu, 0x4eu, 0x45u, 0x4eu, 0x18u, 0x01u, 0x21u,
    0x00u, 0x49u, 0x00u, 0x6fu, 0x49u, 0x4eu, 0x44u, 0x49u, 0x4eu, 0x47u, 0x78u,
    0x00u, 0x0bu, 0x17u, 0x57u, 0xf0u, 0x00u, 0xf8u, 0x03u, 0x41u, 0x4eu, 0x44u,
    0x45u, 0x52u, 0x20u, 0x57u, 0x49u, 0x46u, 0x49u, 0x2du, 0x4eu, 0x45u, 0x54u,
    0x57u, 0x45u, 0x52u, 0x4bu, 0x08u, 0x02u, 0x32u, 0x4fu, 0x50u, 0x07u, 0x22u,
    0x00u, 0x16u, 0x45u, 0xacu, 0x02u, 0x4fu, 0x42u, 0x52u, 0x4fu, 0x4eu, 0xb8u,
    0x01u, 0x23u, 0x00u, 0x3au, 0x00u, 0x5fu, 0x4bu, 0x45u, 0x55u, 0x5au, 0x45u,
    0x18u, 0x01u, 0x0cu, 0x12u, 0x41u, 0x30u, 0x02u, 0x01u, 0x87u, 0x02u, 0x01u,
    0x6eu, 0x00u, 0x50u, 0x57u, 0x49u, 0x4au, 0x5au, 0x49u, 0x21u, 0x01u, 0x0au,
    0x28u, 0x00u, 0x22u, 0x48u, 0x07u, 0x37u, 0x03u, 0x8fu, 0x56u, 0x41u, 0x4eu,
    0x41u, 0x46u, 0x20u, 0x44u, 0x45u, 0x61u, 0x00u, 0x03u, 0x00u, 0x70u, 0x03u,
    0x41u, 0x44u, 0x52u, 0x55u, 0x4bu, 0xefu, 0x00u, 0xffu, 0x06u, 0x54u, 0x4fu,
    0x45u, 0x54u, 0x53u, 0x20u, 0x4fu, 0x4du, 0x20u, 0x54u, 0x45u, 0x52u, 0x55u,
    0x47u, 0x20u, 0x54u, 0x45u, 0x20u, 0x47u, 0x41u, 0x41u, 0xcfu, 0x00u, 0x16u,
    0x50u, 0x20u, 0x20u, 0x20u, 0x20u, 0x20u,
};

/** Six animation frames for the top-left page-fetch indicator. */
static const uint8_t indicator_frames[6][4] = {
    {0x17u, 0x21u, 0x19u, 0x07u}, {0x17u, 0x22u, 0x19u, 0x07u},
    {0x17u, 0x28u, 0x19u, 0x07u}, {0x17u, 0x60u, 0x19u, 0x07u},
    {0x17u, 0x30u, 0x19u, 0x07u}, {0x17u, 0x24u, 0x19u, 0x07u},
};

static void present_page(const viewer_state_t *state);

/**
 * @brief Shows the compressed resident help page and restores viewer content.
 * @param[in] state Viewer state to restore, or null when called from a menu.
 */
static void show_help(const viewer_state_t *state) {
  uint8_t key;
help_controls:
  lz4_decompress(help_screen_lz4, display_screen, sizeof(help_screen_lz4));
  platform_present_screen(display_screen);
  ui_footer();
  ui_action(22u, "2: INFO   ANDERE TOETS: TERUG");
  key = platform_translate_key(platform_read_key());
  if (key == '2') {
    ui_build_info();
    if (platform_translate_key(platform_read_key()) == '1') goto help_controls;
  }
  if (state != 0) present_page(state);
}

/**
 * @brief Loads the persisted custom URL when supported by the protocol.
 * @param[in,out] session Active session; requests advance its sequence.
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
 * @param[in,out] session Active session; requests advance its sequence.
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
 * @param[in,out] session Active session; requests advance its sequence.
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
  if (session->version < 7u && auto_start_source == 3u)
    auto_start_source = AUTOSTART_DISABLED;
}

/**
 * @brief Saves the current source-menu autostart setting.
 * @param[in] session Active protocol session.
 */
static void save_settings(p2wp_session_t *session) {
  p2wp_response_t reply;
  (void)p2wp_request(session, P2WP_TYPE_TELETEKST_SETTINGS_SAVE,
                     &auto_start_source, 1u, &reply);
}

/**
 * @brief Renders the current persistent autostart choice on the source menu.
 * @param[in] session Active protocol session.
 */
static void show_auto_start(p2wp_session_t *session) {
  const char *text = "UIT";
  if (session->version < 6u)
    text = "VEREIST P2WP/6";
  else if (auto_start_source == 0u)
    text = "EIGEN";
  else if (auto_start_source == 1u)
    text = "NOS";
  else if (auto_start_source == 2u)
    text = "P2000T";
  else if (auto_start_source == 3u)
    text = "ARCHIEF";
  platform_clear_line(21u);
  platform_write_text(21u, 0u, "\006A\007 AUTOSTART NA 60S: ");
  platform_write_text(21u, 22u, text);
}

/**
 * @brief Converts a source-menu digit into a wire source identifier.
 *
 * Stored settings use menu digits rather than wire identifiers. Archive
 * requires P2WP/7 because earlier revisions have no verified Archive source.
 *
 * @param[in,out] session Active session; requests advance its sequence.
 * @param[in] menu Source-menu digit from zero through three.
 * @param[out] source Selected wire source identifier.
 * @return One when the menu source is supported, otherwise zero.
 */
static uint8_t select_menu_source(p2wp_session_t *session, uint8_t menu,
                                  uint8_t *source) {
  if (menu == 1u) {
    *source = SOURCE_NOS;
    return 1u;
  }
  if (menu == 2u) {
    *source = SOURCE_P2000T;
    return 1u;
  }
  if (menu == 3u) {
    if (session->version < 7u) return 0u;
    *source = SOURCE_ARCHIVE;
    return 1u;
  }
  if (menu == 0u && session->version >= 4u) {
    international_channel = 0xffu;
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
 * @param[in,out] session Active session; requests advance its sequence.
 * @return One when a nonempty URL is accepted, or zero when cancelled.
 */
static uint8_t choose_custom_url(p2wp_session_t *session) {
  international_channel = 0xffu;
  uint8_t index;
  uint8_t key;
  uint8_t row = 9u;
  uint8_t column = 4u;

  load_custom_url(session);
  ui_custom_setup();
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

/** Fetch implementation shared by the viewer and the Pico-rendered picker. */
static uint8_t fetch_page(p2wp_session_t *session, viewer_state_t *state);

/**
 * @brief Chooses a channel from the Pico-rendered international catalogue.
 * @param[in,out] session Active session for catalogue fetches.
 * @param[in,out] state Viewer workspace shared with catalogue rendering.
 * @return One after selection, or zero after STOP cancellation or link failure.
 */
static uint8_t choose_international_channel(p2wp_session_t *session,
                                            viewer_state_t *state) {
  uint8_t key;
  uint8_t first;
  uint8_t group = 0u;
  state->source = SOURCE_INTERNATIONAL;
  state->page = 100u;
  international_channel = 0xffu;
  for (;;) {
    state->subpage = group;
    if (!fetch_page(session, state)) return 0u;
    key = platform_read_key();
    if (key == P2000T_KEY_STOP || key == EMULATOR_STOP_KEY) return 0u;
    key = platform_lower_ascii(platform_translate_key(key));
    first = group * 9u;
    if (key >= '1' && key <= '9' && first + key - '1' < 35u) {
      international_channel = first + key - '1';
      return 1u;
    }
    if ((key == 'n' || key == P2000T_KEY_RIGHT) && group < 3u) ++group;
    if ((key == 'p' || key == P2000T_KEY_LEFT) && group != 0u) --group;
  }
}

/**
 * @brief Shows the source menu or resolves a configured automatic source.
 * @param[in,out] session Active session; requests advance its sequence.
 * @param[in] use_autostart Whether the saved autostart source may be used.
 * @param[out] started_automatically Set when no menu interaction was needed.
 * @param[in,out] state Viewer workspace shared with channel selection.
 * @return Selected wire source identifier.
 */
static uint8_t choose_source(p2wp_session_t *session, uint8_t use_autostart,
                             uint8_t *started_automatically,
                             viewer_state_t *state) {
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
  ui_draw_logo(0u);
  ui_action(14u, "       KIES UW TELETEKSTBRON");
  platform_write_text(15u, 0u, "\0061\007  NOS TELETEKST");
  platform_write_text(16u, 0u, "\0062\007  P2000T TELETEKST");
  platform_write_text(17u, 0u,
                      session->version >= 7u
                          ? "\0063\007  TELETEKSTARCHIEF.NL"
                          : "\0063\007  ARCHIEF VEREIST P2WP/7");
  platform_write_text(18u, 0u, "\0064\007  PETSCIIPROXY.NL (P2WP/8)");
  platform_write_text(19u, 0u, "\0060\007  EIGEN SERVER");
  show_auto_start(session);
  platform_write_text(22u, 0u, "\006H\007 HULP");
  ui_footer();
  for (;;) {
    key = platform_read_ascii();
    key = platform_lower_ascii(key);
    if (key >= '1' && key <= '3' &&
        select_menu_source(session, key - '0', &source))
      return source;
    if (key == '4' && session->version >= 8u) {
      if (choose_international_channel(session, state))
        return SOURCE_INTERNATIONAL;
      goto draw_menu;
    }
    if (key == '0' && session->version >= 4u && choose_custom_url(session))
      return SOURCE_CUSTOM;
    if (key == 'a' && session->version >= 6u) {
      if (auto_start_source == AUTOSTART_DISABLED)
        auto_start_source = 1u;
      else if (auto_start_source == 0u)
        auto_start_source = AUTOSTART_DISABLED;
      else if (++auto_start_source == 3u && session->version < 7u)
        auto_start_source = 0u;
      else if (auto_start_source == 4u)
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
 * @param[in] version Negotiated P2WP version.
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
  uint8_t column;
  uint8_t graphics = 0u;
  platform_read_bytes(0u, 0u, indicator_saved, sizeof(indicator_saved));
  for (column = 0u; column != 4u; ++column) {
    uint8_t value = indicator_saved[column];
    if (value >= 1u && value <= 7u) graphics = 0u;
    if (value >= 0x11u && value <= 0x17u) graphics = 1u;
  }
  if (graphics) platform_clear_line(0u);
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
 * @brief Restores the screen row temporarily used by the fetch animation.
 */
static void indicator_restore(void) {
  platform_write_bytes(0u, 0u, indicator_saved, sizeof(indicator_saved));
}

/**
 * @brief Records a page-fetch error and removes the fetch animation.
 * @param[in,out] state Viewer state to update.
 * @param[in] error Cartridge or remote error code.
 * @return Always zero for direct use in failure returns.
 */
static uint8_t fail_fetch(viewer_state_t *state, uint8_t error) {
  state->error = error;
  indicator_restore();
  return 0u;
}

/**
 * @brief Converts a local or remote P2WP failure into viewer error state.
 * @param[in,out] state Viewer state to update.
 * @param[in] result P2WP transaction result.
 * @param[in] reply Response containing an optional remote error code.
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
 * @param[in] state Viewer state containing clock fields.
 * @param[out] out Destination text buffer.
 * @return Number of formatted display bytes.
 */
static uint8_t clock_text(const viewer_state_t *state, uint8_t *out) {
  return platform_format_clock((const uint8_t *)state, out);
}

/**
 * @brief Inserts valid provider clock metadata into the raw page header.
 * @param[in] state Viewer state containing clock fields.
 */
static void clock_overlay_raw(const viewer_state_t *state) {
  if (!state->clock_valid) return;
  page_screen[0] = 0x03u;
  (void)clock_text(state, page_screen + 1u);
}

/**
 * @brief Advances and redraws the live clock when its deadline expires.
 * @param[in,out] state Viewer state and clock metadata to update.
 * @param[in] input_active Whether page-number entry is using the right edge.
 */
static void clock_update(viewer_state_t *state, uint8_t input_active) {
  uint8_t text[20];
  uint8_t length;
  if (!platform_advance_clock((uint8_t *)state)) return;
  length = clock_text(state, text);
  if (!state->page_visible) {
    if (input_active) {
      uint8_t clock_has_date = state->clock_has_date;
      platform_write_text(0u, 21u, "               ");
      state->clock_has_date = 0u;
      length = clock_text(state, text);
      state->clock_has_date = clock_has_date;
      platform_write_bytes(0u, 27u, text, length);
      return;
    }
    platform_write_bytes(0u, (uint8_t)(40u - length), text, length);
    return;
  }
  platform_write_bytes(0u, 1u, text, length);
  {
    uint8_t index;
    for (index = 0u; index != length; ++index) {
      page_screen[1u + index] = text[index];
      display_screen[1u + index] = text[index];
    }
  }
}

/**
 * @brief Renders the raw page into the packed display buffer.
 * @param[in] state Viewer state selecting reveal mode.
 */
static void render_page(const viewer_state_t *state) {
  platform_render_page(page_screen, display_screen, state->reveal);
}

/**
 * @brief Renders and atomically presents the current page and mode markers.
 * @param[in] state Current viewer state.
 */
static void present_page(const viewer_state_t *state) {
  render_page(state);
  platform_present_screen(display_screen);
  if (state->auto_page) platform_write_text(0u, 35u, "V");
  if (state->rotation_paused) platform_write_text(0u, 39u, "A");
}

/**
 * @brief Fetches page metadata and four display chunks from the Pico.
 * @param[in,out] session Active session; requests advance its sequence.
 * @param[in,out] state Requested page state, updated with metadata and errors.
 * @return One after presenting a complete page, otherwise zero.
 */
static uint8_t fetch_page(p2wp_session_t *session, viewer_state_t *state) {
  uint8_t chunk;
  uint8_t request_length = 4u;
  uint8_t *destination = page_screen;
  uint16_t tries = 1875u;
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
  if (state->source == SOURCE_INTERNATIONAL) {
    fetch_request[4] = international_channel;
    request_length = 5u;
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
#if !CLOCK_OVERLAY_ENABLED
    state->clock_valid = 0u;
#elif !CLOCK_OVERLAY_INTERNATIONAL
    state->clock_valid &= state->source != SOURCE_INTERNATIONAL;
#endif
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
  if (session->version >= 4u) {
    uint16_t previous_page =
        (uint16_t)reply.payload[13] | ((uint16_t)reply.payload[14] << 8u);
    uint16_t next_page =
        (uint16_t)reply.payload[15] | ((uint16_t)reply.payload[16] << 8u);
    if (state->subpage == 0u || previous_page != 0u)
      state->previous_page = previous_page;
    if (state->subpage == 0u || next_page != 0u) state->next_page = next_page;
  } else {
    state->previous_page = 0u;
    state->next_page = 0u;
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
 * @param[in] state Viewer state containing page and error details.
 */
static void show_fetch_error(viewer_state_t *state) {
  const char *description;
  uint8_t index;
  state->page_visible = 0u;
  platform_clear_screen();
  ui_title(0u, " P2000T  TELETEKST VIA PICO W");
  ui_rule(1u);
  ui_rule(18u);
  ui_action(20u, "0-9: PAGINA   STOP: BRON   W: WIFI");
  ui_footer();
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
  if (state->error == 0x81u) {
    platform_write_text(5u, 9u, "TIMEOUT");
    ui_action(8u, "PROBEER OPNIEUW OF KIES ANDERE BRON");
    return;
  }
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
 * @param[in,out] session Active session; requests advance its sequence.
 * @param[in,out] state Viewer state to reset and update.
 * @param[in] page Page number from 100 through 899.
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
 * @param[in] state Viewer state containing the failed page and error.
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
 * @param[in,out] session Active session; requests advance its sequence.
 * @param[in,out] state Viewer state to reset and update.
 * @param[in] page Page number to request.
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
 * @param[in] state Viewer state controlling the pause marker.
 */
static void restore_header(const viewer_state_t *state) {
  platform_write_bytes(0u, 36u, page_screen + 36u, 4u);
  if (state->rotation_paused) platform_write_text(0u, 39u, "A");
}

/**
 * @brief Reads a one- or two-digit subpage and requests it.
 * @param[in,out] session Active session; requests advance its sequence.
 * @param[in,out] state Viewer state to update.
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
 * @brief Moves to the numerically previous or advertised next subpage.
 * @param[in,out] session Active session; requests advance its sequence.
 * @param[in,out] state Viewer state to update.
 * @param[in] forwards Nonzero for next; zero for previous.
 */
static void step_subpage(p2wp_session_t *session, viewer_state_t *state,
                         uint8_t forwards) {
  uint8_t target;
  if (forwards) {
    target = state->next_subpage;
    if (target == 0u && state->subpage == 0u) return;
  } else {
    if (state->subpage == 0u) return;
    target = state->subpage == 1u ? 0u : (uint8_t)(state->subpage - 1u);
  }
  state->subpage = target;
  state->rotation_paused = 1u;
  state->cycle_started = 0u;
  if (!fetch_page(session, state)) show_fetch_error(state);
}

/**
 * @brief Toggles automatic subpage rotation and its visible marker.
 * @param[in,out] state Viewer state to update.
 */
static void toggle_rotation(viewer_state_t *state) {
  state->rotation_paused ^= 1u;
  if (state->rotation_paused) {
    platform_write_text(0u, 39u, "A");
    return;
  }
  platform_write_bytes(0u, 39u, page_screen + 39u, 1u);
  if (state->auto_page || state->next_subpage != 0u || state->cycle_started)
    state->rotation_deadline = (uint16_t)(platform_clock() + 500u);
}

/**
 * @brief Toggles automatic page navigation and its visible marker.
 * @param[in,out] state Viewer state to update.
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
 * @param[in,out] session Active session; requests advance its sequence.
 * @param[in,out] state Viewer state to update.
 */
static void rotate_subpage(p2wp_session_t *session, viewer_state_t *state) {
  if (state->next_subpage != 0u && state->next_subpage > state->subpage) {
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
 * @param[in,out] session Active session; requests advance its sequence.
 * @param[in,out] state Viewer state to update.
 * @return One to reconfigure Wi-Fi, or zero to return to source selection.
 */
static uint8_t viewer_loop(p2wp_session_t *session, viewer_state_t *state) {
  uint8_t input[3];
  uint8_t input_count = 0u;
  uint8_t raw;
  uint8_t key;
  uint8_t status;

  for (;;) {
    clock_update(state, input_count != 0u);
    status = platform_key_status();
    if (status == 2u) return 0u;
    if (status == 0u) {
      if (input_count == 0u && !state->rotation_paused &&
          (state->auto_page || state->next_subpage != 0u ||
           state->cycle_started) &&
          (int16_t)(platform_clock() - state->rotation_deadline) >= 0) {
        if (state->next_subpage != 0u &&
            (!state->auto_page || state->next_subpage > state->subpage))
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
    if (key == 'a' || key == 'l') {
      input_count = 0u;
      toggle_rotation(state);
      continue;
    }
    if (key == 's') {
      input_count = 0u;
      select_subpage(session, state);
      continue;
    }
    if (key == '<') {
      input_count = 0u;
      step_subpage(session, state, 0u);
      continue;
    }
    if (key == '>') {
      input_count = 0u;
      step_subpage(session, state, 1u);
      continue;
    }
    if (key == 'r' || key == '?') {
      input_count = 0u;
      state->reveal ^= 1u;
      render_page(state);
      platform_commit_reveal(page_screen, state->reveal);
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

/* Public API contract: see teletekst.h. */
void teletekst_start(p2wp_session_t *session, uint8_t opening_timed_out) {
  viewer_state_t state;
  uint8_t started_automatically;
  load_settings(session);
  for (;;) {
    state.page = 100u;
    state.subpage = 0u;
    state.next_subpage = 0u;
    state.previous_page = 0u;
    state.next_page = 0u;
    state.rotation_paused = 0u;
    state.cycle_started = 0u;
    state.reveal = 0u;
    state.auto_page = 0u;
    state.auto_retry = 0u;
    state.source = choose_source(session, opening_timed_out,
                                 &started_automatically, &state);
    opening_timed_out = 0u;
    state.page = 100u;
    state.subpage = 0u;
    state.auto_page = started_automatically;
    if (!fetch_page(session, &state)) show_fetch_error(&state);
    if (viewer_loop(session, &state) != 0u) {
      (void)wifi_reconfigure(session);
    }
  }
}
