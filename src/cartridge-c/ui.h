#ifndef P2000T_UI_H
#define P2000T_UI_H

#include <stdint.h>

void ui_opening_screen(void);
void ui_title(uint8_t row, const char *text);
void ui_panel(uint8_t row, const char *text);
void ui_action(uint8_t row, const char *text);
void ui_rule(uint8_t row);
void ui_footer(void);

#endif
