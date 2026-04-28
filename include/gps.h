#pragma once
#include <Arduino.h>
#include <switches.h>
#include <Wire.h>
#include <network_params.h>
#include <TinyGPSPlus.h>



#define GPS_PAKKE_SIZE 256
extern uint8_t GPS_buffer[GPS_PAKKE_SIZE];
extern size_t GPS_pakke_length;
extern volatile bool GPS_pakke_status;

extern TinyGPSPlus gps;

//extern AntennaDir switch_states[MAX_NODES]; // Skal være look up for hvordan switch skal stå for hver nodeID. Regnes i GPS.cpp


struct TrackerNode {
    int nodeID;      // ID i look_up table
    int i2cAddress;  // I2C adresse på stepper-controlleren
};

struct __attribute__((packed)) Look_up_entry{ //Gør lige så den ik fylder så meget
    uint8_t ID;
    float latitude;
    float longitude;
    //int32_t rssi;
    //bool hasUpdate;
    int8_t lifetime;

    void debug_msg();
};

struct Channel_state_table {
    Look_up_entry entries[MAX_NODES];
    SemaphoreHandle_t table_mutex;

    AntennaDir switch_states[MAX_NODES];
    float P_Signal[MAX_NODES];
    float P_Channel[MAX_NODES];
    float Dist[MAX_NODES];
    float Heading[MAX_NODES];
    //Updates or creates entry with given ID. Lifetime is reset to 0, and hasUpdate is set to true.
    void update_entry(size_t ID, float lat, float lng);
    
    void begin();

    //Serializes the table into a byte buffer. Returns the length of the serialized data.
    uint16_t serialize(uint8_t* buffer, size_t buffer_size);

    uint8_t get_known_positions();

     //Deserializes the table from a byte buffer. Returns true if successful.
    bool deserialize(const uint8_t* buffer, size_t buffer_size);

    bool evaluate_packet(uint8_t *data, size_t len, uint8_t src);

    bool update_life_times(int8_t delta_time);

    Look_up_entry get_entry(uint8_t node_id);

    void printLookUpTable(); 

     //Updates the lifetime of all entries by adding delta_time (in seconds). If lifetime exceeds timeout, it is set to -1 (invalid).
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