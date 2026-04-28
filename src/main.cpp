#include <Arduino.h>
#include "pins.h"
#include "main.h"


extern Channel_state_table channel_state_table;


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



    for (size_t i = 0; i < network_params.number_of_nodes; i++) //Housekeeping beskeder.
    {
        Look_up_entry entry = channel_state_table.get_entry(i);
        char buf[256] = {0};
        size_t offset = 0;
        buf[0] = 42; //42 BETYDER LOOKUP ENTRY
        buf[1] = 0; //LÆngden af svinet


        offset += 2;
        memcpy(&buf[offset], &entry, sizeof(entry)); //Kopirerer entry
        offset += sizeof(entry);

        memcpy(&buf[offset], &channel_state_table.P_Signal[i], sizeof(channel_state_table.P_Signal[i])); //Kopierer tilsvarende P_signal
        offset += sizeof(channel_state_table.P_Signal[i]);

        memcpy(&buf[offset], &channel_state_table.P_Channel[i], sizeof(channel_state_table.P_Channel[i])); //Kopirerer tilsvarende P_Channel
        offset += sizeof(channel_state_table.P_Channel[i]);

        memcpy(&buf[offset], &channel_state_table.switch_states[i], sizeof(channel_state_table.switch_states[i])); //Kopirerer hvad for en switch state vi er i
        offset += sizeof(channel_state_table.switch_states[i]);

        memcpy(&buf[offset], &channel_state_table.Dist[i], sizeof(channel_state_table.Dist[i])); //Kopirerer hvad for en switch state vi er i
        offset += sizeof(channel_state_table.Dist[i]);
        buf[1] = offset; //Til sidst fortæl hvor mange bytes vi har tænkt os at sende


        memcpy(&buf[offset], &channel_state_table.Heading[i], sizeof(channel_state_table.Heading[i])); //
        offset += sizeof(channel_state_table.Heading[i]);
        buf[1] = offset; 

        for (size_t k = 0; k < offset; k++)
        {
            Serial.write((uint8_t)buf[k]);
        }
    }
    delay(100000);

}





