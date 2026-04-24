#include <Arduino.h>


//* Pins for RF modems SPI
#ifdef DUMMY_RADIO

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

#endif

#ifndef DUMMY_RADIO //If we use the real thing
    //* Pins for user interface SPI
    #define PIN_UI_MOSI     5
    #define PIN_UI_SCK      6
    #define PIN_UI_MISO     7
    #define PIN_UI_CSN      15

    //* Pins for RF modems SPI
    #define PIN_RF_CSN      47
    #define PIN_RF_MOSI     17
    #define PIN_RF_MISO     16
    #define PIN_RF_SCK      18
    #define PIN_RF_IRQ      4
    #define PIN_RF_CE       7 //Patch such that LR1121 BUSY Pin is connected to R42.

    #define PIN_RF_LR1121_CSN 6
    #define PIN_RF_LR1121_BUSY 21

    //* Pin for RSSI sampling
    #define PIN_ADC_RSSI    9

    //* Pin for shift registers used by RF switches
    #define PIN_SR_CLK      10
    #define PIN_SR_DAT      11
    #define PIN_SR_STR      38

    //* Pins for GPS UART and PPS.
    #define PIN_GPS_PPS     48
    #define PIN_GPS_RX      12
    #define PIN_GPS_TX      13

    //* Pins for compass 
    #define PIN_COMPASS_SDA 1
    #define PIN_COMPASS_SCL 2

    //? XoXo Malther
    
#endif
