#include <Arduino.h>
#include <TinyGPSPlus.h>
#include "ESP32-c3_pinout.h"
#include "main.h"

void RX_interface(void *pvParameters){

    while (network_params.ready != true)
    {
        delay(100);
    }

    QueueHandle_t rx_queue = router_setup_listener(0x11, 10);

    while (true)
    {   
        network_package block;
        if(xQueueReceive(rx_queue, &block, portMAX_DELAY) == pdTRUE){
            block.debug_msg();
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

    //xTaskCreate(RX_interface, "RX_interface", 4096, NULL, 2, NULL);
    Serial.println("All good");

    while (network_params.ready != true)
    {
        delay(10);
    }
}


void loop() {

    if (GPS_pakkeReady) {
        Serial.println("Ny gps pakke!");
        decodeAndPrintGPSBuffer(GPS_buffer, GPS_pakke_length);
    }
    //router_send_data(0x69, 0x42, (uint8_t*)buf, sizeof(buf));
    //Serial.print("Sending packet");
    delay(6000);
}

