#include "teletekst.h"
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
};

typedef struct {
  uint8_t source;
  uint16_t page;
  uint8_t subpage;
  uint8_t next_subpage;
  uint16_t previous_page;
  uint16_t next_page;
  uint16_t rotation_deadline;
  uint8_t rotation_paused;
  uint8_t cycle_started;
  uint8_t reveal;
  uint8_t zoom;
  uint8_t auto_page;
  uint8_t error;
} viewer_state_t;

static uint8_t page_screen[SCREEN_SIZE];
static uint8_t display_screen[SCREEN_SIZE];
static uint8_t fetch_request[5u + MAX_CUSTOM_URL];
static uint8_t custom_url[MAX_CUSTOM_URL];
static uint8_t custom_url_length;
static uint8_t indicator_saved[4];
static uint8_t indicator_phase;
static const uint8_t indicator_frames[6][4] = {
    {0x17u, 0x21u, 0x19u, 0x07u}, {0x17u, 0x22u, 0x19u, 0x07u},
    {0x17u, 0x28u, 0x19u, 0x07u}, {0x17u, 0x60u, 0x19u, 0x07u},
    {0x17u, 0x30u, 0x19u, 0x07u}, {0x17u, 0x24u, 0x19u, 0x07u},
};

static void save_display(void) {
  uint8_t row;
  for (row = 0u; row != P2000T_SCREEN_ROWS; ++row)
    platform_read_bytes(row, 0u, display_screen + (uint16_t)row * 40u, 40u);
}

static void show_help(void) {
  save_display();
  platform_clear_screen();
  ui_title(0u, " P2000T  HULP");
  ui_rule(1u);
  ui_panel(2u, "       BEDIENING VAN DE CARTRIDGE");
  ui_action(4u, " PAGINA EN VERBINDING");
  ui_panel(5u, " 100-899  TYP DRIE CIJFERS");
  ui_panel(6u, " START/I  INDEXPAGINA 100");
  ui_panel(7u, " <-/P ->/N VORIGE / VOLGENDE PAGINA");
  ui_panel(8u, " V        AUTO VOLGENDE PAGINA");
  ui_action(9u, " WEERGAVE");
  ui_panel(10u, " ?/R      VERBORGEN TEKST ONTHULLEN");
  ui_panel(11u, " Z        BOVEN / ONDER / NORMAAL");
  ui_action(12u, " SUBPAGINA'S");
  ui_panel(13u, " S        KIES EEN SUBPAGINA");
  ui_panel(14u, " A        SUBPAGINA PAUZE / DOOR");
  ui_panel(15u, " W        KIES EEN ANDER WIFI-NETWERK");
  ui_panel(16u, " STOP     ANDERE BRON / INVOER TERUG");
  ui_panel(17u, " H        DEZE HULPPAGINA");
  ui_rule(20u);
  ui_action(22u, "     DRUK EEN TOETS OM TERUG TE GAAN");
  (void)platform_read_key();
  platform_present_screen(display_screen);
}

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

static void save_custom_url(p2wp_session_t *session) {
  p2wp_response_t reply;
  uint8_t index;
  if (session->version < 5u)
    return;
  fetch_request[0] = custom_url_length;
  for (index = 0u; index != custom_url_length; ++index)
    fetch_request[1u + index] = custom_url[index];
  (void)p2wp_request(session, P2WP_TYPE_TELETEKST_CUSTOM_URL_SAVE,
                     fetch_request, (uint16_t)(custom_url_length + 1u), &reply);
}

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
    if (key == P2000T_KEY_STOP || key == EMULATOR_STOP_KEY)
      return 0u;
    key = platform_translate_key(key);
    if (key == 13u) {
      if (custom_url_length != 0u) {
        save_custom_url(session);
        return 1u;
      }
      continue;
    }
    if (key == 8u) {
      if (custom_url_length == 0u)
        continue;
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

static uint8_t choose_source(p2wp_session_t *session) {
  uint8_t key;
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
  ui_panel(11u, " A AUTOSTART NA 60S: UIT");
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
    if (key == '1')
      return SOURCE_NOS;
    if (key == '2')
      return SOURCE_P2000T;
    if (key == '3' && session->version >= 7u)
      return SOURCE_ARCHIVE;
    if (key == '0' && session->version >= 4u && choose_custom_url(session))
      return SOURCE_CUSTOM;
    if (key == 'h') {
      show_help();
      goto draw_menu;
    }
  }
}

