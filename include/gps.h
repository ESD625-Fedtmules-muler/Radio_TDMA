#pragma once
#include <Arduino.h>

#define GPS_PAKKE_SIZE 256
extern uint8_t GPS_buffer[GPS_PAKKE_SIZE];
extern size_t GPS_pakke_length;
extern volatile bool GPS_pakke_status;





struct __attribute__((packed)) Look_up_entry{ //Gør lige så den ik fylder så meget
    uint8_t ID;
    float latitude;
    float longitude;
    int32_t rssi;
    bool hasUpdate;
    int8_t lifetime;
};



void GPS_setup();


