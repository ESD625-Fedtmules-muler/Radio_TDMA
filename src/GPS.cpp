#include <SPI.h>
#include <Arduino.h>
#include <pins.h>
#include <network_params.h>
#include <TinyGPSPlus.h>
#include <gps.h>
#include <router.h>



HardwareSerial gpsSerial(1); // Use UART1 for GPS communication
TinyGPSPlus gps;


struct GPS_state
{
    float Lat = 0;
    float Lng = 0;
};

GPS_state pos;
bool GPS_ok = false;



void Task_GPS_rx(void *pvParameter) {
    char c;

    Serial.println("GPS_Rx task running");
    for(;;) {
        
        while (gpsSerial.available() > 0) {
            c = gpsSerial.read();
            //Serial.print(c);
            if (gps.encode(c)) { 
                if (gps.location.isUpdated()) {
                    GPS_ok = true;
                    pos.Lat = gps.location.lat();
                    pos.Lng = gps.location.lng();
                }
            }
        }
        
    }
}


/// @brief Task in charge of once every second updating lifetimes, and updating most adequate switches.
/// @param pvParameter 
void task_GPS_runner(void *pvparameter) {
    uint32_t period = 1000; //tid i millisekunder
    uint32_t periods_between_beacons = 2;
    while (!GPS_ok)
    {
        delay(50);
    }
    
    
    TickType_t xLastWakeTime = xTaskGetTickCount();
    int i = 0;
    
    for (;;) {
        Serial.print("LAT=");
        Serial.print(pos.Lat);
        Serial.print("\tLNG=");
        Serial.print(pos.Lng);
        Serial.println();
        
        vTaskDelayUntil( &xLastWakeTime, pdMS_TO_TICKS(period));
    }

}  






void GPS_setup() {

    Serial.println("GPS setup");

    byte resetConfig[] = {0xB5, 0x62, 0x06, 0x09, 0x0D, 0x00, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0x00, 0x00, 0x01, 0x19, 0x98};
    
    //gpsSerial.begin(9600, SERIAL_8N1, PIN_GPS_TX, PIN_GPS_RX);
    //delay(100);
    //Serial.println("Writing reset conf");


    //gpsSerial.write(resetConfig, sizeof(resetConfig));
    xTaskCreatePinnedToCore(
        Task_GPS_rx,        // Task function
        "GPS_rx",     // Name
        4096,             // Stack size
        NULL,             // Parameters
        5,                // Priority
        NULL,             // Task handle
        1                 // Core ID (0 or 1)
    );
    xTaskCreatePinnedToCore(
        task_GPS_runner,        // Task function
        "GPS_runner",     // Name
        4096,             // Stack size
        NULL,             // Parameters
        5,                // Priority
        NULL,             // Task handle
        1                 // Core ID (0 or 1)
    );

    Serial.println("GPS serial initialized");

};
