#pragma once
#include "packet.h"
#include "network_params.h"

class network_package {
public:
    uint16_t source_UID;
    uint8_t  nonce;
    uint16_t dest_UID;
    MSG_type_t msg_type = P2P;
    uint8_t  hops = 0;
    uint8_t  hop_list[16] = {0};
    payload_buffer_t payload;

    bool parse_header(Package_queue_item* original_buffer);
    bool parse_payload(Package_queue_item* original_buffer);
    bool serialize(Package_queue_item* dest);
    bool add_hop(uint8_t uid);
    void debug_msg();
};

void setup_router();


/// @brief Sends a network package through the whole stack
/// @param dest_UID Destiation UID, beware 0XFFFF is not allowed and the GPS ip should not be used as well
/// @param src_UID The marked sender of the packet
/// @param buffer the data supposed to be sendt
/// @param length The length of the buffer being sendt
/// @param routing_type MSG_type_t can be 1) 'Near_cast' Where it is never forwarded, 2) 'P2P' where everyone forwards except the reciepient 3) 'Broadcast' Where intended recipient also broadcasts
void router_send_data(uint16_t dest_UID, uint16_t src_UID, uint8_t* buffer, size_t length, MSG_type_t routing_type=P2P);


bool router_remove_listener(uint16_t UID);
QueueHandle_t router_setup_listener(uint16_t UID, uint16_t queuesize);