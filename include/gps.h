#pragma once
#include <Arduino.h>
#include <switches.h>

#define GPS_PAKKE_SIZE 256
extern uint8_t GPS_buffer[GPS_PAKKE_SIZE];
extern size_t GPS_pakke_length;
extern volatile bool GPS_pakke_status;


extern AntennaDir switch_states[MAX_NODES]; // Skal være look up for hvordan switch skal stå for hver nodeID. Regnes i GPS.cpp


struct __attribute__((packed)) Look_up_entry{ //Gør lige så den ik fylder så meget
    uint8_t ID;
    float latitude;
    float longitude;
    int32_t rssi;
    bool hasUpdate;
    int8_t lifetime;

    void debug_msg();
};

void GPS_setup();




