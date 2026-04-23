#pragma once
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>

#define RX_queue_size 100
#define TX_queue_size 100

struct block_item {
    uint8_t block_payload[32] = {0};
    int8_t ID = 0;
    void print_payload();
};

struct Package_queue_item {
    uint8_t data[1024] = {0};
    uint16_t length = 0;
    void debug_msg();
};

typedef struct {
    uint16_t len;
    uint8_t data[512];
} payload_buffer_t;

extern QueueHandle_t rx_package_queue;
extern QueueHandle_t tx_package_queue;
extern QueueHandle_t tx_blockqueue;

void setup_chopper();