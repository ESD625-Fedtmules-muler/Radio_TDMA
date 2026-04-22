#ifndef ESP32C3_PINS_H
#define ESP32C3_PINS_H

#include <Arduino.h>


#define PIN_RF_CSN      18
#define PIN_RF_MOSI     3
#define PIN_RF_MISO     0
#define PIN_RF_SCK      19
#define PIN_RF_IRQ      1
#define PIN_RF_CE       10 //Patch such that LR1121 BUSY Pin is connected to R42.
#define PIN_GPS_PPS     6
#define PIN_GPS_TX      4
#define PIN_GPS_RX      5
#define PIN_I2C_SDA     7
#define PIN_I2C_SCL     8



//!Wtf dude
/**
 * ESP32-C3 GPIO Mapping
 * Reference: Table 2-4 Pin Definitions
 * Note: Use GPIO numbers, not physical pin numbers, in your code.
 * 
 * https://documentation.espressif.com/esp32-c3_datasheet_en.pdf
 * 
 */

// Mapping Physical Pins to GPIOs from your image:
#define GPIO_0   0   // Physical Pin 11
#define GPIO_1   1   // Physical Pin 12
#define GPIO_2   2   // Physical Pin 13 (Yellow - Strapping)
#define GPIO_3   3   // Physical Pin 14
#define GPIO_4   4   // Physical Pin 15
#define GPIO_5   5   // Physical Pin 16
#define GPIO_6   6   // Physical Pin 17
#define GPIO_7   7   // Physical Pin 18
#define GPIO_8   8   // Physical Pin 19 (Yellow - Strapping)
#define GPIO_9   9   // Physical Pin 20 (Yellow - Strapping)
#define GPIO_10  10  // Physical Pin 21
#define GPIO_11  11  // Physical Pin 22 (Red - SPI)
#define GPIO_12  12  // Physical Pin 23 (Red - SPI)
#define GPIO_13  13  // Physical Pin 24 (Red - SPI)
#define GPIO_14  14  // Physical Pin 25 (Red - SPI)
#define GPIO_15  15  // Physical Pin 26 (Red - SPI)
#define GPIO_16  16  // Physical Pin 27 (Red - SPI)
#define GPIO_17  17  // Physical Pin 28 (Red - SPI)
#define GPIO_18  18  // FYFY pin (JTAG)
#define GPIO_19  19  // FYFY pin (JTAG)
#define GPIO_20  20  // Physical Pin 29 (UART0 RX - usually follows 21)
#define GPIO_21  21  // Physical Pin 28 (UART0 TX)

// --- Custom Aliases (Add yours here) ---
#define ESP_TX GPIO_21
#define ESP_RX GPIO_20

#endif