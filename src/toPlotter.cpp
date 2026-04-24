
#include <Arduino.h>
#include <TinyGPSPlus.h>
#include "ESP32-c3_pinout.h"
#include "main.h"


void sendLookUpToSerial(void) {
    for (int i = 0; i < network_params.number_of_nodes; i++) {
        if (look_up[i].hasUpdate) {
            Serial.printf("NODE,%d,%.6f,%.6f\n",
                      i,
                      look_up[i].latitude,
                      look_up[i].longitude);
        }
    }
}