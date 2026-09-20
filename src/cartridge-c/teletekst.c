#include "teletekst.h"
#include "platform.h"
#include "ui.h"

enum {
  FETCH_CONNECTING = 1,
  FETCH_RECEIVING = 2,
  FETCH_COMPLETE = 3,
  FETCH_FAILED = 4,
  SOURCE_NOS = 0,
  SOURCE_P2000T = 1,
  SOURCE_ARCHIVE = 3,
  SCREEN_SIZE = 960,
  CHUNK_SIZE = 240,
  CHUNK_COUNT = 4,
};

static uint8_t page_screen[SCREEN_SIZE];

static void wait_ticks(uint8_t ticks) {
  uint16_t end = platform_clock() + ticks;
  while ((int16_t)(platform_clock() - end) < 0) {
  }
}

static uint8_t choose_source(const p2wp_session_t *session) {
  uint8_t key;
  platform_clear_screen();
  ui_title(0u, "P2000T TELETEKST           BRONKEUZE");
  ui_rule(1u);
  ui_action(3u, "      KIES UW TELETEKSTBRON");
  ui_panel(4u, "  1 - NOS TELETEKST");
  ui_panel(5u, "  2 - P2000T TELETEKST");
  ui_panel(6u, "  3 - TELETEKSTARCHIEF.NL");
  ui_rule(8u);
  ui_action(10u, "        KIES BRON (1-3)");
  ui_panel(13u, " EERSTE C-VERSIE VAN PAGINAWEERGAVE");
  if (session->version < 7u)
    ui_panel(15u, " ARCHIEF VEREIST VOORLOPIG P2WP/7");
  ui_footer();
  for (;;) {
    key = platform_read_ascii();
    if (key == '1')
      return SOURCE_NOS;
    if (key == '2')
      return SOURCE_P2000T;
    if (key == '3' && session->version >= 7u)
      return SOURCE_ARCHIVE;
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

static uint8_t fetch_page(p2wp_session_t *session, uint8_t source,
                          uint16_t page) {
  uint8_t request[4];
  uint8_t chunk;
  uint16_t tries = 750u;
  p2wp_response_t reply;
  enum p2wp_result result;

  request[0] = (uint8_t)page;
  request[1] = (uint8_t)(page >> 8u);
  request[2] = 0u;
  request[3] = source;
  result = p2wp_request(session, P2WP_TYPE_TELETEKST_FETCH_START, request,
                        sizeof(request), &reply);
  if (result != P2WP_OK || reply.payload_length != 0u)
    return 0u;

  platform_clear_screen();
  ui_title(0u, "P2000T TELETEKST          PAGINA 100");
  ui_rule(1u);
  ui_panel(5u, " PAGINA WORDT OPGEHAALD...");
  while (tries-- != 0u) {
    wait_ticks(2u);
    result =
        p2wp_request(session, P2WP_TYPE_TELETEKST_FETCH_STATUS, 0, 0u, &reply);
    if (result != P2WP_OK ||
        reply.payload_length != expected_status_length(session->version))
      return 0u;
    if (reply.payload[0] == FETCH_COMPLETE)
      break;
    if (reply.payload[0] == FETCH_FAILED ||
        (reply.payload[0] != FETCH_CONNECTING &&
         reply.payload[0] != FETCH_RECEIVING)) {
      ui_panel(8u, " OPHALEN MISLUKT, FOUTCODE:");
      platform_write_u8(8u, 33u, reply.payload[1]);
      return 0u;
    }
  }
  if (reply.payload[0] != FETCH_COMPLETE)
    return 0u;

  for (chunk = 0u; chunk != CHUNK_COUNT; ++chunk) {
    result = p2wp_request(session, P2WP_TYPE_TELETEKST_FETCH_ROWS, &chunk, 1u,
                          &reply);
    if (result != P2WP_OK || reply.payload_length != CHUNK_SIZE)
      return 0u;
    {
      uint16_t index;
      uint8_t *destination = page_screen + (uint16_t)chunk * CHUNK_SIZE;
      for (index = 0u; index != CHUNK_SIZE; ++index)
        destination[index] = reply.payload[index];
    }
  }
  platform_present_screen(page_screen);
  return 1u;
}

uint8_t teletekst_start(p2wp_session_t *session) {
  uint8_t source = choose_source(session);
  if (!fetch_page(session, source, 100u)) {
    ui_action(20u, " PAGINA KON NIET WORDEN WEERGEGEVEN");
    return 0u;
  }
  return 1u;
}