static uint8_t expected_status_length(uint8_t version) {
  if (version >= 7u)
    return 21u;
  if (version >= 4u)
    return 17u;
  if (version == 3u)
    return 13u;
  return 5u;
}

static void indicator_draw(void) {
  platform_write_bytes(0u, 0u, indicator_frames[indicator_phase], 4u);
}

static void indicator_begin(void) {
  platform_read_bytes(0u, 0u, indicator_saved, sizeof(indicator_saved));
  indicator_phase = 0u;
  indicator_draw();
}

static void indicator_next(void) {
  ++indicator_phase;
  if (indicator_phase == 6u)
    indicator_phase = 0u;
  indicator_draw();
}

static void indicator_restore(void) {
  platform_write_bytes(0u, 0u, indicator_saved, sizeof(indicator_saved));
}

static uint8_t fail_fetch(viewer_state_t *state, uint8_t error) {
  state->error = error;
  indicator_restore();
  return 0u;
}

static uint8_t request_failure(viewer_state_t *state, enum p2wp_result result,
                               const p2wp_response_t *reply) {
  return fail_fetch(state, result == P2WP_REMOTE_ERROR
                               ? reply->remote_error
                               : (uint8_t)(0x80u + result));
}

static uint8_t render_byte(uint8_t value, uint8_t *colour, uint8_t reveal) {
  uint8_t control = (uint8_t)(value & 0x7fu);
  if ((control >= 1u && control <= 7u) ||
      (control >= 0x11u && control <= 0x17u))
    *colour = control;
  if (control == 0x18u && reveal)
    return *colour;
  return value;
}

static void render_page(const viewer_state_t *state) {
  uint8_t row;
  uint8_t column;
  uint8_t colour;
  uint16_t source;
  uint16_t destination;

  if (state->zoom == 0u) {
    for (row = 0u; row != 24u; ++row) {
      colour = 0x07u;
      source = (uint16_t)row * 40u;
      for (column = 0u; column != 40u; ++column)
        display_screen[source + column] =
            render_byte(page_screen[source + column], &colour, state->reveal);
    }
    return;
  }
  for (source = 0u; source != SCREEN_SIZE; ++source)
    display_screen[source] = ' ';
  source = state->zoom == 2u ? 480u : 0u;
  destination = 0u;
  for (row = 0u; row != 12u; ++row) {
    display_screen[destination] = 0x0du;
    colour = 0x07u;
    for (column = 0u; column != 39u; ++column)
      display_screen[destination + 1u + column] =
          render_byte(page_screen[source + column], &colour, state->reveal);
    source += 40u;
    destination += 80u;
  }
}

static void present_page(const viewer_state_t *state) {
  render_page(state);
  platform_present_screen(display_screen);
  if (state->auto_page)
    platform_write_text(0u, 35u, "V");
  if (state->rotation_paused)
    platform_write_text(0u, 39u, "A");
}

static uint8_t fetch_page(p2wp_session_t *session, viewer_state_t *state) {
  uint8_t chunk;
  uint8_t request_length = 4u;
  uint8_t *destination = page_screen;
  uint16_t tries = 750u;
  p2wp_response_t reply;
  enum p2wp_result result;

  state->error = 0u;
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
  if (result != P2WP_OK)
    return request_failure(state, result, &reply);
  if (reply.payload_length != 0u)
    return fail_fetch(state, 0x82u);

  while (tries-- != 0u) {
    platform_wait_ticks(2u);
    indicator_next();
    result =
        p2wp_request(session, P2WP_TYPE_TELETEKST_FETCH_STATUS, 0, 0u, &reply);
    if (result != P2WP_OK)
      return request_failure(state, result, &reply);
    if (reply.payload_length != expected_status_length(session->version))
      return fail_fetch(state, 0x82u);
    if (reply.payload[0] == FETCH_COMPLETE)
      break;
    if (reply.payload[0] == FETCH_FAILED)
      return fail_fetch(state, reply.payload[1]);
    if (reply.payload[0] != FETCH_CONNECTING &&
        reply.payload[0] != FETCH_RECEIVING)
      return fail_fetch(state, 0x82u);
  }
  if (reply.payload[0] != FETCH_COMPLETE)
    return fail_fetch(state, 0x81u);

  state->next_subpage = reply.payload[4];
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
    if (result != P2WP_OK)
      return request_failure(state, result, &reply);
    if (reply.payload_length != CHUNK_SIZE)
      return fail_fetch(state, 0x82u);
    {
      uint16_t index;
      for (index = 0u; index != CHUNK_SIZE; ++index)
        *destination++ = reply.payload[index];
    }
  }
  state->reveal = 0u;
  state->zoom = 0u;
  present_page(state);
  if (state->next_subpage != 0u)
    state->cycle_started = 1u;
  if (state->auto_page || (!state->rotation_paused &&
                           (state->next_subpage != 0u || state->cycle_started)))
    state->rotation_deadline = (uint16_t)(platform_clock() + 500u);
  return 1u;
}

