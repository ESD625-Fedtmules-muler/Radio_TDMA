#include <Arduino.h>
#include <TinyGPSPlus.h>
#include "ESP32-c3_pinout.h"
#include "main.h"

void RX_interface(void *pvParameters){

    while (network_params.ready != true)
    {
        delay(10);
    }

    while (true)
    {   
        block_item block;
        if(xQueueReceive(rx_blockqueue, &block, portMAX_DELAY) == pdTRUE){
            Serial.print("GOT SOMETHING:");
            block.print_payload();
        }
    }
    
}

void setup() {
    Serial.begin(115200);
    delay(1000);
    Serial.println("Serial initialized");


    TDMA_setup(NODE_ID);
    GPS_setup();
    table_setup();
    switch_setup();
    setup_modem(RF24_PA_MAX);
    xTaskCreate(RX_interface, "RX_interface", 2048, NULL, 2, NULL);
    while (network_params.ready != true)
    {
        delay(10);
    }
    
    
}

void loop() {
    block_item roblocks;
    *(uint32_t*)&roblocks.block_payload[0] = millis();
    Serial.print("Queueing TX");
    for (size_t i = 0; i < 1; i++)
    {
        xQueueSend(tx_blockqueue, &roblocks, 0);
    }
    
    delay(5000);
}

