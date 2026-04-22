#include <SPI.h>
#include <Arduino.h>
#include <ESP32-c3_pinout.h>
#include <main.h>
#include <TinyGPSPlus.h>

HardwareSerial gpsSerial(1); // Use UART1 for GPS communication
TinyGPSPlus gps;

#if NODE_ID == 1
    GPSData myGPS = { 57.013928, 9.987330, false};
#endif

#if NODE_ID != 1   
    GPSData myGPS = {0.0, 0.0, false};
#endif

uint8_t GPS_buffer[GPS_PAKKE_SIZE];
size_t GPS_pakke_length = 0;
volatile bool GPS_pakke_status = false;

void Task_GPS(void *pvParameter);
void task_GPS_Packer(void *pvparameter);

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

    xTaskCreatePinnedToCore(
        task_GPS_Packer,        // Task function
        "GPS_pakker",     // Name
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
            //Serial.print(c);
            if (gps.encode(c)) { 
                if (gps.location.isUpdated()) {
                    myGPS.latitude = gps.location.lat();
                    myGPS.longitude = gps.location.lng();
                    myGPS.hasUpdate = true;
                }
            }
        }
        vTaskDelay(pdMS_TO_TICKS(200)); 
    }
}


void task_GPS_Packer(void *pvparameter) {
    GPS_pakker gps_pakke;
    for (;;) {

        if (GPS_pakke_status == true) {
            //Serial.print("Venter på at sende en GPS pakke");
            vTaskDelay(pdMS_TO_TICKS(5));
            continue;
        }
        
        size_t offset = 0;

        //Tjekker om vores egne koordinater er blevet opdateret.
        if (myGPS.hasUpdate) {
            gps_pakke.type = 1;
            gps_pakke.node_id = NODE_ID;
            gps_pakke.latitude = myGPS.latitude;
            gps_pakke.longitude = myGPS.longitude;
            //gps_pakke.RSSI = look_up[NODE_ID];
            memcpy(GPS_buffer + offset, &gps_pakke, sizeof(gps_pakke));
            offset += sizeof(gps_pakke);
            myGPS.hasUpdate = false;
        }

            //Tjekker om nogle af de andre noder er blevet opdateret
/*         for (int i = 0; i < network_params.number_of_nodes; i++) {

            if (i == NODE_ID){
                continue;
            }
            if (look_up[i].hasUpdate){
                gps_pakke.type = 2;
                gps_pakke.node_id = i;
                gps_pakke.latitude = look_up[i].latitude;
                gps_pakke.longitude = look_up[i].longitude;
                gps_pakke.rssi = look_up[i].rssi;

                if (offset + sizeof(gps_pakke) > GPS_PAKKE_SIZE) {
                    break;
                }
                memcpy(GPS_buffer + offset, &gps_pakke, sizeof(gps_pakke));
                offset += sizeof(gps_pakke);
                look_up[i].hasUpdate = false;
            }
        } */
        GPS_pakke_length = offset;
        GPS_pakke_status = (offset > 0);

        vTaskDelay(pdMS_TO_TICKS(100));
    }

}  


void decodeAndPrintGPSBuffer(const uint8_t* buffer, size_t length) { //Chatten har lige gjort det her.

    Serial.println("=== Decode GPS Buffer ===");

    size_t offset = 0;

    while (offset + sizeof(GPS_pakker) <= length) {

        const GPS_pakker* pkt = (const GPS_pakker*)(buffer + offset);

        Serial.println("---- GPS Pakke ----");
        Serial.print("Type: "); Serial.println(pkt->type);
        Serial.print("Node ID: "); Serial.println(pkt->node_id);
        Serial.print("Latitude: "); Serial.println(pkt->latitude, 6);
        Serial.print("Longitude: "); Serial.println(pkt->longitude, 6);
        Serial.print("RSSI: "); Serial.println(pkt->RSSI);
        Serial.println("-------------------");

        offset += sizeof(GPS_pakker);
    }

    Serial.println("=== Slut ===");
}