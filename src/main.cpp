// ESP32-4848S040 Touch Display with LVGL and Relay Control

#include <Arduino.h>
#include <Arduino_GFX_Library.h>
#include <ESP32_4848S040.h>
#include <lvgl.h>
#include "touch.h"
#include <WiFi.h>
#include <time.h>
#include <lv_conf.h>

//wifi connection info
const char* ssid = "Fake ssid";         
const char* password = "fake password"; 


// NTP Server Settings
const char* ntpServer = "pool.ntp.org";
// Time zone offsets in seconds (e.g., Eastern Time: -5 hours * 3600 = -18000)
const long  gmtOffset_sec = -18000; 
const int   daylightOffset_sec = 3600; // 1 hour for Daylight Saving Time
int hour, minute, second;
int hour_color, minute_color, second_color;
String hour_str, minute_str, second_str, time_str;
lv_color_t text_color = lv_color_hex(0x810226);

// Display backlight pin
#define GFX_BL 38

// Display objects
Arduino_ESP32SPI *bus;
Arduino_RGB_Display *gfx;

// Display dimensions (unused legacy variables)
int16_t w, h, text_size, banner_height, graph_baseline, graph_height, channel_width, signal_width;

// LVGL buffer definitions
#define BYTE_PER_PIXEL (LV_COLOR_FORMAT_GET_SIZE(LV_COLOR_FORMAT_RGB565))  // Will be 2 for RGB565
static uint8_t buf1[480 * 480 / 10 * BYTE_PER_PIXEL];
lv_display_t *display;

// LVGL display configuration
#define TFT_HOR_RES   480
#define TFT_VER_RES   480
#define TFT_ROTATION  LV_DISPLAY_ROTATION_0
#define TFT_BRIGHTNESS 255  // Backlight brightness (0-255)

// LVGL draw buffer: 1/10 screen size usually works well (size is in bytes)
#define DRAW_BUF_SIZE (TFT_HOR_RES * TFT_VER_RES / 10 * (LV_COLOR_DEPTH / 8))
uint32_t draw_buf[DRAW_BUF_SIZE / 4];

// Relay GPIO configuration
#define GPIO_RELAY1  40
#define GPIO_RELAY2  2
#define GPIO_RELAY3  1
/*
Conceptual layout of hex_clock implementation
1. code to connect to wifi
2. code to reach out to ntp server
3. code to retrieve time
4. code to convert time aspects to strings as needed
5. build string for hex color ("0xhrminsec" basically)
6. change color of display based on hex value
7. overlay time in text - color will likely be 0x810226 (just to avoid time being the same as the background)

questions - 
- how often should i reach out to the ntp server for a time refresh?
- how often should i update the display?  since i'm doing it locally, it's not like i'm going to be hammering some server, etc.*/


// Display flushing callback for LVGL
void my_disp_flush(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map)
{
  uint32_t w = (area->x2 - area->x1 + 1);
  uint32_t h = (area->y2 - area->y1 + 1);

#if (LV_COLOR_16_SWAP != 0)
  gfx->draw16bitBeRGBBitmap(area->x1, area->y1, (uint16_t *)px_map, w, h);
#else
  gfx->draw16bitRGBBitmap(area->x1, area->y1, (uint16_t *)px_map, w, h);
#endif

  lv_disp_flush_ready(disp);
}


// Tick callback for LVGL
static uint32_t my_tick(void)
{
  return millis();
}

