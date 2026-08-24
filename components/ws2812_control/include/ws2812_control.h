#pragma once
#include <stdint.h>

void ws2812_control_init(void);
void ws2812_control_set(bool on, uint8_t r, uint8_t g, uint8_t b);
void ws2812_control_get(bool *on, uint8_t *r, uint8_t *g, uint8_t *b);