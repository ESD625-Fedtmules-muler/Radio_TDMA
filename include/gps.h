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

/// @brief Calculates the distance between two geographic coordinates
/// @param lat1 Latitude of the first point (in degrees)
/// @param lon1 Longitude of the first point (in degrees)
/// @param lat2 Latitude of the second point (in degrees)
/// @param lon2 Longitude of the second point (in degrees)
/// @return Distance between the two points (typically in kilometers or meters depending on implementation)
float calculate_distance(float lat1, float lon1, float lat2, float lon2);


/// @brief Calculates the bearing (direction) from one geographic point to another
/// @param lat1 Latitude of the starting point (in degrees)
/// @param lon1 Longitude of the starting point (in degrees)
/// @param lat2 Latitude of the destination point (in degrees)
/// @param lon2 Longitude of the destination point (in degrees)
/// @return Bearing in degrees (0° = North, clockwise)
float calculate_bearing(float lat1, float lon1, float lat2, float lon2);