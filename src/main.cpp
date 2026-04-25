#include <Arduino.h>
#include <TinyGPSPlus.h>
#include "pins.h"
#include "main.h"




void RX_interface(void *pvParameters){

    while (network_params.ready != true)
    {
        delay(100);
    }
    

    while (true)
    {   
        delay(1000);
    }
}






void setup() {
    Serial.begin(115200);
#ifndef DUMMY_RADIO
    //pinMode(PIN_RF_LR1121_CSN, OUTPUT);
    //digitalWrite(PIN_RF_LR1121_CSN, HIGH);
#endif
    TDMA_setup(NODE_ID);
    setup_chopper();
    setup_router();
    GPS_setup();

    Serial.printf("CPU speed: %dMHz", ESP.getCpuFreqMHz());
    xTaskCreatePinnedToCore(
    RX_interface,        // Task function
    "RX_interface",     // Name
    4096,             // Stack size
    NULL,             // Parameters
    3,                // Priority
    NULL,             // Task handle
    1                 // Core ID (0 or 1)
    );
    Serial.println(network_params.channel);
    Serial.println("All good");

    while (network_params.ready != true)
    {
        delay(10);
    }

    

}


void loop() {

    char buf[] = "Axel siger hej.";

    
    //router_send_data(0x10, 0x20, (uint8_t*)buf, sizeof(buf));
    delay(6000);
}





