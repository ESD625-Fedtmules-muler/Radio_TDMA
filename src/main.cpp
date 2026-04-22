#include <Arduino.h>
#include <TinyGPSPlus.h>
#include "ESP32-c3_pinout.h"
#include "main.h"

unsigned long interval_node = 10000; // 15 sekunder
unsigned long last_time_node = 0;



void RX_interface(void *pvParameters){

    while (network_params.ready != true)
    {
        delay(100);
    }
    
    #if NODE_ID == 1
        QueueHandle_t rx_queue = router_setup_listener(0x10, 10);
    #endif 
    #if NODE_ID == 2
        QueueHandle_t rx_queue = router_setup_listener(0x20, 10);
    #endif
    while (true)
    {   
        network_package block;
        if(xQueueReceive(rx_queue, &block, portMAX_DELAY) == pdTRUE){
            //Serial.println("GOT Something");
            //block.debug_msg();
            //decodeAndPrintGPSBuffer(block.payload.data, block.payload.len);
            update_LookUp(block.payload.data, block.payload.len);
        }
    }
}






void setup() {
    Serial.begin(115200);

    setup_modem(RF24_PA_MAX);
    TDMA_setup(NODE_ID);
    setup_chopper();
    setup_router();
    GPS_setup();

    xTaskCreate(RX_interface, "RX_interface", 4096, NULL, 2, NULL);
    Serial.println("All good");

    while (network_params.ready != true)
    {
        delay(10);
    }
}


void loop() {

    unsigned long currentMillis = millis();

    // --- Til at opdatere en nodes position en gang i mellem ---
    if (currentMillis - last_time_node >= interval_node) {
        last_time_node = currentMillis;
        uint8_t currentNode = (int)random(0, 9);
        look_up[currentNode].latitude = (float)random(566000, 578001) / 10000.0;
        look_up[currentNode].longitude = (float)random(820000, 1100001) / 100000.0;
        look_up[currentNode].hasUpdate = true;
        printLookUpTable();
    }

    if (GPS_pakke_status) {
        //Serial.println("Ny gps pakke!");
        //decodeAndPrintGPSBuffer(GPS_buffer, GPS_pakke_length);

        #if NODE_ID == 1
            router_send_data(0x20, 0x10, (uint8_t*)GPS_buffer, GPS_pakke_length);
            GPS_pakke_status = false;
            //Serial.print("Sender det her: ");
            //decodeAndPrintGPSBuffer(GPS_buffer, GPS_pakke_length);
            //Serial.print("Sending packet");

        #endif
        #if NODE_ID == 2
            router_send_data(0x10, 0x20, (uint8_t*)GPS_buffer, GPS_pakke_length);
            GPS_pakke_status = false;

        #endif
    
    }

    delay(6000);
}

