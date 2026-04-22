#include <Arduino.h>
#include <TinyGPSPlus.h>
#include "ESP32-c3_pinout.h"
#include "main.h"

HardwareSerial gpsSerial(1); // Use UART1 for GPS communication
TinyGPSPlus gps;


void RX_interface(void *pvParameters){

    while (network_params.ready != true)
    {
        delay(100);
    }

    QueueHandle_t rx_queue = router_setup_listener(0x69, 10);

    while (true)
    {   
        network_package block;
        if(xQueueReceive(rx_queue, &block, portMAX_DELAY) == pdTRUE){
            Serial.print("GOT SOMETHING:");
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

    xTaskCreate(RX_interface, "RX_interface", 4096, NULL, 2, NULL);
    Serial.println("All good");

    while (network_params.ready != true)
    {
        delay(10);
    }
}


void loop() {
    char buf[] = "INSHALLA OM GUD VIL DET 12aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa3456789 123456789 123456789 123456789 123456789 123456789 123456789 123456789 123456789 123456789 123456789 123456789 ";
    router_send_data(0x42, 0x69, (uint8_t*)buf, sizeof(buf));
    Serial.print("Sending packet");
    delay(6000);
}

