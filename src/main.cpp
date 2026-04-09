#include <Arduino.h>
#include <TinyGPSPlus.h>
#include "ESP32-c3_pinout.h"
#include "main.h"

HardwareSerial gpsSerial(1); // Use UART1 for GPS communication
TinyGPSPlus gps;

void setup() {
    Serial.begin(115200);
    delay(1000);
    Serial.println("Serial initialized");
    gpsSerial.begin(9600); // RX, TX pins for GPS
    Serial.println("GPS serial initialized");

    TDMA_setup(0);
    setup_modem(RF24_PA_MAX);
    //timerRestart(timer_25ms);
    //timerAlarmEnable(timer_25ms);


}

void loop() {
    //vTaskDelete(NULL);
}

