#include <SPI.h>
#include <Arduino.h>
#include <ESP32-c3_pinout.h>
#include <main.h>
#include <TinyGPSPlus.h>

HardwareSerial gpsSerial(1); // Use UART1 for GPS communication
TinyGPSPlus gps;
GPSData currentGPS = {0.0, 0.0, "", false};

void Task_GPS(void *pvParameter);

void GPS_setup() {
    //gpsSerial.begin(9600); // RX, TX pins for GPS
    gpsSerial.begin(9600, SERIAL_8N1, PIN_GPS_RX, PIN_GPS_TX);
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
    String lineBuffer = "";
    
    for(;;) {
        while (gpsSerial.available() > 0) {
            char c = gpsSerial.read();
            lineBuffer += c;

            if (c == '\n') {
                lineBuffer.trim();

                if (lineBuffer.startsWith("$GPGLL")) {
                    int firstComma = lineBuffer.indexOf(',');
                    int secondComma = lineBuffer.indexOf(',', firstComma + 1);
                    int thirdComma = lineBuffer.indexOf(',', secondComma + 1);
                    int fourthComma = lineBuffer.indexOf(',', thirdComma + 1);

                    if (secondComma > firstComma && fourthComma > thirdComma) {
                        String latRaw = lineBuffer.substring(firstComma + 1, secondComma);
                        String lngRaw = lineBuffer.substring(thirdComma + 1, fourthComma);
                        
                        // Gem data i den globale struktur
                        currentGPS.latitude = latRaw.substring(0, 2).toFloat() + (latRaw.substring(2).toFloat() / 60.0);
                        currentGPS.longitude = lngRaw.substring(0, 3).toFloat() + (lngRaw.substring(3).toFloat() / 60.0);
                        currentGPS.rawSentence = lineBuffer;
                        currentGPS.hasUpdate = true;
                    }
                }
                lineBuffer = ""; 
            }
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}