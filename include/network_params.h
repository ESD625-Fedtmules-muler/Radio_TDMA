
#pragma once
#include <RF24.h>

#define MAX_NODES 5

enum MSG_type_t { Near_cast = 0, Broadcast = 1, P2P = 2 };

struct Network_params {
    uint8_t TTL = 6;
    uint8_t channel = 85;
    rf24_datarate_e bitRate = RF24_1MBPS;
    uint8_t number_of_nodes = MAX_NODES;
    uint8_t node_id = 0;
    uint64_t pipe_name = 0x0000000042;
    uint16_t GPS_IP = 0x20;
    uint8_t pos_timeout = 60;
    bool ready = false;
    uint32_t min_dist = 10;
};
extern Network_params network_params;
extern RF24 radio;

void sendLookUpToSerial(void);
void printLookUpTable();