void setup()
{
    Serial.begin(115200);
    delay(1000); // Give the serial monitor time to connect
    touch_init();

    // Initialize 9-bit mode SPI
    bus = new Arduino_ESP32SPI(
        GFX_NOT_DEFINED /* DC */, 39 /* CS */, 48 /* SCK */, 47 /* MOSI */, GFX_NOT_DEFINED /* MISO */);

  // Initialize RGB panel (hardware specific)
    Arduino_ESP32RGBPanel *rgbpanel = new Arduino_ESP32RGBPanel(
        18 /* DE */, 17 /* VSYNC */, 16 /* HSYNC */, 21 /* PCLK */,
    #if 0
        4 /* R0 */, 5 /* R1 */, 6 /* R2 */, 7 /* R3 */, 15 /* R4 */,
        8 /* G0 */, 20 /* G1 */, 3 /* G2 */, 46 /* G3 */, 9 /* G4 */, 10 /* G5 */,
        11 /* B0 */, 12 /* B1 */, 13 /* B2 */, 14 /* B3 */, 0 /* B4 */,
    #else
        11 /* R0 */, 12 /* R1 */, 13 /* R2 */, 14 /* R3 */, 0 /* R4 */,
        8 /* G0 */, 20 /* G1 */, 3 /* G2 */, 46 /* G3 */, 9 /* G4 */, 10 /* G5 */,
        4 /* B0 */, 5 /* B1 */, 6 /* B2 */, 7 /* B3 */, 15 /* B4 */,
    #endif
        1 /* hsync_polarity */, 10 /* hsync_front_porch */, 8 /* hsync_pulse_width */, 50 /* hsync_back_porch */,
        1 /* vsync_polarity */, 10 /* vsync_front_porch */, 8 /* vsync_pulse_width */, 20 /* vsync_back_porch */);

  // Initialize display panel
    gfx = new Arduino_RGB_Display(
        480 /* width */, 480 /* height */, rgbpanel, 0 /* rotation */, true /* auto_flush */,
        bus, GFX_NOT_DEFINED /* RST */, st7701_4848s040_init_operations, sizeof(st7701_4848s040_init_operations));


  // Check if display initialization succeeded
    if (!gfx->begin())
    {
        Serial.println("gfx->begin() failed!");
    }

    // Turn on backlight
    #ifdef GFX_BL
        pinMode(GFX_BL, OUTPUT);
        analogWrite(GFX_BL, TFT_BRIGHTNESS);
    #endif

    //turn the display itself on
    gfx->displayOn();


    // Initialize LVGL
    lv_init();

    // Set tick source for LVGL timing
    lv_tick_set_cb(my_tick);

    // Create LVGL display
    display = lv_display_create(TFT_HOR_RES, TFT_VER_RES);
    lv_display_set_flush_cb(display, my_disp_flush);
    lv_display_set_buffers(display, draw_buf, NULL, sizeof(draw_buf), LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_display_set_rotation(display, TFT_ROTATION);



    Serial.println("\nConnecting to Wi-Fi...");
    WiFi.begin(ssid, password);

    // Wait until connected
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }

    Serial.println("\nWi-Fi Connected!");
    Serial.print("IP Address: ");
    Serial.println(WiFi.localIP());

    // Initialize NTP
    configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
    Serial.println("NTP Initialized!");
    struct tm timeinfo;
    if(!getLocalTime(&timeinfo))
    {
        Serial.println("Failed to obtain time");
        return;
    }
    Serial.println("Time obtained successfully!");
    hour = timeinfo.tm_hour;
    minute = timeinfo.tm_min;
    second = timeinfo.tm_sec;

    hour_color = hour * 256 / 24; // Scale hour to 0-255
    minute_color = minute * 256 / 60; // Scale minute to 0-255
    second_color = second * 256 / 60; // Scale second to 0-255
    Serial.println("Got RGB");
    lv_color_t bg_color = lv_color_make(hour_color, minute_color, second_color);
    lv_obj_set_style_bg_color(lv_screen_active(), bg_color, LV_PART_MAIN);

    lv_obj_t *label = lv_label_create(lv_screen_active());

    if (hour < 10)
        hour_str = "0" + String(hour);
    else
        hour_str = String(hour);

    if (minute < 10)
        minute_str = "0" + String(minute);
    else
        minute_str = String(minute);

    if (second < 10)
        second_str = "0" + String(second);
    else
        second_str = String(second);

    time_str = hour_str + ":" + minute_str + ":" + second_str;

    lv_label_set_text(label, time_str.c_str());
    lv_obj_set_style_text_color(label, text_color, LV_PART_MAIN);
    lv_obj_set_style_text_font(label, &lv_font_montserrat_24, LV_PART_MAIN);
    lv_obj_align(label, LV_ALIGN_CENTER, 90, 90);


    Serial.println("Background color set based on time!");
    Serial.println("RGB Calculated Successfully");
}

void loop()
{
    lv_timer_handler();
    delay(5);
}