static void show_fetch_error(const viewer_state_t *state) {
  ui_panel(7u, " PAGINA KON NIET WORDEN WEERGEGEVEN");
  ui_panel(9u, " FOUTCODE:");
  platform_write_u8(9u, 14u, state->error);
  ui_action(20u, " TYP PAGINA, I INDEX OF STOP BRONKEUZE");
}

static void clear_page_input(void) {
  static const uint8_t spaces[4] = {' ', ' ', ' ', ' '};
  platform_write_bytes(0u, 36u, spaces, sizeof(spaces));
}

static void request_page(p2wp_session_t *session, viewer_state_t *state,
                         uint16_t page) {
  state->page = page;
  state->subpage = 0u;
  state->rotation_paused = 0u;
  state->cycle_started = 0u;
  if (!fetch_page(session, state))
    show_fetch_error(state);
}

static void restore_header(const viewer_state_t *state) {
  platform_write_bytes(0u, 36u, page_screen + 36u, 4u);
  if (state->rotation_paused)
    platform_write_text(0u, 39u, "A");
}

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
  if (!fetch_page(session, state))
    show_fetch_error(state);
}

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

static void toggle_auto_page(viewer_state_t *state) {
  state->auto_page ^= 1u;
  if (state->auto_page) {
    platform_write_text(0u, 35u, "V");
    state->rotation_deadline = (uint16_t)(platform_clock() + 500u);
  } else {
    platform_write_bytes(0u, 35u, display_screen + 35u, 1u);
  }
}

static void rotate_subpage(p2wp_session_t *session, viewer_state_t *state) {
  if (state->next_subpage != 0u) {
    state->subpage = state->next_subpage;
  } else {
    state->subpage = 0u;
    state->cycle_started = 0u;
  }
  if (!fetch_page(session, state))
    show_fetch_error(state);
}

static uint8_t viewer_loop(p2wp_session_t *session, viewer_state_t *state) {
  uint8_t input[3];
  uint8_t input_count = 0u;
  uint8_t raw;
  uint8_t key;
  uint8_t status;

  for (;;) {
    status = platform_key_status();
    if (status == 2u)
      return 0u;
    if (status == 0u) {
      if (input_count == 0u &&
          (state->auto_page ||
           (!state->rotation_paused &&
            (state->next_subpage != 0u || state->cycle_started))) &&
          (int16_t)(platform_clock() - state->rotation_deadline) >= 0) {
        if (!state->rotation_paused && state->next_subpage != 0u)
          rotate_subpage(session, state);
        else if (state->auto_page)
          request_page(session, state,
                       state->next_page != 0u ? state->next_page : 100u);
        else
          rotate_subpage(session, state);
      }
      continue;
    }
    raw = platform_read_key();
    if (raw == P2000T_KEY_STOP || raw == EMULATOR_STOP_KEY)
      return 0u;
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
      present_page(state);
      continue;
    }
    if (key == 'z') {
      input_count = 0u;
      ++state->zoom;
      if (state->zoom == 3u)
        state->zoom = 0u;
      present_page(state);
      continue;
    }
    if (key == 'h') {
      input_count = 0u;
      show_help();
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
    if (key == 'w')
      return 1u;
    if (key == 8u) {
      if (input_count != 0u) {
        --input_count;
        platform_write_text(0u, (uint8_t)(36u + input_count), " ");
      }
      continue;
    }
    if (key < '0' || key > '9')
      continue;
    if (input_count == 0u) {
      if (key < '1' || key > '8')
        continue;
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

uint8_t teletekst_start(p2wp_session_t *session) {
  viewer_state_t state;
  for (;;) {
    state.source = choose_source(session);
    state.page = 100u;
    state.subpage = 0u;
    state.next_subpage = 0u;
    state.previous_page = 0u;
    state.next_page = 0u;
    state.rotation_paused = 0u;
    state.cycle_started = 0u;
    state.reveal = 0u;
    state.zoom = 0u;
    state.auto_page = 0u;
    if (!fetch_page(session, &state))
      show_fetch_error(&state);
    if (viewer_loop(session, &state) != 0u) {
      (void)wifi_reconfigure(session);
    }
  }
  return 1u;
}
