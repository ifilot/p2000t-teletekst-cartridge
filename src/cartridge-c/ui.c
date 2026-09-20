#include "ui.h"
#include "platform.h"

enum {
  SAA_ALPHA_BLUE = 0x04,
  SAA_ALPHA_WHITE = 0x07,
  SAA_GRAPHICS_BLUE = 0x14,
  SAA_GRAPHICS_WHITE = 0x17,
  SAA_NEW_BACKGROUND = 0x1d,
  SAA_CONTIGUOUS_GRAPHICS = 0x19,
};

static void styled_line(uint8_t row, uint8_t foreground, uint8_t text_colour,
                        const char *text) {
  uint8_t prefix[3];
  platform_clear_line(row);
  prefix[0] = foreground;
  prefix[1] = SAA_NEW_BACKGROUND;
  prefix[2] = text_colour;
  platform_write_bytes(row, 0u, prefix, sizeof(prefix));
  platform_write_text(row, 3u, text);
}

void ui_title(uint8_t row, const char *text) {
  styled_line(row, SAA_ALPHA_BLUE, SAA_ALPHA_WHITE, text);
}

void ui_panel(uint8_t row, const char *text) {
  styled_line(row, SAA_ALPHA_WHITE, SAA_ALPHA_BLUE, text);
}

void ui_action(uint8_t row, const char *text) {
  styled_line(row, SAA_ALPHA_BLUE, SAA_ALPHA_WHITE, text);
}

void ui_rule(uint8_t row) {
  static const uint8_t graphics_blue[] = {SAA_GRAPHICS_BLUE};
  static const uint8_t mosaic_rule[] = {0x73};
  uint8_t column;
  platform_clear_line(row);
  platform_write_bytes(row, 0u, graphics_blue, 1u);
  for (column = 1u; column != P2000T_SCREEN_COLUMNS; ++column)
    platform_write_bytes(row, column, mosaic_rule, 1u);
}

void ui_footer(void) { ui_title(23u, "P2000T Teletekst Cartridge     v0.5.0"); }

void ui_opening_screen(void) {
  static const uint8_t badge[] = {
      SAA_ALPHA_BLUE,
      SAA_NEW_BACKGROUND,
      SAA_GRAPHICS_WHITE,
      SAA_CONTIGUOUS_GRAPHICS,
      0x7f,
      0x7f,
      0x70,
      0x7c,
      0x6c,
      0x6c,
      0x7c,
      0x70,
      0x7c,
      0x6c,
      0x6c,
      0x7c,
      0x70,
      0x7f,
      0x7f,
  };
  platform_clear_screen();
  ui_title(0u, "                                      ");
  ui_title(1u, "                                      ");
  ui_title(2u, "    P2000T  INTERNET TELETEKST");
  platform_write_bytes(6u, 10u, badge, sizeof(badge));
  ui_panel(16u, "     UW VENSTER OP DE WERELD");
  ui_panel(17u, "     NOS EN P2000T TELETEKST");
  ui_action(18u, "  ORIGINEEL SAA5050-MOZAIEKBEELD");
  ui_action(19u, "  KLASSIEK BEELD, ACTUEEL NIEUWS");
  ui_rule(20u);
  ui_panel(22u, " DRUK OP EEN TOETS");
  ui_footer();
}
