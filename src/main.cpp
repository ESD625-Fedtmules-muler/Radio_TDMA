#include <Arduino.h>
#include <TinyGPSPlus.h>
#include "ESP32-c3_pinout.h"
#include "main.h"


unsigned long interval_node_1 = 8000; // 15 sekunder
unsigned long interval_node_2 = 7000;  // 30 sekunder

unsigned long last_time_node_1 = 0;
unsigned long last_time_node_2 = 0;


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
            //block.print_payload();
        }
    }
    
}


void setup() {
    Serial.begin(115200);
    delay(1000);
    Serial.println("Serial initialized");

    TDMA_setup(NODE_ID);
    setup_modem(RF24_PA_MAX);
    GPS_setup();
    table_setup();
    switch_setup();

    xTaskCreate(RX_interface, "RX_interface", 2048, NULL, 2, NULL);
    while (network_params.ready != true)
    {
        delay(10);
    }
    

}

void loop() {
    unsigned long currentMillis = millis();

    // --- TRACKER NODE 1 ---
    if (currentMillis - last_time_node_1 >= interval_node_1) {
        last_time_node_1 = currentMillis;

        look_up[TRACK_NODE_ID_1].latitude = (float)random(566000, 578001) / 10000.0;
        look_up[TRACK_NODE_ID_1].longitude = (float)random(820000, 1100001) / 100000.0;
        look_up[TRACK_NODE_ID_1].hasUpdate = true;

        Serial.println("Node 1 lokation opdateret");
        block_item roblocks;
        *(uint32_t*)&roblocks.block_payload[0] = currentMillis;
        xQueueSend(tx_blockqueue, &roblocks, 0);
    }

    // --- TRACKER NODE 2 ---
    if (currentMillis - last_time_node_2 >= interval_node_2) {
        last_time_node_2 = currentMillis;

        look_up[TRACK_NODE_ID_2].latitude = (float)random(566000, 578001) / 10000.0;
        look_up[TRACK_NODE_ID_2].longitude = (float)random(820000, 1100001) / 100000.0;
        look_up[TRACK_NODE_ID_2].hasUpdate = true;

        Serial.println("Node 2 lokation opdateret");

        // Send til TX køen
        block_item roblocks;
        *(uint32_t*)&roblocks.block_payload[0] = currentMillis;
        xQueueSend(tx_blockqueue, &roblocks, 0);
    }

    // Ingen delay() her! loopet kører bare lynhurtigt igennem og tjekker uret.
}