// ESP32-4848S040 Touch Display with LVGL and Relay Control

#include <Arduino.h>
#include <Arduino_GFX_Library.h>
#include <ESP32_4848S040.h>
#include <lvgl.h>
#include "touch.h"
#include <WiFi.h>

const char* ssid = "FakeSSID";         
const char* password = "FakePassword"; 

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


void setup()
{
    Serial.begin(115200);
    delay(1000); // Give the serial monitor time to connect

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
}

void loop()
{
}