#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"

#define SSD1306_WIDTH  128
#define SSD1306_HEIGHT 64

esp_err_t ssd1306_init(void);
void ssd1306_clear(void);
void ssd1306_set_pixel(int x, int y, bool on);
void ssd1306_draw_number_centered(uint32_t value, int scale);
esp_err_t ssd1306_flush(void);
