#include <SPI.h>
#include <Arduino.h>
#include <ESP32-c3_pinout.h>
#include <main.h>
#include <TinyGPSPlus.h>

HardwareSerial gpsSerial(1); // Use UART1 for GPS communication
TinyGPSPlus gps;
GPSData currentGPS = {0.0, 0.0, false};

void Task_GPS(void *pvParameter);

void GPS_setup() {
    
    // Det reset skal sendes til nogle af dem, for at sikre at GPS'en sender mere end bare GPGLL (For at tinyGPS virker)
    byte resetConfig[] = {0xB5, 0x62, 0x06, 0x09, 0x0D, 0x00, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0x00, 0x00, 0x01, 0x19, 0x98};
    gpsSerial.begin(9600, SERIAL_8N1, PIN_GPS_RX, PIN_GPS_TX);
    delay(100);
    gpsSerial.write(resetConfig, sizeof(resetConfig));

    Serial.println("GPS serial initialized");

    xTaskCreatePinnedToCore(
    Task_GPS,        // Task function
    "GPS_Data",     // Name
    4096,             // Stack size
    NULL,             // Parameters
    5,                // Priority
    NULL,             // Task handle
    0                 // Core ID (0 or 1)
    );
};


void Task_GPS(void *pvParameter) {
    char c;
    for(;;) {
        while (gpsSerial.available() > 0) {
            c = gpsSerial.read();
            if (gps.encode(c)) { 
                if (gps.location.isUpdated()) {
                    currentGPS.latitude = gps.location.lat();
                    currentGPS.longitude = gps.location.lng();
                    currentGPS.hasUpdate = true;
                }
            }
        }
        vTaskDelay(pdMS_TO_TICKS(100)); 
    }
}