#include "ssd1306.h"

#include <stdio.h>
#include <string.h>

#include "driver/gpio.h"
#include "driver/i2c_master.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_err.h"
#include "esp_log.h"

#define OLED_ADDR     0x3C
#define OLED_SDA_GPIO 17
#define OLED_SCL_GPIO 18
#define OLED_RST_GPIO 21
#define VEXT_GPIO     36
#define I2C_FREQ_HZ   400000

#define FB_SIZE       (SSD1306_WIDTH * SSD1306_HEIGHT / 8)

static const char *TAG = "ssd1306";

static i2c_master_bus_handle_t s_bus;
static i2c_master_dev_handle_t s_dev;
static uint8_t s_framebuffer[FB_SIZE];

/* Standard 5x7 font for digits 0-9. Each digit = 5 columns, 1 byte per column,
   LSB = top pixel. */
static const uint8_t font5x7_digits[10][5] = {
    {0x3E, 0x51, 0x49, 0x45, 0x3E},
    {0x00, 0x42, 0x7F, 0x40, 0x00},
    {0x42, 0x61, 0x51, 0x49, 0x46},
    {0x21, 0x41, 0x45, 0x4B, 0x31},
    {0x18, 0x14, 0x12, 0x7F, 0x10},
    {0x27, 0x45, 0x45, 0x45, 0x39},
    {0x3C, 0x4A, 0x49, 0x49, 0x30},
    {0x01, 0x71, 0x09, 0x05, 0x03},
    {0x36, 0x49, 0x49, 0x49, 0x36},
    {0x06, 0x49, 0x49, 0x29, 0x1E},
};

static esp_err_t write_cmd(uint8_t cmd)
{
    uint8_t buf[2] = {0x00, cmd};
    return i2c_master_transmit(s_dev, buf, sizeof(buf), 100);
}

static esp_err_t write_cmd_list(const uint8_t *cmds, size_t n)
{
    for (size_t i = 0; i < n; i++) {
        esp_err_t err = write_cmd(cmds[i]);
        if (err != ESP_OK) {
            return err;
        }
    }
    return ESP_OK;
}

void ssd1306_clear(void)
{
    memset(s_framebuffer, 0, sizeof(s_framebuffer));
}

void ssd1306_set_pixel(int x, int y, bool on)
{
    if (x < 0 || x >= SSD1306_WIDTH || y < 0 || y >= SSD1306_HEIGHT) {
        return;
    }
    int idx = (y / 8) * SSD1306_WIDTH + x;
    uint8_t mask = (uint8_t)(1U << (y % 8));
    if (on) {
        s_framebuffer[idx] |= mask;
    } else {
        s_framebuffer[idx] &= (uint8_t)~mask;
    }
}

static void draw_digit(int digit, int x0, int y0, int scale)
{
    if (digit < 0 || digit > 9) {
        return;
    }
    for (int col = 0; col < 5; col++) {
        uint8_t bits = font5x7_digits[digit][col];
        for (int row = 0; row < 7; row++) {
            if (!(bits & (1U << row))) {
                continue;
            }
            for (int dx = 0; dx < scale; dx++) {
                for (int dy = 0; dy < scale; dy++) {
                    ssd1306_set_pixel(x0 + col * scale + dx, y0 + row * scale + dy, true);
                }
            }
        }
    }
}

void ssd1306_draw_number_centered(uint32_t value, int scale)
{
    char buf[16];
    int len = snprintf(buf, sizeof(buf), "%u", (unsigned)value);

    const int digit_w = 5 * scale;
    const int gap     = scale;
    const int total_w = len * digit_w + (len - 1) * gap;
    const int digit_h = 7 * scale;

    int x = (SSD1306_WIDTH - total_w) / 2;
    int y = (SSD1306_HEIGHT - digit_h) / 2;

    for (int i = 0; i < len; i++) {
        draw_digit(buf[i] - '0', x + i * (digit_w + gap), y, scale);
    }
}

esp_err_t ssd1306_flush(void)
{
    const uint8_t addr_cmds[] = {
        0x21, 0, SSD1306_WIDTH - 1,         /* column range */
        0x22, 0, (SSD1306_HEIGHT / 8) - 1,  /* page range */
    };
    esp_err_t err = write_cmd_list(addr_cmds, sizeof(addr_cmds));
    if (err != ESP_OK) {
        return err;
    }

    uint8_t chunk[1 + SSD1306_WIDTH];
    chunk[0] = 0x40; /* data mode */
    for (int page = 0; page < SSD1306_HEIGHT / 8; page++) {
        memcpy(&chunk[1], &s_framebuffer[page * SSD1306_WIDTH], SSD1306_WIDTH);
        err = i2c_master_transmit(s_dev, chunk, sizeof(chunk), 100);
        if (err != ESP_OK) {
            return err;
        }
    }
    return ESP_OK;
}

esp_err_t ssd1306_init(void)
{
    /* Enable Vext (active LOW) — powers the OLED rail on Heltec V3 */
    gpio_reset_pin(VEXT_GPIO);
    gpio_set_direction(VEXT_GPIO, GPIO_MODE_OUTPUT);
    gpio_set_level(VEXT_GPIO, 0);

    /* Pulse OLED reset */
    gpio_reset_pin(OLED_RST_GPIO);
    gpio_set_direction(OLED_RST_GPIO, GPIO_MODE_OUTPUT);
    gpio_set_level(OLED_RST_GPIO, 0);
    vTaskDelay(pdMS_TO_TICKS(20));
    gpio_set_level(OLED_RST_GPIO, 1);
    vTaskDelay(pdMS_TO_TICKS(20));

    i2c_master_bus_config_t bus_cfg = {
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .i2c_port = I2C_NUM_0,
        .sda_io_num = OLED_SDA_GPIO,
        .scl_io_num = OLED_SCL_GPIO,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };
    ESP_ERROR_CHECK(i2c_new_master_bus(&bus_cfg, &s_bus));

    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = OLED_ADDR,
        .scl_speed_hz = I2C_FREQ_HZ,
    };
    ESP_ERROR_CHECK(i2c_master_bus_add_device(s_bus, &dev_cfg, &s_dev));

    const uint8_t init_seq[] = {
        0xAE,             /* display off */
        0xD5, 0x80,       /* clock divide ratio / osc freq */
        0xA8, 0x3F,       /* multiplex ratio = 64 */
        0xD3, 0x00,       /* display offset = 0 */
        0x40,             /* start line = 0 */
        0x8D, 0x14,       /* charge pump on */
        0x20, 0x00,       /* horizontal addressing mode */
        0xA1,             /* segment remap (flip horizontally) */
        0xC8,             /* COM scan direction reversed */
        0xDA, 0x12,       /* COM pins: alternative, no remap */
        0x81, 0xCF,       /* contrast */
        0xD9, 0xF1,       /* pre-charge */
        0xDB, 0x40,       /* VCOMH deselect level */
        0xA4,             /* output follows RAM */
        0xA6,             /* normal (non-inverted) */
        0xAF,             /* display on */
    };
    esp_err_t err = write_cmd_list(init_seq, sizeof(init_seq));
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "init sequence failed: %s", esp_err_to_name(err));
        return err;
    }

    ssd1306_clear();
    return ssd1306_flush();
}
