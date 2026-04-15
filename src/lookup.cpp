#include <Arduino.h>
#include <ESP32-c3_pinout.h>
#include <main.h>


Look_up look_up[MAX_NUMBER_OF_NODES];
SemaphoreHandle_t tableMutex; //Bruges ikke nu her, men tænkte vi skulle have mulighed for det

void table_setup() {
    tableMutex = xSemaphoreCreateMutex();
    
    for(int i = 0; i < network_params.number_of_nodes; i++) {
        look_up[i].latitude = 0;
        look_up[i].longitude = 0;
        look_up[i].rssi = -100;
        look_up[i].switchState = ERROR;
    }

    // Test koordinater til vilkårlige noder!
    look_up[2].latitude = 57.014860;
    look_up[2].longitude = 9.987288;
    look_up[3].latitude = 57.013959;
    look_up[3].longitude = 9.988031;
    look_up[4].latitude = 57.013689;
    look_up[4].longitude =  9.987972;
    look_up[5].latitude = 57.013488;
    look_up[5].longitude =  9.987175;
}

// Til at opdatere pos og RSSI
void update_node(int index, float lat, float lon, int rssi) {
    look_up[index].latitude = lat;
    look_up[index].longitude = lon;
    look_up[index].rssi = rssi;
}