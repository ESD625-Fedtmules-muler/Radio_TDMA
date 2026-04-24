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
    pinMode(PIN_RF_LR1121_CSN, OUTPUT);
    digitalWrite(PIN_RF_LR1121_CSN, HIGH);
#endif
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

    char buf[] = "Axel siger hej.";

    
    //router_send_data(0x10, 0x20, (uint8_t*)buf, sizeof(buf));
    delay(6000);
}





