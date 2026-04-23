#include <Arduino.h>
#include <TinyGPSPlus.h>
#include "ESP32-c3_pinout.h"
#include "main.h"

unsigned long interval_node = 10000; // 15 sekunder
unsigned long last_time_node = 0;


void task_debug_serial(void *pvParameters) {
    while (true) {
        sendLookUpToSerial();
        vTaskDelay(pdMS_TO_TICKS(1000)); // 2 Hz
    }
} 


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
            Serial.println("GOT Something");
            block.debug_msg();
            //decodeAndPrintGPSBuffer(block.payload.data, block.payload.len);
            //update_LookUp(block.payload.data, block.payload.len);
        }
    }
}






void setup() {
    Serial.begin(115200);
    pinMode(PIN_RF_LR1121_CSN, OUTPUT);
    digitalWrite(PIN_RF_LR1121_CSN, HIGH);
    
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

    //xTaskCreate(task_debug_serial, "Python_Interface", 4096, NULL, 2, NULL);

}


void loop() {

    char buf[] = "Axel siger hej.";
    router_send_data(0x20, 0x10, (uint8_t*)buf, sizeof(buf));

    delay(6000);
}





/* 
    unsigned long currentMillis = millis();

    // --- Til at opdatere en nodes position en gang i mellem ---
    if (currentMillis - last_time_node >= interval_node) {
        last_time_node = currentMillis;
        printLookUpTable();
    }



    if (GPS_pakke_status) {
        
        #if NODE_ID == 1
            router_send_data(0x20, 0x10, (uint8_t*)GPS_buffer, GPS_pakke_length);
            GPS_pakke_status = false;
        #endif
        #if NODE_ID == 2
            router_send_data(0x10, 0x20, (uint8_t*)GPS_buffer, GPS_pakke_length);
            GPS_pakke_status = false;
        #endif
    
    } */